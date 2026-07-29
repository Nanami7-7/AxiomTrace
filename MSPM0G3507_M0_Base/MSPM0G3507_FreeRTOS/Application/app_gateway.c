/**
 * @file    app_gateway.c
 * @brief   Board B AB 板协议网关任务实现。
 *
 * 数据路径：
 *   UART2 BLE RX -> COBS 分帧 -> gateway_router_on_host_frame()
 *   UART1 Board A RX -> COBS 分帧 -> gateway_router_on_board_frame()
 *
 * 本任务独占 UART2 BLE 接收缓冲。不要同时启动 app_dx_ble_service_task，
 * 否则两个任务会竞争同一 UART2 接收队列。
 */
#include "app_gateway.h"
#include "gateway_router.h"
#include "proto_stream.h"
#include "proto_uart1_b.h"
#include "bsp_ble_uart.h"
#include "app_dx_ble_service.h"
#include "app_ble_debug_cli.h"
#include "bsp_debug.h"
#include "project_config.h"
#include "osal_api.h"
#include <stddef.h>
#include <stdio.h>

#define APP_GATEWAY_TASK_STACK_WORDS (768U)
#define APP_GATEWAY_TASK_PRIORITY    (3U)
#define APP_GATEWAY_TICK_MS          (2U)

static proto_stream_t s_host_stream;
static proto_stream_t s_board_stream;
static uint8_t s_host_decoded[PROTO_MAX_DECODED];
static uint8_t s_board_decoded[PROTO_MAX_DECODED];
static gateway_router_t s_router;
static gateway_router_adapter_t s_adapter;
static bool s_initialized;

/*
 * 启动配置会连续执行多条 AT 命令，这些结果缓冲区放在静态区，
 * 不占用 gateway 任务栈，避免 vApplicationStackOverflowHook。
 */
static app_dx_ble_command_result_t s_ble_result;
static app_dx_ble_command_result_t s_ble_role_result;
static app_dx_ble_command_result_t s_ble_name_result;
static app_dx_ble_command_result_t s_ble_mac_result;

static uint32_t gateway_now_ms(void)
{
    return osal_ticks_to_ms(osal_get_tick_count());
}

static bool gateway_tx_board(const uint8_t *wire, size_t len)
{
    if (wire == NULL || len > UINT16_MAX) {
        return false;
    }
    return proto_uart1_b_write(wire, (uint16_t)len) == BSP_OK;
}

static bool gateway_tx_ble(const uint8_t *wire, size_t len)
{
    if (wire == NULL || len > UINT16_MAX) {
        return false;
    }
    return bsp_ble_uart_write(wire, (uint16_t)len) == BSP_OK;
}

/**
 * @brief 启动时检查 BLE 模式，并在从机模式下设置名称。
 *
 * DX-BT311 的 AT+NAME 只在从机模式有效。
 * 当前工程要求手机/上位机连接 Board B，因此 BLE 必须是从机（ROLE=0）。
 * 如果模块实际为主机（ROLE=1），这里只打印提示，不自动切换，避免模块
 * 自动重启导致调试过程变得不可控。可先用串口工具发送 AT+ROLE0。
 */
static void gateway_try_set_ble_name(void)
{
#if (PRJ_BLE_AUTO_SET_NAME != 0U) && \
    (PRJ_BLE_ROLE == PRJ_BLE_ROLE_SLAVE)
    app_dx_ble_command_result_t *result = &s_ble_result;
    app_dx_ble_command_result_t *role_result = &s_ble_role_result;
    app_dx_ble_command_result_t *current_name_result = &s_ble_name_result;
    app_dx_ble_command_result_t *mac_result = &s_ble_mac_result;
    dx_bt311_status_t init_status;
    dx_bt311_status_t role_status;
    dx_bt311_status_t current_name_status;
    dx_bt311_status_t name_status;
    dx_bt311_status_t mac_status;

    init_status = app_dx_ble_service_init();
    if (init_status != DX_BT311_OK) {
        printf("[BLE] AT 初始化失败 status=%d\r\n", (int)init_status);
        return;
    }

    /* 必须先查询 ROLE，再访问从机专用的 NAME 命令。 */
    role_status = app_dx_ble_execute_command(
        DX_BT311_CMD_ROLE,
        role_result,
        PRJ_BLE_NAME_CONFIG_TIMEOUT_MS);
    printf("[BLE] 当前角色 status=%d value=%s raw=%s\r\n",
           (int)role_status,
           (role_result->value[0] != '\0') ?
           role_result->value : "<无解析值>",
           (role_result->raw_response[0] != '\0') ?
           role_result->raw_response : "<无响应>");

    if ((role_status != DX_BT311_OK) ||
        (role_result->value[0] == '\0')) {
        printf("[BLE] 无法确认角色，跳过名称配置\r\n");
    } else if (role_result->value[0] != '0') {
        printf("[BLE] 当前为主机模式，AT+NAME 只在从机模式有效，跳过改名\r\n");
        printf("[BLE] 请先发送 AT+ROLE0，模块会自动重启；重启后再测试\r\n");
    } else {
        /* ROLE=0：从机模式，NAME 命令现在才允许执行。 */
        current_name_status = app_dx_ble_execute_command(
            DX_BT311_CMD_NAME,
            current_name_result,
            PRJ_BLE_NAME_CONFIG_TIMEOUT_MS);
        printf("[BLE] 当前名称 status=%d value=%s raw=%s\r\n",
               (int)current_name_status,
               (current_name_result->value[0] != '\0') ?
               current_name_result->value : "<无解析值>",
               (current_name_result->raw_response[0] != '\0') ?
               current_name_result->raw_response : "<无响应>");

        name_status = app_dx_ble_set_command(
            DX_BT311_CMD_NAME,
            PRJ_BLE_SLAVE_NAME,
            result,
            PRJ_BLE_NAME_CONFIG_TIMEOUT_MS);
        printf("[BLE] 设置名称=%s status=%d response=%s\r\n",
               PRJ_BLE_SLAVE_NAME,
               (int)name_status,
               (result->raw_response[0] != '\0') ?
               result->raw_response : "<无响应>");
        printf("[BLE] 注意：名称设置成功后需要重启模块才会生效\r\n");
    }

    /* 基础命令在主从模式都有效，始终查询并打印 MAC。 */
    mac_status = app_dx_ble_execute_command(
        DX_BT311_CMD_LADDR,
        mac_result,
        PRJ_BLE_NAME_CONFIG_TIMEOUT_MS);
    printf("[BLE] MAC 查询 status=%d value=%s raw=%s\r\n",
           (int)mac_status,
           (mac_result->value[0] != '\0') ?
           mac_result->value : "<无解析值>",
           (mac_result->raw_response[0] != '\0') ?
           mac_result->raw_response : "<无响应>");
#else
    /* 当前配置不是 BLE 从机，或已关闭启动时改名。 */
#endif
}
static void gateway_enter_critical(void)
{
    osal_critical_enter();
}

static void gateway_exit_critical(void)
{
    osal_critical_exit();
}

static void gateway_process_host_rx(void)
{
    uint8_t byte;
    size_t decoded_len;

    while (bsp_ble_uart_getc(&byte) == BSP_OK) {
        proto_stream_result_t result = proto_stream_feed(
            &s_host_stream, byte, s_host_decoded, sizeof(s_host_decoded),
            &decoded_len);
        if (result == PROTO_STREAM_FRAME) {
            gateway_router_on_host_frame(&s_router, s_host_decoded, decoded_len);
        }
    }
}

static void gateway_print_protocol_diag(uint32_t now_ms)
{
    static uint32_t last_print_ms;

    if ((uint32_t)(now_ms - last_print_ms) < 1000U) {
        return;
    }
    last_print_ms = now_ms;

    /* 诊断只输出到 UART0，绝不污染 UART1 的 COBS 二进制链路。 */
    printf("[AB] host=%lu board=%lu malformed=%lu unmatched=%lu enum=%lu tx_fail=%lu\r\n",
           (unsigned long)s_router.host_frames_received,
           (unsigned long)s_router.board_frames_received,
           (unsigned long)s_router.malformed_board_frame_count,
           (unsigned long)s_router.unmatched_response_count,
           (unsigned long)s_router.invalid_enum_count,
           (unsigned long)s_router.board_tx_fail_count);
    printf("[AB] host_stream cobs=%lu overflow=%lu empty=%lu | board_stream cobs=%lu overflow=%lu empty=%lu\r\n",
           (unsigned long)s_host_stream.cobs_error_count,
           (unsigned long)s_host_stream.overflow_count,
           (unsigned long)s_host_stream.empty_count,
           (unsigned long)s_board_stream.cobs_error_count,
           (unsigned long)s_board_stream.overflow_count,
           (unsigned long)s_board_stream.empty_count);
}

static void gateway_process_board_rx(void)
{
    uint8_t byte;
    size_t decoded_len;

    while (proto_uart1_b_getc(&byte) == BSP_OK) {
        proto_stream_result_t result = proto_stream_feed(
            &s_board_stream, byte, s_board_decoded, sizeof(s_board_decoded),
            &decoded_len);
        if (result == PROTO_STREAM_FRAME) {
            gateway_router_on_board_frame(&s_router, s_board_decoded, decoded_len);
        }
    }
}

int32_t app_gateway_init(void)
{
    if (s_initialized) {
        return 0;
    }

    /* UART0 仍然负责 printf，同时打开 UART0 接收，供 BLE 调试命令使用。 */
    if (bsp_debug_init() != BSP_OK) {
        return -1;
    }

    if (proto_uart1_b_init() != BSP_OK) {
        return -1;
    }
    if (bsp_ble_uart_init() != BSP_OK) {
        proto_uart1_b_deinit();
        return -2;
    }

    /* UART0 打印启动信息，确认 Board B 网关已经初始化。 */
    printf("[AB] Board B 网关启动，UART1=Board A，UART2=BLE\r\n");

    proto_stream_init(&s_host_stream);
    proto_stream_init(&s_board_stream);

    s_adapter.tx_board_link = gateway_tx_board;
    s_adapter.tx_ble = gateway_tx_ble;
    s_adapter.now_ms = gateway_now_ms;
    s_adapter.enter_critical = gateway_enter_critical;
    s_adapter.exit_critical = gateway_exit_critical;

    gateway_router_init(&s_router, &s_adapter);
    s_router.motor_count = 4U;


    if (osal_task_create(app_gateway_task,
                         "gateway",
                         APP_GATEWAY_TASK_STACK_WORDS,
                         NULL,
                         APP_GATEWAY_TASK_PRIORITY) == NULL) {
        bsp_ble_uart_deinit();
        proto_uart1_b_deinit();
        return -3;
    }

    s_initialized = true;
    return 0;
}

bool app_gateway_send_custom_command(uint8_t opcode,
                                      const uint8_t *payload,
                                      uint16_t payload_len)
{
    bool ok;

    if (!s_initialized) {
        return false;
    }

    ok = gateway_router_send_custom_command(&s_router, opcode,
                                            payload, payload_len);
    printf("[AB TEST] 本地Mode发送 opcode=0x%02X len=%u %s\r\n",
           (unsigned int)opcode,
           (unsigned int)payload_len,
           ok ? "已发往Board A" : "发送失败");
    return ok;
}
void app_gateway_task(void *param)
{
    (void)param;

    /*
     * 此处已经进入 FreeRTOS 任务上下文，可以安全使用任务延时。
     * BLE 名称设置和 MAC 查询会等待 UART2 响应，不能放在
     * app_gateway_init() 中，否则调度器尚未启动时可能卡住系统。
     */
    printf("[BLE] 开始设置名称并查询 MAC\r\n");
    gateway_try_set_ble_name();
    app_ble_debug_cli_init();
    printf("[AB] 网关任务已进入主循环\r\n");

    for (;;) {
        /* UART0 命令在本任务中执行，保证不会和 UART2 网关访问并发冲突。 */
        app_ble_debug_cli_process();
        gateway_process_host_rx();
        gateway_process_board_rx();
        uint32_t now_ms = gateway_now_ms();
        gateway_router_tick(&s_router, now_ms);
        gateway_print_protocol_diag(now_ms);
        osal_task_delay_ms(APP_GATEWAY_TICK_MS);
    }
}
