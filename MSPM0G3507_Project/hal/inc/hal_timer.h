/**
 * @file    hal_timer.h
 * @brief   Timer hardware abstraction layer interface
 */

#ifndef HAL_TIMER_H
#define HAL_TIMER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "hal_common.h"

/* ---------- Timer mode ---------- */
typedef enum {
    HAL_TIMER_MODE_ONESHOT   = 0,
    HAL_TIMER_MODE_PERIODIC  = 1,
} hal_timer_mode_t;

/* ---------- Timer clock source ---------- */
typedef enum {
    HAL_TIMER_CLK_BUSCLK  = 0,
    HAL_TIMER_CLK_LFCLK   = 1,
    HAL_TIMER_CLK_MFCLK   = 2,
} hal_timer_clk_src_t;

/* ---------- Timer configuration ---------- */
typedef struct {
    uint32_t            instance;      /* TIMA0_INST / TIMG0_INST etc. */
    hal_timer_mode_t    mode;
    hal_timer_clk_src_t clk_src;
    uint32_t            clk_div;       /* Clock divider value */
    uint32_t            period_us;     /* Period in microseconds */
    hal_irq_priority_t  irq_priority;
} hal_timer_config_t;

/* ---------- Timer callback ---------- */
typedef void (*hal_timer_cb_t)(uint32_t instance);

/* ========== API ========== */

hal_status_t hal_timer_init(const hal_timer_config_t *config);
hal_status_t hal_timer_deinit(uint32_t instance);

hal_status_t hal_timer_start(uint32_t instance);
hal_status_t hal_timer_stop(uint32_t instance);

hal_status_t hal_timer_set_period(uint32_t instance, uint32_t period_us);
uint32_t     hal_timer_get_counter(uint32_t instance);

hal_status_t hal_timer_register_callback(uint32_t instance, hal_timer_cb_t cb);

#ifdef __cplusplus
}
#endif

#endif /* HAL_TIMER_H */
