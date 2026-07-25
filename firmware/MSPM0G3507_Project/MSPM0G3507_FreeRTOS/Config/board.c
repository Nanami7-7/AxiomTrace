#include "board.h"
#include "bsp_uart.h"
#include "bsp_adc.h"
#include "ti_msp_dl_config.h"
#include <stdio.h>


/**
 * @brief  UART0中断服务函数
 * @note   调用BSP层IRQ handler, 仅做读数据+存环形缓冲区
 */
void UART_0_DEBUG_INST_IRQHandler(void)
{
	bsp_uart_irq_handler();
}

/**
 * @brief  ADC1中断服务函数
 * @note   ADC1的5个MEM通道序列转换完成后触发(MEM4_RESULT_LOADED)
 *         一次性读取5个MEM结果(4路电流+1路电压), 设置done标志
 */
void ADC1_IRQHandler(void)
{
	bsp_adc_irq_handler();
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

