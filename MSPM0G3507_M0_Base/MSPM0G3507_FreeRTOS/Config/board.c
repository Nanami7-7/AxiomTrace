/**
 * @file    board.c
 * @brief   Board B 中断转发入口。
 *
 * UART0 保留给调试；UART1 连接 Board A；UART2 连接 BLE 模块。
 * 中断函数只搬运字节到各自的环形缓冲，不执行协议解析或业务逻辑。
 */
#include "ti_msp_dl_config.h"
#include "proto_uart1_b.h"
#include "bsp_ble_uart.h"
#include "bsp_debug.h"

void UART0_IRQHandler(void)
{
    bsp_debug_irq_handler();
}

void UART1_IRQHandler(void)
{
    proto_uart1_b_irq_handler();
}

void UART2_IRQHandler(void)
{
    bsp_ble_uart_irq_handler();
}
