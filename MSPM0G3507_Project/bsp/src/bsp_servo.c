/**
 * @file    bsp_servo.c
 * @brief   Servo driver implementation (PWM angle / pulse-width control)
 */

#include "bsp_servo.h"
#include "bsp_pin_config.h"
#include "bsp_config.h"
#include "hal_pwm.h"

/* ---------- Servo pin map ---------- */
typedef struct {
    uint32_t pwm_inst;
    uint32_t pwm_channel;
} bsp_servo_pinmap_t;

static const bsp_servo_pinmap_t s_servo_map[BSP_SERVO_COUNT] = {
    { BSP_SERVO0_PWM_INST, BSP_SERVO0_PWM_CHANNEL },
    { BSP_SERVO1_PWM_INST, BSP_SERVO1_PWM_CHANNEL },
};

/* ---------- Internal helpers ---------- */

/**
 * @brief  Convert pulse width (us) to duty cycle percent for 50 Hz PWM
 *         Period = 1/50 = 20000 us
 *         duty% = pulse_us * 100 / 20000
 */
static uint32_t pulse_to_duty(uint32_t pulse_us)
{
    if (pulse_us < BSP_SERVO_MIN_PULSE_US) pulse_us = BSP_SERVO_MIN_PULSE_US;
    if (pulse_us > BSP_SERVO_MAX_PULSE_US) pulse_us = BSP_SERVO_MAX_PULSE_US;

    return (pulse_us * 100UL) / 20000UL;
}

/**
 * @brief  Convert angle (0~180) to pulse width (us)
 *         pulse = MIN + (MAX - MIN) * angle / 180
 */
static uint32_t angle_to_pulse(uint32_t angle)
{
    if (angle > 180) angle = 180;

    return BSP_SERVO_MIN_PULSE_US +
           (BSP_SERVO_MAX_PULSE_US - BSP_SERVO_MIN_PULSE_US) * angle / 180UL;
}

/* ========== API ========== */

hal_status_t bsp_servo_init(void)
{
    hal_status_t status;

    for (uint32_t i = 0; i < BSP_SERVO_COUNT; i++) {
        /* Center position (90° = 1.5 ms) at init */
        uint32_t center_pulse = angle_to_pulse(90);
        uint32_t center_duty  = pulse_to_duty(center_pulse);

        hal_pwm_config_t pwm_cfg = {
            .instance     = s_servo_map[i].pwm_inst,
            .channel      = s_servo_map[i].pwm_channel,
            .freq_hz      = BSP_SERVO_PWM_FREQ_HZ,
            .duty_percent = center_duty,
            .irq_priority = HAL_IRQ_PRIORITY_LOW,
        };

        status = hal_pwm_init(&pwm_cfg);
        if (status != HAL_OK) return status;

        status = hal_pwm_start(s_servo_map[i].pwm_inst, s_servo_map[i].pwm_channel);
        if (status != HAL_OK) return status;
    }

    return HAL_OK;
}

hal_status_t bsp_servo_deinit(void)
{
    hal_status_t status;

    for (uint32_t i = 0; i < BSP_SERVO_COUNT; i++) {
        hal_pwm_stop(s_servo_map[i].pwm_inst, s_servo_map[i].pwm_channel);
        status = hal_pwm_deinit(s_servo_map[i].pwm_inst, s_servo_map[i].pwm_channel);
        if (status != HAL_OK) return status;
    }

    return HAL_OK;
}

hal_status_t bsp_servo_set_angle(bsp_servo_t servo, uint32_t angle)
{
    if (servo >= BSP_SERVO_COUNT) return HAL_INVALID;

    uint32_t duty = pulse_to_duty(angle_to_pulse(angle));

    return hal_pwm_set_duty(s_servo_map[servo].pwm_inst,
                            s_servo_map[servo].pwm_channel, duty);
}

hal_status_t bsp_servo_set_pulse_width(bsp_servo_t servo, uint32_t pulse_us)
{
    if (servo >= BSP_SERVO_COUNT) return HAL_INVALID;

    uint32_t duty = pulse_to_duty(pulse_us);

    return hal_pwm_set_duty(s_servo_map[servo].pwm_inst,
                            s_servo_map[servo].pwm_channel, duty);
}
