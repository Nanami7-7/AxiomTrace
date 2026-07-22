/**
 * @file    bsp_ble_uart.h
 * @brief   UART1 byte-stream transport for external BLE modules.
 * @note    Owns only UART1 RX buffering and polling TX. It does not parse
 *          JDY-23 AT commands and does not share UART0 debug/DMA state.
 */
#ifndef BSP_BLE_UART_H
#define BSP_BLE_UART_H

#ifdef __cplusplus
extern "C" {
#endif

#include "bsp_common.h"

#define BSP_BLE_UART_RX_BUF_SIZE (256U)

typedef struct {
    uint32_t rx_bytes;
    uint32_t rx_overflow;
    uint32_t irq_count;
    uint32_t ignored_irq_count;
} bsp_ble_uart_diag_t;

bsp_status_t bsp_ble_uart_init(void);
void bsp_ble_uart_deinit(void);
bsp_status_t bsp_ble_uart_write(const uint8_t *data, uint16_t len);
bsp_status_t bsp_ble_uart_getc(uint8_t *data);
uint32_t bsp_ble_uart_available(void);
void bsp_ble_uart_flush_rx(void);
bsp_status_t bsp_ble_uart_get_diag(bsp_ble_uart_diag_t *diag);
void bsp_ble_uart_irq_handler(void);

#ifdef __cplusplus
}
#endif

#endif /* BSP_BLE_UART_H */
