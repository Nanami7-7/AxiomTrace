/**
 * @file    hal_common.c
 * @brief   HAL common utilities implementation
 * @note    Provides hal_get_tick() - default uses SysTick, optional FreeRTOS tick
 */

#include "hal_conf.h"
#include "hal_common.h"

#ifdef HAL_COMMON_MODULE_ENABLED

/* ---------- Tick counter implementation ---------- */

#ifdef HAL_USE_FREERTOS_TICK
/* Use FreeRTOS tick count (requires FreeRTOS.h in include path) */
#include "FreeRTOS.h"
#include "task.h"

uint32_t hal_get_tick(void)
{
    return (uint32_t)xTaskGetTickCount();
}

#else
/* Use ARM Cortex-M SysTick counter (default, RTOS-agnostic) */
#include "systick.h"  /* Provided by TI driver or custom systick implementation */

#ifndef SYSTICK_LOAD_RELOAD
/* Fallback: Simple millisecond counter using DWT or generic approach */
#include "DL.h"  /* TI DriverLib header */

/* Systick configuration - typically done in system_mspm0g3507.c or startup */
extern uint32_t SystemCoreClock;

static volatile uint32_t s_hal_tick = 0;

/* Call this from SysTick interrupt (typically 1ms interval) */
void HAL_SysTick_Handler(void)
{
    s_hal_tick++;
}

uint32_t hal_get_tick(void)
{
    /* If SysTick is configured by system, use it directly */
    #ifdef SysTick
    return (uint32_t)SysTick->VAL;
    #else
    return s_hal_tick;
    #endif
}

#else
/* Direct SysTick usage */
uint32_t hal_get_tick(void)
{
    /* SysTick is typically configured to count down from LOAD to 0, then wrap.
     * For ms-based tick, typically:
     *   SysTick->VAL gives current count (counts down)
     *   SysTick->LOAD gives reload value
     * The actual ms calculation depends on clock configuration.
     *
     * This is a simplified version - adjust based on actual clock setup.
     * In most MSPM0G projects, the system clock is used for SysTick.
     */
    extern uint32_t SystemCoreClock;
    static uint32_t s_hal_ticks = 0;
    static uint32_t s_last_val = 0;

    uint32_t curr_val = SysTick->VAL;
    uint32_t reload = SysTick->LOAD;

    /* Detect overflow (wraparound) */
    if (curr_val > s_last_val) {
        s_hal_ticks += (s_last_val + 1);
    }
    s_last_val = curr_val;

    /* Return total ticks (approximation based on SysTick config) */
    /* Adjust this formula based on actual SysTick configuration for accurate ms */
    return s_hal_ticks / (SystemCoreClock / 1000U);
}

#endif /* SYSTICK_LOAD_RELOAD */

#endif /* HAL_USE_FREERTOS_TICK */

#endif /* HAL_COMMON_MODULE_ENABLED */
