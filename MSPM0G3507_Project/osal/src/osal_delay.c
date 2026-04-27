/**
 * @file    osal_delay.c
 * @brief   OSAL delay implementation (wraps FreeRTOS vTaskDelay)
 */

#include "osal_conf.h"

#ifdef OSAL_DELAY_ENABLED

#include "osal_delay.h"
#include "FreeRTOS.h"
#include "task.h"

void osal_delay_ms(uint32_t ms)
{
    if (ms == 0) {
        taskYIELD();
        return;
    }
    vTaskDelay(pdMS_TO_TICKS(ms));
}

void osal_delay_ticks(osal_tick_t ticks)
{
    vTaskDelay((TickType_t)ticks);
}

void osal_delay_until(osal_tick_t *prev_tick, uint32_t increment_ms)
{
    if (prev_tick == NULL) {
        return;
    }
    TickType_t tick = (TickType_t)*prev_tick;
    vTaskDelayUntil(&tick, pdMS_TO_TICKS(increment_ms));
    *prev_tick = (osal_tick_t)tick;
}

osal_tick_t osal_get_tick(void)
{
    return (osal_tick_t)xTaskGetTickCount();
}

uint32_t osal_get_tick_freq_hz(void)
{
    return (uint32_t)configTICK_RATE_HZ;
}

#endif /* OSAL_DELAY_ENABLED */
