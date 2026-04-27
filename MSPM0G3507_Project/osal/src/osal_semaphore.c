/**
 * @file    osal_semaphore.c
 * @brief   OSAL semaphore implementation (wraps FreeRTOS semaphore API)
 */

#include "osal_conf.h"

#ifdef OSAL_SEMAPHORE_ENABLED

#include "osal_semaphore.h"
#include "FreeRTOS.h"
#include "semphr.h"

osal_status_t osal_semaphore_create_binary(osal_semaphore_handle_t *handle)
{
    if (handle == NULL) {
        return OSAL_INVALID;
    }

    SemaphoreHandle_t sem = xSemaphoreCreateBinary();
    if (sem == NULL) {
        return OSAL_ERROR;
    }

    *handle = (osal_semaphore_handle_t)sem;
    return OSAL_OK;
}

osal_status_t osal_semaphore_create_counting(osal_semaphore_handle_t *handle, uint32_t max_count, uint32_t init_count)
{
    if (handle == NULL) {
        return OSAL_INVALID;
    }

    SemaphoreHandle_t sem = xSemaphoreCreateCounting((UBaseType_t)max_count, (UBaseType_t)init_count);
    if (sem == NULL) {
        return OSAL_ERROR;
    }

    *handle = (osal_semaphore_handle_t)sem;
    return OSAL_OK;
}

osal_status_t osal_semaphore_delete(osal_semaphore_handle_t handle)
{
    if (handle == NULL) {
        return OSAL_INVALID;
    }

    vSemaphoreDelete((SemaphoreHandle_t)handle);
    return OSAL_OK;
}

osal_status_t osal_semaphore_take(osal_semaphore_handle_t handle, uint32_t timeout_ms)
{
    if (handle == NULL) {
        return OSAL_INVALID;
    }

    TickType_t ticks = (timeout_ms == OSAL_WAIT_FOREVER) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
    BaseType_t result = xSemaphoreTake((SemaphoreHandle_t)handle, ticks);

    return (result == pdTRUE) ? OSAL_OK : OSAL_TIMEOUT;
}

osal_status_t osal_semaphore_give(osal_semaphore_handle_t handle)
{
    if (handle == NULL) {
        return OSAL_INVALID;
    }

    BaseType_t result = xSemaphoreGive((SemaphoreHandle_t)handle);
    return (result == pdTRUE) ? OSAL_OK : OSAL_ERROR;
}

osal_status_t osal_semaphore_take_from_isr(osal_semaphore_handle_t handle, int32_t *higher_priority_task_woken)
{
    if (handle == NULL) {
        return OSAL_INVALID;
    }

    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    BaseType_t result = xSemaphoreTakeFromISR((SemaphoreHandle_t)handle, &xHigherPriorityTaskWoken);

    if (higher_priority_task_woken != NULL) {
        *higher_priority_task_woken = (int32_t)xHigherPriorityTaskWoken;
    }

    return (result == pdTRUE) ? OSAL_OK : OSAL_TIMEOUT;
}

osal_status_t osal_semaphore_give_from_isr(osal_semaphore_handle_t handle, int32_t *higher_priority_task_woken)
{
    if (handle == NULL) {
        return OSAL_INVALID;
    }

    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    BaseType_t result = xSemaphoreGiveFromISR((SemaphoreHandle_t)handle, &xHigherPriorityTaskWoken);

    if (higher_priority_task_woken != NULL) {
        *higher_priority_task_woken = (int32_t)xHigherPriorityTaskWoken;
    }

    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);

    return (result == pdTRUE) ? OSAL_OK : OSAL_ERROR;
}

#endif /* OSAL_SEMAPHORE_ENABLED */
