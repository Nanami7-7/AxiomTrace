/**
 * @file    osal_mutex.h
 * @brief   OSAL mutex interface
 */

#ifndef OSAL_MUTEX_H
#define OSAL_MUTEX_H

#ifdef __cplusplus
extern "C" {
#endif

#include "osal_common.h"

/* ========== API ========== */

osal_status_t osal_mutex_create(osal_mutex_handle_t *handle);
osal_status_t osal_mutex_delete(osal_mutex_handle_t handle);

osal_status_t osal_mutex_take(osal_mutex_handle_t handle, uint32_t timeout_ms);
osal_status_t osal_mutex_give(osal_mutex_handle_t handle);

/* Recursive mutex */
osal_status_t osal_mutex_create_recursive(osal_mutex_handle_t *handle);
osal_status_t osal_mutex_take_recursive(osal_mutex_handle_t handle, uint32_t timeout_ms);
osal_status_t osal_mutex_give_recursive(osal_mutex_handle_t handle);

#ifdef __cplusplus
}
#endif

#endif /* OSAL_MUTEX_H */
