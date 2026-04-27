/**
 * @file    osal_mutex.c
 * @brief   OSAL mutex implementation (wraps FreeRTOS mutex API)
 */

#include "osal_conf.h"

#ifdef OSAL_MUTEX_ENABLED

#include "osal_mutex.h"
#include "FreeRTOS.h"
#include "semphr.h"

osal_status_t osal_mutex_create(osal_mutex_handle_t *handle)
{
    if (handle == NULL) {
        return OSAL_INVALID;
    }

    SemaphoreHandle_t mutex = xSemaphoreCreateMutex();
    if (mutex == NULL) {
        return OSAL_ERROR;
    }

    *handle = (osal_mutex_handle_t)mutex;
    return OSAL_OK;
}

osal_status_t osal_mutex_delete(osal_mutex_handle_t handle)
{
    if (handle == NULL) {
        return OSAL_INVALID;
    }

    vSemaphoreDelete((SemaphoreHandle_t)handle);
    return OSAL_OK;
}

osal_status_t osal_mutex_take(osal_mutex_handle_t handle, uint32_t timeout_ms)
{
    if (handle == NULL) {
        return OSAL_INVALID;
    }

    TickType_t ticks = (timeout_ms == OSAL_WAIT_FOREVER) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
    BaseType_t result = xSemaphoreTake((SemaphoreHandle_t)handle, ticks);

    return (result == pdTRUE) ? OSAL_OK : OSAL_TIMEOUT;
}

osal_status_t osal_mutex_give(osal_mutex_handle_t handle)
{
    if (handle == NULL) {
        return OSAL_INVALID;
    }

    BaseType_t result = xSemaphoreGive((SemaphoreHandle_t)handle);
    return (result == pdTRUE) ? OSAL_OK : OSAL_ERROR;
}

osal_status_t osal_mutex_create_recursive(osal_mutex_handle_t *handle)
{
    if (handle == NULL) {
        return OSAL_INVALID;
    }

    SemaphoreHandle_t mutex = xSemaphoreCreateRecursiveMutex();
    if (mutex == NULL) {
        return OSAL_ERROR;
    }

    *handle = (osal_mutex_handle_t)mutex;
    return OSAL_OK;
}

osal_status_t osal_mutex_take_recursive(osal_mutex_handle_t handle, uint32_t timeout_ms)
{
    if (handle == NULL) {
        return OSAL_INVALID;
    }

    TickType_t ticks = (timeout_ms == OSAL_WAIT_FOREVER) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
    BaseType_t result = xSemaphoreTakeRecursive((SemaphoreHandle_t)handle, ticks);

    return (result == pdTRUE) ? OSAL_OK : OSAL_TIMEOUT;
}

osal_status_t osal_mutex_give_recursive(osal_mutex_handle_t handle)
{
    if (handle == NULL) {
        return OSAL_INVALID;
    }

    BaseType_t result = xSemaphoreGiveRecursive((SemaphoreHandle_t)handle);
    return (result == pdTRUE) ? OSAL_OK : OSAL_ERROR;
}

#endif /* OSAL_MUTEX_ENABLED */
