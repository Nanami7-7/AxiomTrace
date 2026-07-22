/**
 * @file    app_ble_service.c
 * @brief   JDY-23 service wiring. Protocol remains independent of DriverLib.
 */
#include "app_ble_service.h"
#include "osal_api.h"
#include "project_config.h"
#include <stddef.h>

static jdy23_t s_jdy23;
static bool s_initialized;

static bool ble_transport_write(void *context, const uint8_t *data,
                                uint16_t len)
{
    (void)context;
    return (bsp_ble_uart_write(data, len) == BSP_OK);
}

static bool ble_transport_read_byte(void *context, uint8_t *data)
{
    (void)context;
    return (bsp_ble_uart_getc(data) == BSP_OK);
}

static void ble_transport_flush(void *context)
{
    (void)context;
    bsp_ble_uart_flush_rx();
}

static uint32_t ble_transport_time_ms(void *context)
{
    (void)context;
    return osal_ticks_to_ms(osal_get_tick_count());
}

static void ble_transport_delay_ms(void *context, uint32_t delay_ms)
{
    (void)context;
    osal_task_delay_ms(delay_ms);
}

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

jdy23_status_t app_ble_probe(char *response, uint16_t response_size,
                             uint32_t timeout_ms)
{
    if (!s_initialized) {
        return JDY23_ERR_NOT_INIT;
    }
    return jdy23_probe(&s_jdy23, response, response_size, timeout_ms);
}

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

bool app_ble_find_command(const char *name, jdy23_command_t *command)
{
    return jdy23_find_command(name, command);
}

const jdy23_command_info_t *app_ble_get_command_info(jdy23_command_t command)
{
    return jdy23_get_command_info(command);
}

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

jdy23_status_t app_ble_send(const uint8_t *data, uint16_t len)
{
    if (!s_initialized) {
        return JDY23_ERR_NOT_INIT;
    }
    return jdy23_send(&s_jdy23, data, len);
}

jdy23_status_t app_ble_receive(uint8_t *data, uint16_t capacity,
                               uint16_t *received)
{
    if (!s_initialized) {
        return JDY23_ERR_NOT_INIT;
    }
    return jdy23_receive(&s_jdy23, data, capacity, received);
}

void app_ble_flush_rx(void)
{
    if (s_initialized) {
        bsp_ble_uart_flush_rx();
    }
}

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
