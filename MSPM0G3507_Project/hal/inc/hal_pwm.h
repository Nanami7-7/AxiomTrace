/**
 * @file    hal_pwm.h
 * @brief   PWM hardware abstraction layer interface
 */

#ifndef HAL_PWM_H
#define HAL_PWM_H

#ifdef __cplusplus
extern "C" {
#endif

#include "hal_common.h"

/* ---------- PWM configuration ---------- */
typedef struct {
    uint32_t  instance;      /* TIMA0_INST / TIMG0_INST etc. (timer used for PWM) */
    uint32_t  channel;       /* PWM output channel (CCR index) */
    uint32_t  freq_hz;       /* PWM frequency in Hz */
    uint32_t  duty_percent;  /* Initial duty cycle 0~100 */
    hal_irq_priority_t irq_priority;
} hal_pwm_config_t;

/* ========== API ========== */

hal_status_t hal_pwm_init(const hal_pwm_config_t *config);
hal_status_t hal_pwm_deinit(uint32_t instance, uint32_t channel);

hal_status_t hal_pwm_start(uint32_t instance, uint32_t channel);
hal_status_t hal_pwm_stop(uint32_t instance, uint32_t channel);

hal_status_t hal_pwm_set_duty(uint32_t instance, uint32_t channel, uint32_t duty_percent);
hal_status_t hal_pwm_set_frequency(uint32_t instance, uint32_t channel, uint32_t freq_hz);

#ifdef __cplusplus
}
#endif

#endif /* HAL_PWM_H */
