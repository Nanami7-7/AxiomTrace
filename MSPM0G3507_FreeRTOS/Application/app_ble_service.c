/**
 * @file    app_ble_service.c
 * @brief   JDY-23 应用服务层与 UART1 BSP 的适配实现。
 * @details
 * 本文件负责组装 jdy23_transport_t 回调，将协议设备驱动连接到 UART1 板级
 * 服务，并对外提供应用层可直接使用的命令、透明数据和诊断接口。协议层不
 * 感知 FreeRTOS、DriverLib 或具体 UART 实例。
 */
#include "app_ble_service.h"
#include "osal_api.h"
#include "project_config.h"
#include <stddef.h>

/** @brief 应用层唯一使用的 JDY-23 协议实例。 */
static jdy23_t s_jdy23;

/** @brief UART1 BLE 服务是否已经完成初始化。 */
static bool s_initialized;

/**
 * @brief 将协议层发送回调适配到 UART1 BSP。
 * @param context 保留参数，当前实现未使用。
 * @param data 待发送数据。
 * @param len 数据长度，单位为字节。
 * @return UART1 成功发送返回 true，否则返回 false。
 */
static bool ble_transport_write(void *context, const uint8_t *data,
                                uint16_t len)
{
    (void)context;
    return (bsp_ble_uart_write(data, len) == BSP_OK);
}

/**
 * @brief 将协议层单字节读取回调适配到 UART1 BSP。
 * @param context 保留参数，当前实现未使用。
 * @param[out] data 输出读取到的字节。
 * @return 读取到字节返回 true；当前没有数据或发生错误返回 false。
 */
static bool ble_transport_read_byte(void *context, uint8_t *data)
{
    (void)context;
    return (bsp_ble_uart_getc(data) == BSP_OK);
}

/**
 * @brief 清空 UART1 BLE 接收缓存。
 * @param context 保留参数，当前实现未使用。
 */
static void ble_transport_flush(void *context)
{
    (void)context;
    bsp_ble_uart_flush_rx();
}

/**
 * @brief 提供协议层使用的毫秒计时源。
 * @param context 保留参数，当前实现未使用。
 * @return 当前 OSAL 单调时间，单位为毫秒。
 */
static uint32_t ble_transport_time_ms(void *context)
{
    (void)context;
    return osal_ticks_to_ms(osal_get_tick_count());
}

/**
 * @brief 提供协议层使用的任务级延时。
 * @param context 保留参数，当前实现未使用。
 * @param delay_ms 延时时间，单位为毫秒。
 */
static void ble_transport_delay_ms(void *context, uint32_t delay_ms)
{
    (void)context;
    osal_task_delay_ms(delay_ms);
}

/**
 * @brief 初始化应用层 BLE 服务及其 UART1 传输依赖。
 * @return 初始化状态。
 */
jdy23_status_t app_ble_service_init(void)
{
    jdy23_transport_t transport;

    if (s_initialized) {
        return JDY23_OK;
    }
    if (bsp_ble_uart_init() != BSP_OK) {
        return JDY23_ERR_IO;
    }

    transport.context = NULL;
    transport.write = ble_transport_write;
    transport.read_byte = ble_transport_read_byte;
    transport.flush_rx = ble_transport_flush;
    transport.time_ms = ble_transport_time_ms;
    transport.delay_ms = ble_transport_delay_ms;

    if (jdy23_init(&s_jdy23, &transport) != JDY23_OK) {
        bsp_ble_uart_deinit();
        return JDY23_ERR_IO;
    }

    s_initialized = true;
    return JDY23_OK;
}

/**
 * @brief 从应用层发起 JDY-23 在线探测。
 * @param[out] response 原始响应缓冲区。
 * @param response_size 缓冲区容量。
 * @param timeout_ms 探测超时时间，单位为毫秒。
 * @return 探测状态。
 */
jdy23_status_t app_ble_probe(char *response, uint16_t response_size,
                             uint32_t timeout_ms)
{
    if (!s_initialized) {
        return JDY23_ERR_NOT_INIT;
    }
    return jdy23_probe(&s_jdy23, response, response_size, timeout_ms);
}

/**
 * @brief 从应用层发送任意 AT 命令。
 * @param command 命令文本，不包含行结束符。
 * @param line_end 行结束符策略。
 * @param[out] response 原始响应缓冲区。
 * @param response_size 响应缓冲区容量。
 * @param timeout_ms 最大等待时间，单位为毫秒。
 * @return 传输状态。
 */
jdy23_status_t app_ble_send_at(const char *command, jdy23_line_end_t line_end,
                               char *response, uint16_t response_size,
                               uint32_t timeout_ms)
{
    if (!s_initialized) {
        return JDY23_ERR_NOT_INIT;
    }
    return jdy23_send_at(&s_jdy23, command, line_end, response,
                         response_size, timeout_ms);
}

/**
 * @brief 查找应用层允许使用的内置 JDY-23 命令。
 * @param name 短命令名或完整 AT 命令。
 * @param[out] command 输出命令索引。
 * @return 找到返回 true，否则返回 false。
 */
bool app_ble_find_command(const char *name, jdy23_command_t *command)
{
    return jdy23_find_command(name, command);
}

/**
 * @brief 获取应用层内置命令元数据。
 * @param command 命令索引。
 * @return 元数据指针；索引非法时返回 NULL。
 */
const jdy23_command_info_t *app_ble_get_command_info(jdy23_command_t command)
{
    return jdy23_get_command_info(command);
}

/**
 * @brief 执行内置命令并保存原始响应与解析结果。
 * @param command 命令索引。
 * @param[out] result 输出事务和解析结果。
 * @param timeout_ms 命令超时时间，单位为毫秒。
 * @return 传输状态。
 */
jdy23_status_t app_ble_execute_command(jdy23_command_t command,
                                       app_ble_command_result_t *result,
                                       uint32_t timeout_ms)
{
    bool prefix_matched = false;

    if (result == NULL) {
        return JDY23_ERR_INVALID_PARAM;
    }

    result->transfer_status = JDY23_ERR_NOT_INIT;
    result->parse_status = JDY23_ERR_UNEXPECTED_RESPONSE;
    result->format = APP_BLE_RESPONSE_FORMAT_NONE;
    result->raw_response[0] = '\0';
    result->value[0] = '\0';

    if (!s_initialized) {
        return result->transfer_status;
    }

    result->transfer_status = jdy23_execute_command(
        &s_jdy23, command, result->raw_response,
        (uint16_t)sizeof(result->raw_response), timeout_ms);

    /* Preserve and parse a partial response even when transport reports
     * truncation. Unknown firmware formats deliberately fall back to raw. */
    if (result->raw_response[0] != '\0') {
        result->parse_status = jdy23_extract_response_value(
            command, result->raw_response, result->value,
            (uint16_t)sizeof(result->value), &prefix_matched);
        if (result->parse_status == JDY23_OK) {
            result->format = prefix_matched ?
                APP_BLE_RESPONSE_FORMAT_PREFIX_MATCHED :
                APP_BLE_RESPONSE_FORMAT_RAW_FALLBACK;
        }
    }

    return result->transfer_status;
}

/**
 * @brief 发送透明通道数据。
 * @param data 待发送数据。
 * @param len 数据长度，单位为字节。
 * @return 发送状态。
 */
jdy23_status_t app_ble_send(const uint8_t *data, uint16_t len)
{
    if (!s_initialized) {
        return JDY23_ERR_NOT_INIT;
    }
    return jdy23_send(&s_jdy23, data, len);
}

/**
 * @brief 读取透明通道接收数据。
 * @param[out] data 接收缓冲区。
 * @param capacity 缓冲区容量。
 * @param[out] received 实际读取字节数。
 * @return 读取状态。
 */
jdy23_status_t app_ble_receive(uint8_t *data, uint16_t capacity,
                               uint16_t *received)
{
    if (!s_initialized) {
        return JDY23_ERR_NOT_INIT;
    }
    return jdy23_receive(&s_jdy23, data, capacity, received);
}

/**
 * @brief 清空应用层 BLE 接收缓存。
 */
void app_ble_flush_rx(void)
{
    if (s_initialized) {
        bsp_ble_uart_flush_rx();
    }
}

/**
 * @brief 获取 BLE 服务状态和 UART1 诊断计数。
 * @param[out] status 输出状态快照。
 */
void app_ble_get_status(app_ble_status_t *status)
{
    if (status == NULL) {
        return;
    }

    status->initialized = s_initialized;
    status->detected = s_initialized && jdy23_is_detected(&s_jdy23);
    status->baud_rate = PRJ_JDY23_UART_BAUD;
    status->rx_pending = bsp_ble_uart_available();
    (void)bsp_ble_uart_get_diag(&status->uart_diag);
}
