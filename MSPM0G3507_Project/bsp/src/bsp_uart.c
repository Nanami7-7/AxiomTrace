/**
 * @file    bsp_uart.c
 * @brief   BSP UART implementation (debug serial port)
 */

#include "bsp_uart.h"
#include "bsp_pin_config.h"
#include "bsp_config.h"
#include "hal_uart.h"

#include <stdarg.h>
#include <stdio.h>

/* ========== API ========== */

hal_status_t bsp_uart_init(void)
{
    hal_uart_config_t cfg = {
        .instance     = BSP_DEBUG_UART_INST,
        .baudrate     = BSP_DEBUG_UART_BAUDRATE,
        .data_bits    = HAL_UART_DATABITS_8,
        .stop_bits    = HAL_UART_STOPBITS_1,
        .parity       = HAL_UART_PARITY_NONE,
        .irq_priority = HAL_IRQ_PRIORITY_MEDIUM,
    };

    return hal_uart_init(&cfg);
}

hal_status_t bsp_uart_send(const uint8_t *data, uint16_t len)
{
    return hal_uart_write(BSP_DEBUG_UART_INST, data, len);
}

hal_status_t bsp_uart_receive(uint8_t *buf, uint16_t len, uint32_t timeout_ms)
{
    return hal_uart_read(BSP_DEBUG_UART_INST, buf, len, timeout_ms);
}

int bsp_debug_printf(const char *fmt, ...)
{
    char buf[128];
    int len;

    va_list args;
    va_start(args, fmt);
    len = vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    if (len > 0) {
        if ((uint16_t)len > sizeof(buf) - 1) {
            len = sizeof(buf) - 1;
        }
        hal_uart_write(BSP_DEBUG_UART_INST, (const uint8_t *)buf, (uint16_t)len);
    }

    return len;
}
