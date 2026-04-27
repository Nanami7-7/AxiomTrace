/**
 * @file    osal_common.h
 * @brief   OSAL common type definitions (handles, status, priorities)
 */

#ifndef OSAL_COMMON_H
#define OSAL_COMMON_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>

/* ---------- OSAL handle types (opaque wrappers around FreeRTOS handles) ---------- */
typedef void* osal_task_handle_t;
typedef void* osal_semaphore_handle_t;
typedef void* osal_mutex_handle_t;
typedef void* osal_queue_handle_t;
typedef void* osal_timer_handle_t;

/* ---------- OSAL status codes ---------- */
typedef enum {
    OSAL_OK       = 0,
    OSAL_ERROR    = -1,
    OSAL_TIMEOUT  = -2,
    OSAL_INVALID  = -3,
    OSAL_BUSY     = -4,
} osal_status_t;

/* ---------- Task priority levels ---------- */
typedef enum {
    OSAL_TASK_PRIORITY_IDLE    = 0,
    OSAL_TASK_PRIORITY_LOW     = 1,
    OSAL_TASK_PRIORITY_BELOW   = 2,
    OSAL_TASK_PRIORITY_NORMAL  = 3,
    OSAL_TASK_PRIORITY_ABOVE   = 4,
    OSAL_TASK_PRIORITY_HIGH    = 5,
    OSAL_TASK_PRIORITY_HIGHEST = 6,
} osal_task_priority_t;

/* ---------- Timeout constants ---------- */
#define OSAL_WAIT_FOREVER   ((uint32_t)0xFFFFFFFFUL)
#define OSAL_NO_WAIT        ((uint32_t)0)

/* ---------- Tick type ---------- */
typedef uint32_t osal_tick_t;

#ifdef __cplusplus
}
#endif

#endif /* OSAL_COMMON_H */
