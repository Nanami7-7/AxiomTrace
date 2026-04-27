/**
 * @file    osal_delay.h
 * @brief   OSAL delay and tick interface
 */

#ifndef OSAL_DELAY_H
#define OSAL_DELAY_H

#ifdef __cplusplus
extern "C" {
#endif

#include "osal_common.h"

/* ========== API ========== */

/**
 * @brief  Delay for specified milliseconds (yields to other tasks)
 */
void osal_delay_ms(uint32_t ms);

/**
 * @brief  Delay for specified ticks
 */
void osal_delay_ticks(osal_tick_t ticks);

/**
 * @brief  Delay until an absolute tick count (for precise periodic loops)
 */
void osal_delay_until(osal_tick_t *prev_tick, uint32_t increment_ms);

/**
 * @brief  Get current system tick count
 */
osal_tick_t osal_get_tick(void);

/**
 * @brief  Get tick frequency in Hz
 */
uint32_t osal_get_tick_freq_hz(void);

#ifdef __cplusplus
}
#endif

#endif /* OSAL_DELAY_H */
