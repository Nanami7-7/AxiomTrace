/**
 * @file    app_dx_ble_service.c
 * @brief   DX-BT311 应用服务层与 UART2 BSP 的适配实现。
 * @details
 * 本文件负责组装 dx_bt311_transport_t 回调，将协议设备驱动连接到 UART2 板级
 * 服务，并对外提供应用层可直接使用的命令、设置、搜索、连接、透明数据和诊断
 * 接口。协议层不感知 FreeRTOS、DriverLib 或具体 UART 实例。
 *
 * 与 app_ble_service.c 共用同一 bsp_ble_uart 传输层，但使用独立的
 * dx_bt311_t 实例。两者不可同时初始化同一 UART2。
 */
#include "app_dx_ble_service.h"
#include "osal_api.h"
#include "project_config.h"
#include <stddef.h>
#include <stdio.h>
#include <string.h>

/** @brief 应用层唯一使用的 DX-BT311 协议实例。 */
static dx_bt311_t s_dx_bt311;

/** @brief UART2 BLE 服务是否已经完成初始化。 */
static bool s_initialized;
static bool s_config_applied;
static bool s_config_reboot_required;
static dx_bt311_status_t s_last_config_status = DX_BT311_ERR_NOT_INIT;
static dx_bt311_status_t s_last_connect_status = DX_BT311_ERR_NOT_INIT;

/**
 * @brief 将协议层发送回调适配到 UART2 BSP。
 */
static bool dx_ble_transport_write(void *context, const uint8_t *data,
                                   uint16_t len)
{
    (void)context;
    return (bsp_ble_uart_write(data, len) == BSP_OK);
}

/**
 * @brief 将协议层单字节读取回调适配到 UART2 BSP。
 */
static bool dx_ble_transport_read_byte(void *context, uint8_t *data)
{
    (void)context;
    return (bsp_ble_uart_getc(data) == BSP_OK);
}

/**
 * @brief 清空 UART2 BLE 接收缓存。
 */
static void dx_ble_transport_flush(void *context)
{
    (void)context;
    bsp_ble_uart_flush_rx();
}

/**
 * @brief 提供协议层使用的毫秒计时源。
 */
static uint32_t dx_ble_transport_time_ms(void *context)
{
    (void)context;
    return osal_ticks_to_ms(osal_get_tick_count());
}

/**
 * @brief 提供协议层使用的任务级延时。
 */
static void dx_ble_transport_delay_ms(void *context, uint32_t delay_ms)
{
    (void)context;
    osal_task_delay_ms(delay_ms);
}

/**
 * @brief 初始化应用层 DX-BT311 BLE 服务及其 UART2 传输依赖。
 */
dx_bt311_status_t app_dx_ble_service_init(void)
{
    dx_bt311_transport_t transport;

    if (s_initialized) {
        return DX_BT311_OK;
    }
    if (bsp_ble_uart_init() != BSP_OK) {
        return DX_BT311_ERR_IO;
    }

    transport.context = NULL;
    transport.write = dx_ble_transport_write;
    transport.read_byte = dx_ble_transport_read_byte;
    transport.flush_rx = dx_ble_transport_flush;
    transport.time_ms = dx_ble_transport_time_ms;
    transport.delay_ms = dx_ble_transport_delay_ms;

    if (dx_bt311_init(&s_dx_bt311, &transport) != DX_BT311_OK) {
        bsp_ble_uart_deinit();
        return DX_BT311_ERR_IO;
    }

    s_initialized = true;
    return DX_BT311_OK;
}

/**
 * @brief 从应用层发起 DX-BT311 在线探测。
 */
dx_bt311_status_t app_dx_ble_probe(char *response, uint16_t response_size,
                                    uint32_t timeout_ms)
{
    if (!s_initialized) {
        return DX_BT311_ERR_NOT_INIT;
    }
    return dx_bt311_probe(&s_dx_bt311, response, response_size, timeout_ms);
}

/**
 * @brief 从应用层发送任意 AT 命令。
 */
dx_bt311_status_t app_dx_ble_send_at(const char *command,
                                      dx_bt311_line_end_t line_end,
                                      char *response, uint16_t response_size,
                                      uint32_t timeout_ms)
{
    if (!s_initialized) {
        return DX_BT311_ERR_NOT_INIT;
    }
    return dx_bt311_send_at(&s_dx_bt311, command, line_end, response,
                            response_size, timeout_ms);
}

/**
 * @brief 查找应用层允许使用的内置 DX-BT311 命令。
 */
bool app_dx_ble_find_command(const char *name,
                              dx_bt311_command_t *command)
{
    return dx_bt311_find_command(name, command);
}

/**
 * @brief 获取应用层内置命令元数据。
 */
const dx_bt311_command_info_t *app_dx_ble_get_command_info(
    dx_bt311_command_t command)
{
    return dx_bt311_get_command_info(command);
}

/**
 * @brief 执行内置查询命令并保存原始响应与解析结果。
 */
dx_bt311_status_t app_dx_ble_execute_command(dx_bt311_command_t command,
                                              app_dx_ble_command_result_t *result,
                                              uint32_t timeout_ms)
{
    bool prefix_matched = false;

    if (result == NULL) {
        return DX_BT311_ERR_INVALID_PARAM;
    }

    result->transfer_status = DX_BT311_ERR_NOT_INIT;
    result->parse_status = DX_BT311_ERR_UNEXPECTED_RESPONSE;
    result->format = APP_DX_BLE_RESPONSE_FORMAT_NONE;
    result->module_error = DX_BT311_ERROR_NONE;
    result->raw_response[0] = '\0';
    result->value[0] = '\0';

    if (!s_initialized) {
        return result->transfer_status;
    }

    result->transfer_status = dx_bt311_query(
        &s_dx_bt311, command, result->raw_response,
        (uint16_t)sizeof(result->raw_response), timeout_ms);

    /* 解析模块错误码（即使传输成功也可能包含 EEROR） */
    result->module_error = dx_bt311_parse_error(result->raw_response);

    /* 保留并解析部分响应（即使传输报告截断） */
    if (result->raw_response[0] != '\0') {
        result->parse_status = dx_bt311_extract_value(
            command, result->raw_response, result->value,
            (uint16_t)sizeof(result->value), &prefix_matched);
        if (result->parse_status == DX_BT311_OK) {
            result->format = prefix_matched ?
                APP_DX_BLE_RESPONSE_FORMAT_PREFIX_MATCHED :
                APP_DX_BLE_RESPONSE_FORMAT_RAW_FALLBACK;
        }
    }

    /* 查询命令收到 ERROR/EEROR 时，向上层返回模块错误，而不是 0。 */
    if (result->module_error != DX_BT311_ERROR_NONE) {
        return DX_BT311_ERR_MODULE;
    }

    return result->transfer_status;
}

/**
 * @brief 执行内置设置命令。
 */
dx_bt311_status_t app_dx_ble_set_command(dx_bt311_command_t command,
                                          const char *value,
                                          app_dx_ble_command_result_t *result,
                                          uint32_t timeout_ms)
{
    dx_bt311_status_t status;
    char local_response[APP_DX_BLE_RESPONSE_MAX];
    char *resp_ptr;
    uint16_t resp_size;

    if (!s_initialized) {
        return DX_BT311_ERR_NOT_INIT;
    }
    if (value == NULL) {
        return DX_BT311_ERR_INVALID_PARAM;
    }

    if (result != NULL) {
        result->transfer_status = DX_BT311_ERR_NOT_INIT;
        result->parse_status = DX_BT311_ERR_UNEXPECTED_RESPONSE;
        result->format = APP_DX_BLE_RESPONSE_FORMAT_NONE;
        result->module_error = DX_BT311_ERROR_NONE;
        result->raw_response[0] = '\0';
        result->value[0] = '\0';
        resp_ptr = result->raw_response;
        resp_size = (uint16_t)sizeof(result->raw_response);
    } else {
        resp_ptr = local_response;
        resp_size = (uint16_t)sizeof(local_response);
    }

    status = dx_bt311_set(&s_dx_bt311, command, value,
                          resp_ptr, resp_size, timeout_ms);

    if (result != NULL) {
        result->transfer_status = status;
        result->module_error = dx_bt311_parse_error(result->raw_response);
    }

    return status;
}

/**
 * @brief 搜索蓝牙设备（主机模式）。
 */
dx_bt311_status_t app_dx_ble_inquire(dx_bt311_inq_device_t *devices,
                                      uint8_t max_count,
                                      uint8_t *found_count,
                                      uint32_t timeout_ms)
{
    if (!s_initialized) {
        return DX_BT311_ERR_NOT_INIT;
    }
    return dx_bt311_inquire(&s_dx_bt311, devices, max_count,
                            found_count, timeout_ms);
}

/**
 * @brief 获取函数 app_dx_ble_get_last_inq_response，完成对应模块的功能处理。
 * @return 函数执行结果。
 */
const char *app_dx_ble_get_last_inq_response(void)
{
    return dx_bt311_get_last_inq_response();
}

/**
 * @brief 获取函数 app_dx_ble_get_last_connect_response，完成对应模块的功能处理。
 * @return 函数执行结果。
 */
const char *app_dx_ble_get_last_connect_response(void)
{
    return dx_bt311_get_last_connect_response();
}

/**
 * @brief 通过序号连接蓝牙设备（主机模式）。
 */
dx_bt311_status_t app_dx_ble_connect_by_seq(uint8_t seq,
                                             char *mac_out, uint16_t mac_size,
                                             uint32_t timeout_ms)
{
    if (!s_initialized) {
        return DX_BT311_ERR_NOT_INIT;
    }
    return dx_bt311_connect_by_seq(&s_dx_bt311, seq, mac_out, mac_size,
                                    timeout_ms);
}

/**
 * @brief 通过 MAC 地址连接蓝牙设备（主机模式）。
 */
dx_bt311_status_t app_dx_ble_connect_by_addr(const char *mac,
                                              char *mac_out, uint16_t mac_size,
                                              uint32_t timeout_ms)
{
    if (!s_initialized) {
        return DX_BT311_ERR_NOT_INIT;
    }
    return dx_bt311_connect_by_addr(&s_dx_bt311, mac, mac_out, mac_size,
                                     timeout_ms);
}

/**
 * @brief 发送透明通道数据。
 */
dx_bt311_status_t app_dx_ble_send(const uint8_t *data, uint16_t len)
{
    if (!s_initialized) {
        return DX_BT311_ERR_NOT_INIT;
    }
    return dx_bt311_send(&s_dx_bt311, data, len);
}

/**
 * @brief 读取透明通道接收数据。
 */
dx_bt311_status_t app_dx_ble_receive(uint8_t *data, uint16_t capacity,
                                      uint16_t *received)
{
    if (!s_initialized) {
        return DX_BT311_ERR_NOT_INIT;
    }
    return dx_bt311_receive(&s_dx_bt311, data, capacity, received);
}

/**
 * @brief 清空应用层 BLE 接收缓存。
 */
void app_dx_ble_flush_rx(void)
{
    if (s_initialized) {
        bsp_ble_uart_flush_rx();
    }
}

/**
 * @brief 获取 BLE 服务状态和 UART2 诊断计数。
 */
void app_dx_ble_get_status(app_dx_ble_status_t *status)
{
    if (status == NULL) {
        return;
    }

    status->initialized = s_initialized;
    status->detected = s_initialized && dx_bt311_is_detected(&s_dx_bt311);
    status->baud_rate = DX_BT311_UART_BAUD;
    status->rx_pending = bsp_ble_uart_available();
    (void)bsp_ble_uart_get_diag(&status->uart_diag);
    status->role = PRJ_BLE_ROLE;
    status->config_applied = s_config_applied;
    status->last_config_status = s_last_config_status;
    status->last_connect_status = s_last_connect_status;
}


/* ======================== 工程配置应用 ======================== */

/**
 * @brief 判断配置字符串是否有效。
 */
static bool app_dx_ble_configured_string(const char *value)
{
    return (value != NULL) && (value[0] != '\0');
}

/**
 * @brief 向模块写入一项非空的工程配置。
 */
static dx_bt311_status_t app_dx_ble_apply_one_config(
    dx_bt311_command_t command, const char *label, const char *value)
{
    dx_bt311_status_t status;
    const dx_bt311_command_info_t *info;

    if (!app_dx_ble_configured_string(value)) {
        return DX_BT311_OK;
    }

    status = app_dx_ble_set_command(command, value, NULL,
                                    PRJ_BLE_INIT_TIMEOUT_MS);
    if (status != DX_BT311_OK) {
        printf("[BLE] AT config %s failed: %d\r\n", label, (int)status);
        return status;
    }

    info = dx_bt311_get_command_info(command);
    if ((info != NULL) && info->needs_reboot) {
        s_config_reboot_required = true;
    }
    printf("[BLE] AT config %s applied: %s\r\n", label, value);
    return DX_BT311_OK;
}

/**
 * @brief 根据 project_config.h 应用主机或从机配置。
 */
dx_bt311_status_t app_dx_ble_apply_project_config(void)
{
    dx_bt311_status_t status;
    const char *role_value;

    if (!s_initialized) {
        s_last_config_status = DX_BT311_ERR_NOT_INIT;
        return s_last_config_status;
    }

#if (PRJ_BLE_ROLE == PRJ_BLE_ROLE_MASTER)
    role_value = "1";
#else
    role_value = "0";
#endif

    s_config_reboot_required = false;
    status = app_dx_ble_apply_one_config(DX_BT311_CMD_ROLE, "ROLE", role_value);
    if (status != DX_BT311_OK) {
        s_config_applied = false;
        s_last_config_status = status;
        return status;
    }

#if (PRJ_BLE_ROLE == PRJ_BLE_ROLE_MASTER)
    status = app_dx_ble_apply_one_config(DX_BT311_CMD_MUUID, "MUUID",
                                         PRJ_BLE_MASTER_MUUID);
#else
    status = app_dx_ble_apply_one_config(DX_BT311_CMD_NAME, "NAME",
                                         PRJ_BLE_SLAVE_NAME);
    if (status == DX_BT311_OK) {
        status = app_dx_ble_apply_one_config(DX_BT311_CMD_UUID, "UUID",
                                             PRJ_BLE_SLAVE_UUID);
    }
    if (status == DX_BT311_OK) {
        status = app_dx_ble_apply_one_config(DX_BT311_CMD_CHAR, "CHAR",
                                             PRJ_BLE_SLAVE_CHAR);
    }
    if (status == DX_BT311_OK) {
        status = app_dx_ble_apply_one_config(DX_BT311_CMD_WRITE, "WRITE",
                                             PRJ_BLE_SLAVE_WRITE);
    }
    if (status == DX_BT311_OK) {
        status = app_dx_ble_apply_one_config(DX_BT311_CMD_NOTI, "NOTI",
                                             PRJ_BLE_SLAVE_NOTI);
    }
    if (status == DX_BT311_OK) {
        status = app_dx_ble_apply_one_config(DX_BT311_CMD_ADVI, "ADVI",
                                             PRJ_BLE_SLAVE_ADVI);
    }
    if (status == DX_BT311_OK) {
        status = app_dx_ble_apply_one_config(DX_BT311_CMD_CLOSEADV,
                                             "CLOSEADV", PRJ_BLE_SLAVE_CLOSEADV);
    }
#endif

    s_config_applied = (status == DX_BT311_OK);
    s_last_config_status = status;
    if (status == DX_BT311_OK) {
        printf("[BLE] project config applied: role=%s\r\n",
#if (PRJ_BLE_ROLE == PRJ_BLE_ROLE_MASTER)
               "MASTER"
#else
               "SLAVE"
#endif
        );
        if (s_config_reboot_required) {
            printf("[BLE] module reset may be required for persistent settings\r\n");
        }
    }
    return status;
}

/**
 * @brief 按工程配置连接指定 MAC 或名称目标。
 */
dx_bt311_status_t app_dx_ble_connect_project_target(void)
{
    dx_bt311_status_t status;
    char connected_mac[13] = {0};

    if (!s_initialized) {
        s_last_connect_status = DX_BT311_ERR_NOT_INIT;
        return s_last_connect_status;
    }
#if (PRJ_BLE_ROLE != PRJ_BLE_ROLE_MASTER)
    s_last_connect_status = DX_BT311_ERR_NOT_CONFIGURED;
    return s_last_connect_status;
#else
    if (app_dx_ble_configured_string(PRJ_BLE_MASTER_TARGET_MAC)) {
        status = app_dx_ble_connect_by_addr(PRJ_BLE_MASTER_TARGET_MAC,
                                            connected_mac,
                                            (uint16_t)sizeof(connected_mac),
                                            PRJ_BLE_CONNECT_TIMEOUT_MS);
        printf("[BLE] master MAC connect %s: %d (%s)\r\n",
               PRJ_BLE_MASTER_TARGET_MAC, (int)status,
               (connected_mac[0] != '\0') ? connected_mac : "no peer");
        s_last_connect_status = status;
        return status;
    }

    if (app_dx_ble_configured_string(PRJ_BLE_MASTER_TARGET_NAME)) {
        dx_bt311_inq_device_t devices[PRJ_BLE_MASTER_MAX_DEVICES];
        uint8_t found_count = 0U;
        uint8_t i;

        status = app_dx_ble_inquire(devices, PRJ_BLE_MASTER_MAX_DEVICES,
                                    &found_count, PRJ_BLE_INQUIRE_TIMEOUT_MS);
        if (status != DX_BT311_OK) {
            printf("[BLE] master inquiry failed: %d\r\n", (int)status);
            s_last_connect_status = status;
            return status;
        }
        for (i = 0U; i < found_count; i++) {
            if (strcmp(devices[i].name, PRJ_BLE_MASTER_TARGET_NAME) == 0) {
                status = app_dx_ble_connect_by_seq(
                    devices[i].seq, connected_mac,
                    (uint16_t)sizeof(connected_mac), PRJ_BLE_CONNECT_TIMEOUT_MS);
                printf("[BLE] master name connect %s (%s): %d\r\n",
                       PRJ_BLE_MASTER_TARGET_NAME,
                       (connected_mac[0] != '\0') ? connected_mac : devices[i].mac,
                       (int)status);
                s_last_connect_status = status;
                return status;
            }
        }
        printf("[BLE] master target name not found: %s\r\n",
               PRJ_BLE_MASTER_TARGET_NAME);
        s_last_connect_status = DX_BT311_ERR_CONNECT_FAILED;
        return s_last_connect_status;
    }

    printf("[BLE] master target is not configured (MAC/name empty)\r\n");
    s_last_connect_status = DX_BT311_ERR_NOT_CONFIGURED;
    return s_last_connect_status;
#endif
}

/**
 * @brief BLE 服务任务：初始化、探测、应用配置并按需连接。
 */
void app_dx_ble_service_task(void *param)
{
    dx_bt311_status_t status;
    char response[APP_DX_BLE_RESPONSE_MAX];

    (void)param;
#if (PRJ_BLE_AUTO_INIT == 0U)
    for (;;) {
        osal_task_delay_ms(1000U);
    }
#else
    status = app_dx_ble_service_init();
    if (status != DX_BT311_OK) {
        printf("[BLE] service init failed: %d; system continues\r\n", (int)status);
        for (;;) {
            osal_task_delay_ms(1000U);
        }
    }
    printf("[BLE] UART2 service initialized, role=%s\r\n",
#if (PRJ_BLE_ROLE == PRJ_BLE_ROLE_MASTER)
           "MASTER"
#else
           "SLAVE"
#endif
    );

#if (PRJ_BLE_AUTO_PROBE != 0U)
    response[0] = '\0';
    status = app_dx_ble_probe(response, (uint16_t)sizeof(response),
                              PRJ_BLE_INIT_TIMEOUT_MS);
    printf("[BLE] probe: %d, response=%s\r\n", (int)status,
           (response[0] != '\0') ? response : "<none>");
#endif

#if (PRJ_BLE_APPLY_AT_CONFIG != 0U)
    status = app_dx_ble_apply_project_config();
    if (status != DX_BT311_OK) {
        printf("[BLE] project config failed: %d; system continues\r\n",
               (int)status);
    }
#endif

#if ((PRJ_BLE_ROLE == PRJ_BLE_ROLE_MASTER) && (PRJ_BLE_AUTO_CONNECT != 0U))
#if (PRJ_BLE_APPLY_AT_CONFIG != 0U)
    if (!s_config_applied) {
        printf("[BLE] auto connect skipped: project config was not applied\r\n");
    } else if (s_config_reboot_required) {
        printf("[BLE] auto connect skipped: module reset is required first\r\n");
    } else {
        status = app_dx_ble_connect_project_target();
        if (status != DX_BT311_OK) {
            printf("[BLE] auto connect failed: %d; system continues\r\n",
                   (int)status);
        }
    }
#else
    printf("[BLE] auto connect assumes the module is already in MASTER role\r\n");
    status = app_dx_ble_connect_project_target();
    if (status != DX_BT311_OK) {
        printf("[BLE] auto connect failed: %d; system continues\r\n",
               (int)status);
    }
#endif
#endif

    for (;;) {
        osal_task_delay_ms(1000U);
    }
#endif
}
