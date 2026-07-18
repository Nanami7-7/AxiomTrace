/**
 * @file    osal_critical.c
 * @brief   OSAL critical-section implementation
 * @note    In the FreeRTOS build, taskENTER_CRITICAL() must not be called
 *          before vTaskStartScheduler(). The Cortex-M0 FreeRTOS port starts
 *          its nesting counter at a sentinel value and keeps interrupts
 *          disabled until the scheduler initializes that counter. Therefore
 *          pre-scheduler users use a small PRIMASK-based critical section;
 *          task-context users continue to use the FreeRTOS primitive.
 */
#include "osal_api.h"

/* ======================== Raw interrupt state helpers ======================== */

static uint32_t osal_irq_save_and_disable(void)
{
    uint32_t primask;

    __asm volatile (
        "mrs   %0, primask   \n"
        "cpsid i             \n"
        : "=r" (primask)
        :
        : "memory"
    );

    return primask;
}

static void osal_irq_restore(uint32_t primask)
{
    __asm volatile (
        "msr   primask, %0  \n"
        :
        : "r" (primask)
        : "memory"
    );
}

#if USE_FREERTOS

/*
 * FreeRTOS ARM_CM0 initializes its private critical-nesting counter with a
 * sentinel before the scheduler starts. Keep this independent pre-scheduler
 * nesting state so early BSP initialization and printf can still receive UART
 * interrupts. Scheduler start is never permitted while this nesting count is
 * non-zero.
 */
static uint32_t s_pre_scheduler_nest = 0U;
static uint32_t s_pre_scheduler_primask = 0U;

static bool osal_scheduler_started(void)
{
    return (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED);
}

void osal_critical_enter(void)
{
    if (!osal_scheduler_started()) {
        if (s_pre_scheduler_nest == 0U) {
            s_pre_scheduler_primask = osal_irq_save_and_disable();
        }
        s_pre_scheduler_nest++;
        return;
    }

    taskENTER_CRITICAL();
}

void osal_critical_exit(void)
{
    if (!osal_scheduler_started()) {
        if (s_pre_scheduler_nest > 0U) {
            s_pre_scheduler_nest--;
            if (s_pre_scheduler_nest == 0U) {
                osal_irq_restore(s_pre_scheduler_primask);
            }
        }
        return;
    }

    taskEXIT_CRITICAL();
}

#else /* !USE_FREERTOS */

uint32_t osal_critical_enter_raw(void)
{
    return osal_irq_save_and_disable();
}

void osal_critical_exit_raw(uint32_t primask)
{
    osal_irq_restore(primask);
}

/** Bare-metal critical-section nesting counter. */
static uint32_t s_nest_count = 0U;
/** Saved PRIMASK before the outermost bare-metal critical section. */
static uint32_t s_primask = 0U;

void osal_critical_enter(void)
{
    if (s_nest_count == 0U) {
        s_primask = osal_critical_enter_raw();
    }
    s_nest_count++;
}

void osal_critical_exit(void)
{
    if (s_nest_count > 0U) {
        s_nest_count--;
        if (s_nest_count == 0U) {
            osal_critical_exit_raw(s_primask);
        }
    }
}

#endif /* USE_FREERTOS */