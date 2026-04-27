/**
 * @file    osal_timer.c
 * @brief   OSAL software timer implementation (wraps FreeRTOS timer API)
 */

#include "osal_conf.h"

#ifdef OSAL_TIMER_ENABLED

#include "osal_timer.h"
#include "FreeRTOS.h"
#include "timers.h"

osal_status_t osal_timer_create(const osal_timer_config_t *config, osal_timer_handle_t *handle)
{
    if (config == NULL || handle == NULL) {
        return OSAL_INVALID;
    }

    TickType_t period_ticks = pdMS_TO_TICKS(config->period_ms);

    TimerHandle_t timer = xTimerCreate(
        config->name,
        period_ticks,
        (config->mode == OSAL_TIMER_MODE_PERIODIC) ? pdTRUE : pdFALSE,
        config->param,
        (TimerCallbackFunction_t)config->callback
    );

    if (timer == NULL) {
        return OSAL_ERROR;
    }

    *handle = (osal_timer_handle_t)timer;
    return OSAL_OK;
}

osal_status_t osal_timer_delete(osal_timer_handle_t handle, uint32_t timeout_ms)
{
    if (handle == NULL) {
        return OSAL_INVALID;
    }

    TickType_t ticks = (timeout_ms == OSAL_WAIT_FOREVER) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
    BaseType_t result = xTimerDelete((TimerHandle_t)handle, ticks);

    return (result == pdPASS) ? OSAL_OK : OSAL_ERROR;
}

osal_status_t osal_timer_start(osal_timer_handle_t handle, uint32_t timeout_ms)
{
    if (handle == NULL) {
        return OSAL_INVALID;
    }

    TickType_t ticks = (timeout_ms == OSAL_WAIT_FOREVER) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
    BaseType_t result = xTimerStart((TimerHandle_t)handle, ticks);

    return (result == pdPASS) ? OSAL_OK : OSAL_ERROR;
}

osal_status_t osal_timer_stop(osal_timer_handle_t handle, uint32_t timeout_ms)
{
    if (handle == NULL) {
        return OSAL_INVALID;
    }

    TickType_t ticks = (timeout_ms == OSAL_WAIT_FOREVER) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
    BaseType_t result = xTimerStop((TimerHandle_t)handle, ticks);

    return (result == pdPASS) ? OSAL_OK : OSAL_ERROR;
}

osal_status_t osal_timer_reset(osal_timer_handle_t handle, uint32_t timeout_ms)
{
    if (handle == NULL) {
        return OSAL_INVALID;
    }

    TickType_t ticks = (timeout_ms == OSAL_WAIT_FOREVER) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
    BaseType_t result = xTimerReset((TimerHandle_t)handle, ticks);

    return (result == pdPASS) ? OSAL_OK : OSAL_ERROR;
}

osal_status_t osal_timer_change_period(osal_timer_handle_t handle, uint32_t new_period_ms, uint32_t timeout_ms)
{
    if (handle == NULL) {
        return OSAL_INVALID;
    }

    TickType_t cmd_ticks = (timeout_ms == OSAL_WAIT_FOREVER) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
    TickType_t new_period_ticks = pdMS_TO_TICKS(new_period_ms);
    BaseType_t result = xTimerChangePeriod((TimerHandle_t)handle, new_period_ticks, cmd_ticks);

    return (result == pdPASS) ? OSAL_OK : OSAL_ERROR;
}

#endif /* OSAL_TIMER_ENABLED */
