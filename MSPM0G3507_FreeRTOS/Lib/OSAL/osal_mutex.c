/**
 * @file    osal_mutex.c
 * @brief   OSAL互斥锁实现
 * @note    根据USE_FREERTOS宏条件编译:
 *          - FreeRTOS模式: 封装xSemaphoreCreateMutex/xSemaphoreTake等
 *          - 裸机模式: 关中断/恢复中断实现临界区保护
 */
#include "osal_api.h"

#if USE_FREERTOS
/* ======================== FreeRTOS模式实现 ======================== */

osal_mutex_t osal_mutex_create(void)
{
    return xSemaphoreCreateMutex();
}

void osal_mutex_delete(osal_mutex_t mutex)
{
    if (NULL != mutex) {
        vSemaphoreDelete(mutex);
    }
}

void osal_mutex_lock(osal_mutex_t mutex)
{
    if (NULL != mutex) {
        /* 永久等待直到获取互斥锁 */
        (void)xSemaphoreTake(mutex, portMAX_DELAY);
    }
}

void osal_mutex_unlock(osal_mutex_t mutex)
{
    if (NULL != mutex) {
        (void)xSemaphoreGive(mutex);
    }
}

#else /* !USE_FREERTOS */
/* ======================== 裸机模式实现 ======================== */

osal_mutex_t osal_mutex_create(void)
{
    /* 裸机模式: 需要调用者提供静态存储 */
    return NULL;
}

void osal_mutex_delete(osal_mutex_t mutex)
{
    (void)mutex;
}

void osal_mutex_lock(osal_mutex_t mutex)
{
    if (NULL != mutex) {
        /* 裸机模式: 关中断实现互斥 */
        mutex->int_mask = osal_critical_enter_raw();
        mutex->count++;
    } else {
        /* 无句柄时也关中断保护 */
        (void)osal_critical_enter();
    }
}

void osal_mutex_unlock(osal_mutex_t mutex)
{
    if (NULL != mutex) {
        mutex->count--;
        if (0 >= mutex->count) {
            mutex->count = 0;
            osal_critical_exit_raw(mutex->int_mask);
        }
    } else {
        (void)osal_critical_exit();
    }
}

#endif /* USE_FREERTOS */
