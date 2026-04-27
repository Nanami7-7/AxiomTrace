/**
 * @file    bsp_servo.h
 * @brief   Servo driver interface (PWM angle / pulse-width control)
 * @note    Standard 50 Hz servo (SG90 etc.), 0.5~2.5 ms pulse → 0°~180°
 */

#ifndef BSP_SERVO_H
#define BSP_SERVO_H

#ifdef __cplusplus
extern "C" {
#endif

#include "hal_common.h"

/* ---------- Servo channel ---------- */
typedef enum {
    BSP_SERVO_0 = 0,
    BSP_SERVO_1 = 1,
} bsp_servo_t;

/* ========== API ========== */

/**
 * @brief  Initialize servo PWM channels
 */
hal_status_t bsp_servo_init(void);

/**
 * @brief  Deinitialize servo PWM channels
 */
hal_status_t bsp_servo_deinit(void);

/**
 * @brief  Set servo angle
 * @param  servo  Servo channel
 * @param  angle  Angle in degrees (0~180)
 */
hal_status_t bsp_servo_set_angle(bsp_servo_t servo, uint32_t angle);

/**
 * @brief  Set servo pulse width directly
 * @param  servo       Servo channel
 * @param  pulse_us    Pulse width in microseconds (500~2500)
 */
hal_status_t bsp_servo_set_pulse_width(bsp_servo_t servo, uint32_t pulse_us);

#ifdef __cplusplus
}
#endif

#endif /* BSP_SERVO_H */
