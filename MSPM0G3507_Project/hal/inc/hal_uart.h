/**
 * @file    hal_uart.h
 * @brief   UART hardware abstraction layer interface
 */

#ifndef HAL_UART_H
#define HAL_UART_H

#ifdef __cplusplus
extern "C" {
#endif

#include "hal_common.h"

/* ---------- Parity ---------- */
typedef enum {
    HAL_UART_PARITY_NONE = 0,
    HAL_UART_PARITY_ODD  = 1,
    HAL_UART_PARITY_EVEN = 2,
} hal_uart_parity_t;

/* ---------- Stop bits ---------- */
typedef enum {
    HAL_UART_STOPBITS_1 = 0,
    HAL_UART_STOPBITS_2 = 1,
} hal_uart_stopbits_t;

/* ---------- Data bits ---------- */
typedef enum {
    HAL_UART_DATABITS_7 = 7,
    HAL_UART_DATABITS_8 = 8,
} hal_uart_databits_t;

/* ---------- UART configuration ---------- */
typedef struct {
    uint32_t            instance;      /* UART_0_INST / UART_1_INST */
    uint32_t            baudrate;      /* 9600, 115200, etc. */
    hal_uart_databits_t data_bits;
    hal_uart_stopbits_t stop_bits;
    hal_uart_parity_t   parity;
    hal_irq_priority_t  irq_priority;
} hal_uart_config_t;

/* ---------- UART callbacks ---------- */
typedef void (*hal_uart_rx_cb_t)(uint32_t instance, uint8_t data);
typedef void (*hal_uart_tx_cb_t)(uint32_t instance);

/* ========== API ========== */

hal_status_t hal_uart_init(const hal_uart_config_t *config);
hal_status_t hal_uart_deinit(uint32_t instance);

hal_status_t hal_uart_write(uint32_t instance, const uint8_t *data, uint16_t len);
hal_status_t hal_uart_read(uint32_t instance, uint8_t *buf, uint16_t len, uint32_t timeout_ms);

hal_status_t hal_uart_write_byte(uint32_t instance, uint8_t byte);
hal_status_t hal_uart_read_byte(uint32_t instance, uint8_t *byte, uint32_t timeout_ms);

hal_status_t hal_uart_register_rx_callback(uint32_t instance, hal_uart_rx_cb_t cb);
hal_status_t hal_uart_register_tx_callback(uint32_t instance, hal_uart_tx_cb_t cb);

#ifdef __cplusplus
}
#endif

#endif /* HAL_UART_H */
