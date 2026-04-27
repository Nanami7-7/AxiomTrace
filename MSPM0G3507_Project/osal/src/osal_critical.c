/**
 * @file    osal_critical.c
 * @brief   OSAL critical section implementation (wraps FreeRTOS taskENTER_CRITICAL)
 */

#include "osal_conf.h"

#ifdef OSAL_CRITICAL_ENABLED

#include "osal_critical.h"
#include "FreeRTOS.h"
#include "task.h"

void osal_enter_critical(void)
{
    taskENTER_CRITICAL();
}

void osal_exit_critical(void)
{
    taskEXIT_CRITICAL();
}

uint32_t osal_enter_critical_from_isr(void)
{
    UBaseType_t mask = taskENTER_CRITICAL_FROM_ISR();
    return (uint32_t)mask;
}

void osal_exit_critical_from_isr(uint32_t prev_mask)
{
    taskEXIT_CRITICAL_FROM_ISR((UBaseType_t)prev_mask);
}

#endif /* OSAL_CRITICAL_ENABLED */
