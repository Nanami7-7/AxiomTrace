/**
 * @file    axiom_port_mspm0.c
 * @brief   AxiomTrace MSPM0 平台移植
 * @note    实现 axiom_port_string_out() → bsp_uart_puts()
 */
#include "axiom_port.h"
#include "bsp_uart.h"

void axiom_port_string_out(const char *str)
{
    if (str != NULL) {
        (void)bsp_uart_puts(str);
    }
}
