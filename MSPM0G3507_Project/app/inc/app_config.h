/**
 * @file    app_config.h
 * @brief   Application configuration (task parameters, feature switches)
 */

#ifndef APP_CONFIG_H
#define APP_CONFIG_H

/* ========== Task Stack Sizes (in words = 4 bytes each) ========== */
#define APP_LED_TASK_STACK_SIZE       128
#define APP_MONITOR_TASK_STACK_SIZE   256
#define APP_UART_TASK_STACK_SIZE      256

/* ========== Task Priorities ========== */
#define APP_LED_TASK_PRIORITY         OSAL_TASK_PRIORITY_LOW
#define APP_MONITOR_TASK_PRIORITY     OSAL_TASK_PRIORITY_NORMAL
#define APP_UART_TASK_PRIORITY        OSAL_TASK_PRIORITY_ABOVE

/* ========== Task Periods ========== */
#define APP_LED_BLINK_PERIOD_MS       500
#define APP_MONITOR_PERIOD_MS         5000
#define APP_UART_RX_TIMEOUT_MS        100

/* ========== Feature Switches ========== */
#define APP_ENABLE_LED_TASK           1
#define APP_ENABLE_MONITOR_TASK       1
#define APP_ENABLE_UART_TASK          1

/* ========== Message Queue ========== */
#define APP_UART_RX_QUEUE_LENGTH      32
#define APP_UART_RX_ITEM_SIZE         sizeof(uint8_t)

#endif /* APP_CONFIG_H */
