/**
 * @file    osal_task.c
 * @brief   OSAL task management implementation (wraps FreeRTOS xTaskCreate)
 */

#include "osal_conf.h"

#ifdef OSAL_TASK_ENABLED

#include "osal_task.h"
#include "FreeRTOS.h"
#include "task.h"

osal_status_t osal_task_create(const osal_task_config_t *config, osal_task_handle_t *handle)
{
    if (config == NULL || handle == NULL) {
        return OSAL_INVALID;
    }

    BaseType_t result = xTaskCreate(
        (TaskFunction_t)config->function,
        config->name,
        (uint16_t)config->stack_size,
        config->param,
        (UBaseType_t)config->priority,
        (TaskHandle_t *)handle
    );

    return (result == pdPASS) ? OSAL_OK : OSAL_ERROR;
}

osal_status_t osal_task_delete(osal_task_handle_t handle)
{
    vTaskDelete((TaskHandle_t)handle);
    return OSAL_OK;
}

osal_task_handle_t osal_task_get_current_handle(void)
{
    return (osal_task_handle_t)xTaskGetCurrentTaskHandle();
}

osal_status_t osal_task_suspend(osal_task_handle_t handle)
{
    vTaskSuspend((TaskHandle_t)handle);
    return OSAL_OK;
}

osal_status_t osal_task_resume(osal_task_handle_t handle)
{
    vTaskResume((TaskHandle_t)handle);
    return OSAL_OK;
}

void osal_task_yield(void)
{
    taskYIELD();
}

void osal_start_scheduler(void)
{
    vTaskStartScheduler();
    /* Should never reach here unless heap exhausted */
    for (;;) {}
}

#endif /* OSAL_TASK_ENABLED */
