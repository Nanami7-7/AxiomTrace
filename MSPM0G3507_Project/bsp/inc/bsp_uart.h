/**
 * @file    bsp_uart.h
 * @brief   BSP UART interface (debug serial port)
 */

#ifndef BSP_UART_H
#define BSP_UART_H

#ifdef __cplusplus
extern "C" {
#endif

#include "hal_common.h"

/* ========== API ========== */

hal_status_t bsp_uart_init(void);

hal_status_t bsp_uart_send(const uint8_t *data, uint16_t len);
hal_status_t bsp_uart_receive(uint8_t *buf, uint16_t len, uint32_t timeout_ms);

/**
 * @brief  Printf-style debug output (requires OSAL mutex for thread safety)
 */
int bsp_debug_printf(const char *fmt, ...);

#ifdef __cplusplus
}
#endif

#endif /* BSP_UART_H */
