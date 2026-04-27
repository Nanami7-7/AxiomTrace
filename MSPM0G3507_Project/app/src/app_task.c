/**
 * @file    app_task.c
 * @brief   Application task implementations
 */

#include "app_task.h"
#include "app_config.h"
#include "bsp_gpio.h"
#include "bsp_uart.h"
#include "bsp_adc.h"
#include "osal_task.h"
#include "osal_delay.h"
#include "osal_queue.h"

/* ---------- Shared queue handle for UART RX ---------- */
static osal_queue_handle_t s_uart_rx_queue;

/* ========== Task creation ========== */

osal_status_t app_task_create_all(void)
{
    osal_status_t status;
    osal_task_handle_t handle;

#if APP_ENABLE_LED_TASK
    {
        osal_task_config_t cfg = {
            .name       = "LED",
            .function   = app_led_task,
            .param      = NULL,
            .stack_size = APP_LED_TASK_STACK_SIZE,
            .priority   = APP_LED_TASK_PRIORITY,
        };
        status = osal_task_create(&cfg, &handle);
        if (status != OSAL_OK) return status;
    }
#endif

#if APP_ENABLE_MONITOR_TASK
    {
        osal_task_config_t cfg = {
            .name       = "Monitor",
            .function   = app_monitor_task,
            .param      = NULL,
            .stack_size = APP_MONITOR_TASK_STACK_SIZE,
            .priority   = APP_MONITOR_TASK_PRIORITY,
        };
        status = osal_task_create(&cfg, &handle);
        if (status != OSAL_OK) return status;
    }
#endif

#if APP_ENABLE_UART_TASK
    {
        /* Create UART RX queue */
        status = osal_queue_create(&s_uart_rx_queue,
                                   APP_UART_RX_QUEUE_LENGTH,
                                   APP_UART_RX_ITEM_SIZE);
        if (status != OSAL_OK) return status;

        osal_task_config_t cfg = {
            .name       = "UART",
            .function   = app_uart_task,
            .param      = NULL,
            .stack_size = APP_UART_TASK_STACK_SIZE,
            .priority   = APP_UART_TASK_PRIORITY,
        };
        status = osal_task_create(&cfg, &handle);
        if (status != OSAL_OK) return status;
    }
#endif

    return OSAL_OK;
}

/* ========== LED blink task ========== */

void app_led_task(void *param)
{
    (void)param;

    for (;;) {
        bsp_led_toggle(BSP_LED_1);
        osal_delay_ms(APP_LED_BLINK_PERIOD_MS);
    }
}

/* ========== System monitor task ========== */

void app_monitor_task(void *param)
{
    (void)param;

    for (;;) {
        /* Read internal temperature */
        int32_t temp;
        if (bsp_adc_read_temperature(BSP_ADC_CH_INTERNAL_TEMP, &temp, 100) == HAL_OK) {
            bsp_debug_printf("[Monitor] Temp: %d.%d C\r\n",
                             (int)(temp / 10), (int)(temp % 10));
        }

        /* Read button state */
        uint8_t key1 = bsp_key_read(BSP_KEY_1);
        if (key1) {
            bsp_debug_printf("[Monitor] KEY1 pressed\r\n");
        }

        osal_delay_ms(APP_MONITOR_PERIOD_MS);
    }
}

/* ========== UART echo task ========== */

void app_uart_task(void *param)
{
    (void)param;

    bsp_debug_printf("\r\n=== MSPM0G3507 FreeRTOS Demo ===\r\n");
    bsp_debug_printf("Type any character for echo:\r\n");

    for (;;) {
        uint8_t byte;
        if (bsp_uart_receive(&byte, 1, APP_UART_RX_TIMEOUT_MS) == HAL_OK) {
            /* Echo back */
            bsp_uart_send(&byte, 1);
        }

        osal_task_yield();
    }
}
