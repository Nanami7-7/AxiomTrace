/**
 * @file    app_ble_service.h
 * @brief   Application-facing JDY-23 service bound to BSP UART1.
 */
#ifndef APP_BLE_SERVICE_H
#define APP_BLE_SERVICE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "bsp_ble_uart.h"
#include "jdy23.h"

#define APP_BLE_RESPONSE_MAX (128U)

typedef struct {
    bool initialized;
    bool detected;
    uint32_t baud_rate;
    uint32_t rx_pending;
    bsp_ble_uart_diag_t uart_diag;
} app_ble_status_t;

typedef enum {
    APP_BLE_RESPONSE_FORMAT_NONE = 0,
    APP_BLE_RESPONSE_FORMAT_PREFIX_MATCHED,
    APP_BLE_RESPONSE_FORMAT_RAW_FALLBACK,
} app_ble_response_format_t;

typedef struct {
    jdy23_status_t transfer_status;
    jdy23_status_t parse_status;
    app_ble_response_format_t format;
    char raw_response[APP_BLE_RESPONSE_MAX];
    char value[APP_BLE_RESPONSE_MAX];
} app_ble_command_result_t;

jdy23_status_t app_ble_service_init(void);
jdy23_status_t app_ble_probe(char *response, uint16_t response_size,
                             uint32_t timeout_ms);
jdy23_status_t app_ble_send_at(const char *command, jdy23_line_end_t line_end,
                               char *response, uint16_t response_size,
                               uint32_t timeout_ms);
bool app_ble_find_command(const char *name, jdy23_command_t *command);
const jdy23_command_info_t *app_ble_get_command_info(jdy23_command_t command);
jdy23_status_t app_ble_execute_command(jdy23_command_t command,
                                       app_ble_command_result_t *result,
                                       uint32_t timeout_ms);
jdy23_status_t app_ble_send(const uint8_t *data, uint16_t len);
jdy23_status_t app_ble_receive(uint8_t *data, uint16_t capacity,
                               uint16_t *received);
void app_ble_flush_rx(void);
void app_ble_get_status(app_ble_status_t *status);

#ifdef __cplusplus
}
#endif

#endif /* APP_BLE_SERVICE_H */
