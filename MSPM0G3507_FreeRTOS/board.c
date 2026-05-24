#include "board.h"
#include "bsp_uart.h"
#include "ti_msp_dl_config.h"
#include <stdio.h>


void delay_ms(uint32_t count)
{
	uint32_t told=SysTick->VAL;
	uint32_t ticks;
	uint32_t tnow;
	uint32_t tcnt=0;
	uint32_t reload=SysTick->LOAD;
	ticks=(CPUCLK_FREQ/1000)*count;//计算完成延时时间所需的systick计数值
	told=SysTick->VAL;//开始延时前记录当前的计数器值
	while(1)//循环等待延时结束
	{
		tnow=SysTick->VAL;
		if(tnow!=told)
		{
			if(tnow<told)
				tcnt+=told-tnow;//记录systick的计数值
			else
				tcnt+=reload+told-tnow;//如果systick重装载了，记录systick的计数值
			told=tnow;
			if(tcnt>=ticks)//systick的计数值达到所需延时时间
				break;//结束延时
		}
	}
}
void delay_us(uint32_t count)
{
	uint32_t told=SysTick->VAL;
	uint32_t ticks;
	uint32_t tnow;
	uint32_t tcnt=0;
	uint32_t reload=SysTick->LOAD;
	ticks=(CPUCLK_FREQ/1000000)*count;//计算完成延时时间所需的systick计数值
	told=SysTick->VAL;//开始延时前记录当前的计数器值
	while(1)//循环等待延时结束
	{
		tnow=SysTick->VAL;
		if(tnow!=told)
		{
			if(tnow<told)
				tcnt+=told-tnow;//记录systick的计数值
			else
				tcnt+=reload+told-tnow;//如果systick重装载了，记录systick的计数值
			told=tnow;
			if(tcnt>=ticks)//systick的计数值达到所需延时时间
				break;//结束延时
		}
	}
}


void delay_1us(uint32_t  __us){ delay_us(__us); }
void delay_1ms( uint32_t ms){ delay_ms(ms); }

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
	/* 超时保护: 约1ms@80MHz, 防止UART故障时永久阻塞 */
	uint32_t timeout = 80000U;
	while( DL_UART_isBusy(UART_0_DEBUG_INST) == true ) {
		if (--timeout == 0U) { return -1; }
	}
	
	DL_UART_Main_transmitData(UART_0_DEBUG_INST, ch);
	
	return ch;
}

