/**
 * @file    osal_semaphore.h
 * @brief   OSAL semaphore interface
 */

#ifndef OSAL_SEMAPHORE_H
#define OSAL_SEMAPHORE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "osal_common.h"

/* ========== API ========== */

osal_status_t osal_semaphore_create_binary(osal_semaphore_handle_t *handle);
osal_status_t osal_semaphore_create_counting(osal_semaphore_handle_t *handle, uint32_t max_count, uint32_t init_count);
osal_status_t osal_semaphore_delete(osal_semaphore_handle_t handle);

osal_status_t osal_semaphore_take(osal_semaphore_handle_t handle, uint32_t timeout_ms);
osal_status_t osal_semaphore_give(osal_semaphore_handle_t handle);

/* ISR-safe variants */
osal_status_t osal_semaphore_take_from_isr(osal_semaphore_handle_t handle, int32_t *higher_priority_task_woken);
osal_status_t osal_semaphore_give_from_isr(osal_semaphore_handle_t handle, int32_t *higher_priority_task_woken);

#ifdef __cplusplus
}
#endif

#endif /* OSAL_SEMAPHORE_H */
