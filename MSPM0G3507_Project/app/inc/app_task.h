/**
 * @file    app_task.h
 * @brief   Application task interface declarations
 */

#ifndef APP_TASK_H
#define APP_TASK_H

#ifdef __cplusplus
extern "C" {
#endif

#include "osal_common.h"

/**
 * @brief  Create all application tasks
 * @return OSAL_OK on success
 */
osal_status_t app_task_create_all(void);

/* ---------- Individual task entry functions ---------- */
void app_led_task(void *param);
void app_monitor_task(void *param);
void app_uart_task(void *param);

#ifdef __cplusplus
}
#endif

#endif /* APP_TASK_H */
