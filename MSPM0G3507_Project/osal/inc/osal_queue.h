/**
 * @file    osal_queue.h
 * @brief   OSAL message queue interface
 */

#ifndef OSAL_QUEUE_H
#define OSAL_QUEUE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "osal_common.h"

/* ========== API ========== */

osal_status_t osal_queue_create(osal_queue_handle_t *handle, uint32_t length, uint32_t item_size);
osal_status_t osal_queue_delete(osal_queue_handle_t handle);

osal_status_t osal_queue_send(osal_queue_handle_t handle, const void *item, uint32_t timeout_ms);
osal_status_t osal_queue_receive(osal_queue_handle_t handle, void *item, uint32_t timeout_ms);

osal_status_t osal_queue_send_to_front(osal_queue_handle_t handle, const void *item, uint32_t timeout_ms);

/* ISR-safe variants */
osal_status_t osal_queue_send_from_isr(osal_queue_handle_t handle, const void *item, int32_t *higher_priority_task_woken);
osal_status_t osal_queue_receive_from_isr(osal_queue_handle_t handle, void *item, int32_t *higher_priority_task_woken);

uint32_t osal_queue_messages_waiting(osal_queue_handle_t handle);
uint32_t osal_queue_spaces_available(osal_queue_handle_t handle);

#ifdef __cplusplus
}
#endif

#endif /* OSAL_QUEUE_H */
