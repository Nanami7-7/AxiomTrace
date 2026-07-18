#include "bsp_timer.h"
#include "ti_msp_dl_config.h"
#include "project_config.h" /* PRJ_US_PER_MS */

static volatile uint32_t s_timer_overflow = 0;

void TIMER_0_INST_IRQHandler(void)
{
    DL_TimerG_clearInterruptStatus(TIMER_0_INST, DL_TIMERG_INTERRUPT_LOAD_EVENT);
    s_timer_overflow++;
}

void bsp_timer_init(void)
{
    DL_TimerG_startCounter(TIMER_0_INST);
    NVIC_EnableIRQ(TIMER_0_INST_INT_IRQN);
}

uint32_t bsp_get_us(void)
{
    uint32_t overflow;
    uint32_t counter;

    __disable_irq();
    overflow = s_timer_overflow;
    counter = DL_TimerG_getTimerCount(TIMER_0_INST);
    __enable_irq();

    return overflow * (TIMER_0_INST_LOAD_VALUE + 1) + counter;
}

uint32_t bsp_get_ms(void)
{
    return bsp_get_us() / PRJ_US_PER_MS;
}

void bsp_delay_us(uint32_t us)
{
    uint32_t start = bsp_get_us();
    while ((bsp_get_us() - start) < us) {
    }
}

void bsp_delay_ms(uint32_t ms)
{
    bsp_delay_us((uint32_t)ms * 1000);
}
