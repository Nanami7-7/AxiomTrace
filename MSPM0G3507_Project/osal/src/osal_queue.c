/**
 * @file    osal_queue.c
 * @brief   OSAL message queue implementation (wraps FreeRTOS queue API)
 */

#include "osal_conf.h"

#ifdef OSAL_QUEUE_ENABLED

#include "osal_queue.h"
#include "FreeRTOS.h"
#include "queue.h"

osal_status_t osal_queue_create(osal_queue_handle_t *handle, uint32_t length, uint32_t item_size)
{
    if (handle == NULL) {
        return OSAL_INVALID;
    }

    QueueHandle_t queue = xQueueCreate((UBaseType_t)length, (UBaseType_t)item_size);
    if (queue == NULL) {
        return OSAL_ERROR;
    }

    *handle = (osal_queue_handle_t)queue;
    return OSAL_OK;
}

osal_status_t osal_queue_delete(osal_queue_handle_t handle)
{
    if (handle == NULL) {
        return OSAL_INVALID;
    }

    vQueueDelete((QueueHandle_t)handle);
    return OSAL_OK;
}

osal_status_t osal_queue_send(osal_queue_handle_t handle, const void *item, uint32_t timeout_ms)
{
    if (handle == NULL || item == NULL) {
        return OSAL_INVALID;
    }

    TickType_t ticks = (timeout_ms == OSAL_WAIT_FOREVER) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
    BaseType_t result = xQueueSend((QueueHandle_t)handle, item, ticks);

    return (result == pdTRUE) ? OSAL_OK : OSAL_TIMEOUT;
}

osal_status_t osal_queue_receive(osal_queue_handle_t handle, void *item, uint32_t timeout_ms)
{
    if (handle == NULL || item == NULL) {
        return OSAL_INVALID;
    }

    TickType_t ticks = (timeout_ms == OSAL_WAIT_FOREVER) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
    BaseType_t result = xQueueReceive((QueueHandle_t)handle, item, ticks);

    return (result == pdTRUE) ? OSAL_OK : OSAL_TIMEOUT;
}

osal_status_t osal_queue_send_to_front(osal_queue_handle_t handle, const void *item, uint32_t timeout_ms)
{
    if (handle == NULL || item == NULL) {
        return OSAL_INVALID;
    }

    TickType_t ticks = (timeout_ms == OSAL_WAIT_FOREVER) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
    BaseType_t result = xQueueSendToFront((QueueHandle_t)handle, item, ticks);

    return (result == pdTRUE) ? OSAL_OK : OSAL_TIMEOUT;
}

osal_status_t osal_queue_send_from_isr(osal_queue_handle_t handle, const void *item, int32_t *higher_priority_task_woken)
{
    if (handle == NULL || item == NULL) {
        return OSAL_INVALID;
    }

    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    BaseType_t result = xQueueSendFromISR((QueueHandle_t)handle, item, &xHigherPriorityTaskWoken);

    if (higher_priority_task_woken != NULL) {
        *higher_priority_task_woken = (int32_t)xHigherPriorityTaskWoken;
    }

    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);

    return (result == pdTRUE) ? OSAL_OK : OSAL_ERROR;
}

osal_status_t osal_queue_receive_from_isr(osal_queue_handle_t handle, void *item, int32_t *higher_priority_task_woken)
{
    if (handle == NULL || item == NULL) {
        return OSAL_INVALID;
    }

    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    BaseType_t result = xQueueReceiveFromISR((QueueHandle_t)handle, item, &xHigherPriorityTaskWoken);

    if (higher_priority_task_woken != NULL) {
        *higher_priority_task_woken = (int32_t)xHigherPriorityTaskWoken;
    }

    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);

    return (result == pdTRUE) ? OSAL_OK : OSAL_TIMEOUT;
}

uint32_t osal_queue_messages_waiting(osal_queue_handle_t handle)
{
    if (handle == NULL) {
        return 0;
    }
    return (uint32_t)uxQueueMessagesWaiting((QueueHandle_t)handle);
}

uint32_t osal_queue_spaces_available(osal_queue_handle_t handle)
{
    if (handle == NULL) {
        return 0;
    }
    return (uint32_t)uxQueueSpacesAvailable((QueueHandle_t)handle);
}

#endif /* OSAL_QUEUE_ENABLED */
