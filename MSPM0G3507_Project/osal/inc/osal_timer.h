/**
 * @file    osal_timer.h
 * @brief   OSAL software timer interface
 */

#ifndef OSAL_TIMER_H
#define OSAL_TIMER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "osal_common.h"

/* ---------- Timer mode ---------- */
typedef enum {
    OSAL_TIMER_MODE_ONESHOT  = 0,
    OSAL_TIMER_MODE_PERIODIC = 1,
} osal_timer_mode_t;

/* ---------- Timer callback ---------- */
typedef void (*osal_timer_cb_t)(osal_timer_handle_t handle);

/* ---------- Timer configuration ---------- */
typedef struct {
    const char       *name;
    osal_timer_mode_t mode;
    uint32_t          period_ms;
    void             *param;
    osal_timer_cb_t   callback;
} osal_timer_config_t;

/* ========== API ========== */

osal_status_t osal_timer_create(const osal_timer_config_t *config, osal_timer_handle_t *handle);
osal_status_t osal_timer_delete(osal_timer_handle_t handle, uint32_t timeout_ms);

osal_status_t osal_timer_start(osal_timer_handle_t handle, uint32_t timeout_ms);
osal_status_t osal_timer_stop(osal_timer_handle_t handle, uint32_t timeout_ms);
osal_status_t osal_timer_reset(osal_timer_handle_t handle, uint32_t timeout_ms);

osal_status_t osal_timer_change_period(osal_timer_handle_t handle, uint32_t new_period_ms, uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif

#endif /* OSAL_TIMER_H */
