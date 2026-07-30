#include "board.h"
#include "bsp_uart.h"
#include "project_config.h"
#if (PRJ_UART1_BLE_DEBUG_ENABLE != 0U)
#include "app_uart1_ble_debug.h"
#else
#include "proto_uart1_a.h"
#endif
#include "ti_msp_dl_config.h"
#include <stdio.h>
/**
 * @brief  Board A UART1 板间协议中断入口。
 * @note   只搬运字节到环形缓冲，协议解析在任务上下文执行。
 */
void UART1_IRQHandler(void)
{
#if (PRJ_UART1_BLE_DEBUG_ENABLE != 0U)
    app_uart1_ble_debug_irq_handler();
#else
    proto_uart1_a_irq_handler();
#endif
}


/**
 * @brief  UART0中断服务函数
 * @note   调用BSP层IRQ handler, 仅做读数据+存环形缓冲区
 */
void UART_0_DEBUG_INST_IRQHandler(void)
{
    bsp_uart_irq_handler();
}

#if !defined(__MICROLIB)
#if (__ARMCLIB_VERSION <= 6000000)
struct __FILE
{
	int handle;
};
#endif

FILE __stdout;

void _sys_exit(int x)
{
	x = x;
}
#endif


int fputc(int ch, FILE *stream)
{
    (void)stream;

    /*
     * Do not let stdio bypass the BSP and write UART registers directly.
     * printf, putc/puts, and DMA frames now share the same EOT arbitration.
     */
    return (bsp_uart_putc((uint8_t)ch) == BSP_OK) ? ch : -1;
}

