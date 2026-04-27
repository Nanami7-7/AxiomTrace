/**
 * @file    osal_critical.h
 * @brief   OSAL critical section interface
 */

#ifndef OSAL_CRITICAL_H
#define OSAL_CRITICAL_H

#ifdef __cplusplus
extern "C" {
#endif

#include "osal_common.h"

/* ========== API ========== */

/**
 * @brief  Enter critical section (disable interrupts / take scheduler lock)
 */
void osal_enter_critical(void);

/**
 * @brief  Exit critical section (restore interrupts / release scheduler lock)
 */
void osal_exit_critical(void);

/**
 * @brief  Enter critical section from ISR (returns previous interrupt mask)
 */
uint32_t osal_enter_critical_from_isr(void);

/**
 * @brief  Exit critical section from ISR (restores interrupt mask)
 */
void osal_exit_critical_from_isr(uint32_t prev_mask);

#ifdef __cplusplus
}
#endif

#endif /* OSAL_CRITICAL_H */
