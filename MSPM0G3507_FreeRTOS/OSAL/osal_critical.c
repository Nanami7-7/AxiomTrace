/**
 * @file    osal_critical.c
 * @brief   OSAL临界区实现
 * @note    根据USE_FREERTOS宏条件编译:
 *          - FreeRTOS模式: taskENTER_CRITICAL/taskEXIT_CRITICAL
 *          - 裸机模式: __disable_irq/__enable_irq
 *          同时提供裸机模式下的osal_critical_enter_raw/exit_raw
 *          供osal_mutex/osal_sem的裸机实现使用
 */
#include "osal_api.h"

/* ======================== 原始中断操作(裸机模式专用) ======================== */

#if !USE_FREERTOS

/**
 * @brief  关中断并返回之前的中断状态
 * @retval 之前的中断状态(PRIMASK值)
 * @note   裸机模式下供osal_mutex/osal_sem使用
 */
uint32_t osal_critical_enter_raw(void)
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

/**
 * @brief  恢复中断状态
 * @param  primask 之前保存的PRIMASK值
 */
void osal_critical_exit_raw(uint32_t primask)
{
    __asm volatile (
        "msr   primask, %0  \n"
        :
        : "r" (primask)
        : "memory"
    );
}

#endif /* !USE_FREERTOS */

/* ======================== 公共临界区接口 ======================== */

#if USE_FREERTOS

void osal_critical_enter(void)
{
    taskENTER_CRITICAL();
}

void osal_critical_exit(void)
{
    taskEXIT_CRITICAL();
}

#else /* !USE_FREERTOS */

/** 保存裸机临界区进入前的PRIMASK */
static uint32_t s_primask = 0U;

void osal_critical_enter(void)
{
    s_primask = osal_critical_enter_raw();
}

void osal_critical_exit(void)
{
    osal_critical_exit_raw(s_primask);
}

#endif /* USE_FREERTOS */
