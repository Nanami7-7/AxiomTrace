/**
 * @file    bsp_motor.h
 * @brief   Motor driver interface (PWM + GPIO H-bridge control)
 * @note    Supports dual DC motors with TB6612/L298N H-bridge
 */

#ifndef BSP_MOTOR_H
#define BSP_MOTOR_H

#ifdef __cplusplus
extern "C" {
#endif

#include "hal_common.h"

/* ---------- Motor channel ---------- */
typedef enum {
    BSP_MOTOR_A = 0,
    BSP_MOTOR_B = 1,
} bsp_motor_t;

/* ---------- Motor direction ---------- */
typedef enum {
    BSP_MOTOR_DIR_FORWARD  = 0,
    BSP_MOTOR_DIR_BACKWARD = 1,
    BSP_MOTOR_DIR_STOP     = 2,
    BSP_MOTOR_DIR_BRAKE    = 3,
} bsp_motor_dir_t;

/* ========== API ========== */

/**
 * @brief  Initialize both motor drivers (PWM + GPIO direction pins)
 */
hal_status_t bsp_motor_init(void);

/**
 * @brief  Deinitialize both motor drivers
 */
hal_status_t bsp_motor_deinit(void);

/**
 * @brief  Set motor speed
 * @param  motor   Motor channel (A or B)
 * @param  speed   Speed in percent (0~100)
 */
hal_status_t bsp_motor_set_speed(bsp_motor_t motor, uint32_t speed);

/**
 * @brief  Set motor direction
 * @param  motor     Motor channel (A or B)
 * @param  direction Forward / Backward / Stop / Brake
 */
hal_status_t bsp_motor_set_direction(bsp_motor_t motor, bsp_motor_dir_t direction);

/**
 * @brief  Convenience: set both speed and direction at once
 */
hal_status_t bsp_motor_run(bsp_motor_t motor, bsp_motor_dir_t direction, uint32_t speed);

/**
 * @brief  Stop motor (coast, IN1=0 IN2=0)
 */
hal_status_t bsp_motor_stop(bsp_motor_t motor);

/**
 * @brief  Brake motor (short brake, IN1=1 IN2=1)
 */
hal_status_t bsp_motor_brake(bsp_motor_t motor);

#ifdef __cplusplus
}
#endif

#endif /* BSP_MOTOR_H */
