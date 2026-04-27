/**
 * @file    osal_task.h
 * @brief   OSAL task management interface
 */

#ifndef OSAL_TASK_H
#define OSAL_TASK_H

#ifdef __cplusplus
extern "C" {
#endif

#include "osal_common.h"
#include "osal_conf.h"

/* ---------- Task function signature ---------- */
typedef void (*osal_task_function_t)(void *param);

/* ---------- Task configuration ---------- */
typedef struct {
    const char           *name;
    osal_task_function_t  function;
    void                 *param;
    uint32_t              stack_size;    /* Words (4 bytes each) */
    osal_task_priority_t  priority;
} osal_task_config_t;

/* ========== API ========== */

osal_status_t osal_task_create(const osal_task_config_t *config, osal_task_handle_t *handle);
osal_status_t osal_task_delete(osal_task_handle_t handle);
osal_task_handle_t osal_task_get_current_handle(void);
osal_status_t osal_task_suspend(osal_task_handle_t handle);
osal_status_t osal_task_resume(osal_task_handle_t handle);
void osal_task_yield(void);

/**
 * @brief  Start the OS scheduler (never returns on success)
 * @note   Wrapper for vTaskStartScheduler() - this should be the last call in main()
 */
void osal_start_scheduler(void);

#ifdef __cplusplus
}
#endif

#endif /* OSAL_TASK_H */
