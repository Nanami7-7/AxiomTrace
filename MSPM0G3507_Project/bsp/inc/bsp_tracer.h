/**
 * @file    bsp_tracer.h
 * @brief   Line tracer module interface (multi-channel GPIO IR sensors)
 * @note    Each channel: LOW = black line detected, HIGH = white ground
 */

#ifndef BSP_TRACER_H
#define BSP_TRACER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "hal_common.h"

/* ---------- Tracer channel IDs ---------- */
typedef enum {
    BSP_TRACER_CH0 = 0,
    BSP_TRACER_CH1 = 1,
    BSP_TRACER_CH2 = 2,
    BSP_TRACER_CH3 = 3,
    BSP_TRACER_CH4 = 4,
    BSP_TRACER_CH5 = 5,
    BSP_TRACER_CH6 = 6,
    BSP_TRACER_CH7 = 7,
    BSP_TRACER_CH_MAX,
} bsp_tracer_ch_t;

/* ========== API ========== */

/**
 * @brief  Initialize all tracer IR sensor GPIO pins (input with pull-up)
 */
hal_status_t bsp_tracer_init(void);

/**
 * @brief  Deinitialize tracer GPIO pins
 */
hal_status_t bsp_tracer_deinit(void);

/**
 * @brief  Read a single channel state
 * @param  channel  Sensor channel (CH0 ~ CH3)
 * @return 1 = black line detected, 0 = white ground
 */
uint8_t bsp_tracer_read_channel(bsp_tracer_ch_t channel);

/**
 * @brief  Read all channels at once as a bitmask
 * @return Bitmask: bit0=CH0, bit1=CH1, ..., 1=black line, 0=white ground
 */
uint8_t bsp_tracer_read_all(void);

#ifdef __cplusplus
}
#endif

#endif /* BSP_TRACER_H */
