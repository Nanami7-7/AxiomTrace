/**
 * @file    osal_sem.c
 * @brief   OSAL信号量实现
 * @note    根据USE_FREERTOS宏条件编译:
 *          - FreeRTOS模式: 封装xSemaphoreCreateCounting/xSemaphoreTake等
 *          - 裸机模式: volatile计数+关中断保护
 */
#include "osal_api.h"

#if USE_FREERTOS
/* ======================== FreeRTOS模式实现 ======================== */

osal_sem_t osal_sem_create(uint32_t init_count, uint32_t max_count)
{
    return xSemaphoreCreateCounting((UBaseType_t)max_count,
        (UBaseType_t)init_count);
}

void osal_sem_delete(osal_sem_t sem)
{
    if (NULL != sem) {
        vSemaphoreDelete(sem);
    }
}

osal_status_t osal_sem_wait(osal_sem_t sem, uint32_t timeout_ms)
{
    if (NULL == sem) {
        return OSAL_ERR_INVALID_PARAM;
    }

    TickType_t ticks;
    if (OSAL_WAIT_FOREVER == timeout_ms) {
        ticks = portMAX_DELAY;
    } else {
        ticks = pdMS_TO_TICKS(timeout_ms);
    }

    if (pdTRUE == xSemaphoreTake(sem, ticks)) {
        return OSAL_OK;
    }

    return OSAL_ERR_TIMEOUT;
}

void osal_sem_release(osal_sem_t sem)
{
    if (NULL != sem) {
        /* 任务上下文释放信号量 */
        (void)xSemaphoreGive(sem);
    }
}

void osal_sem_release_from_isr(osal_sem_t sem)
{
    if (NULL != sem) {
        BaseType_t higher_priority_task_woken = pdFALSE;

        /* ISR上下文释放信号量 */
        (void)xSemaphoreGiveFromISR(sem, &higher_priority_task_woken);

        /* 如果释放了高优先级任务，请求上下文切换 */
        portYIELD_FROM_ISR(higher_priority_task_woken);
    }
}

#else /* !USE_FREERTOS */
/* ======================== 裸机模式实现 ======================== */

osal_sem_t osal_sem_create(uint32_t init_count, uint32_t max_count)
{
    /* 裸机模式: 需要调用者提供静态存储 */
    (void)init_count;
    (void)max_count;
    return NULL;
}

void osal_sem_delete(osal_sem_t sem)
{
    (void)sem;
}

osal_status_t osal_sem_wait(osal_sem_t sem, uint32_t timeout_ms)
{
    (void)timeout_ms;

    if (NULL == sem) {
        return OSAL_ERR_INVALID_PARAM;
    }

    /* 裸机模式: 关中断检查计数 */
    uint32_t primask = osal_critical_enter_raw();

    if (sem->count > 0) {
        sem->count--;
        osal_critical_exit_raw(primask);
        return OSAL_OK;
    }

    osal_critical_exit_raw(primask);

    /* 裸机模式不支持阻塞等待，立即返回超时 */
    return OSAL_ERR_TIMEOUT;
}

void osal_sem_release(osal_sem_t sem)
{
    if (NULL == sem) {
        return;
    }

    uint32_t primask = osal_critical_enter_raw();

    if (sem->count < sem->max_count) {
        sem->count++;
    }

    osal_critical_exit_raw(primask);
}

void osal_sem_release_from_isr(osal_sem_t sem)
{
    /* 裸机模式: ISR中直接操作，已经在中断中无需关中断 */
    if ((NULL != sem) && (sem->count < sem->max_count)) {
        sem->count++;
    }
}

#endif /* USE_FREERTOS */
