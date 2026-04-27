/**
 * @file    bsp_hcsr04.h
 * @brief   HC-SR04 ultrasonic distance sensor interface
 * @note    Trig (GPIO output) + Echo (GPIO input interrupt) + Timer
 *          Distance = sound_speed * time / 2 ≈ 0.034 cm/µs * time / 2
 */

#ifndef BSP_HCSR04_H
#define BSP_HCSR04_H

#ifdef __cplusplus
extern "C" {
#endif

#include "hal_common.h"

/* ---------- HC-SR04 callback (for async mode) ---------- */
typedef void (*bsp_hcsr04_callback_t)(uint32_t distance_mm);

/* ========== API ========== */

/**
 * @brief  Initialize HC-SR04 (Trig GPIO output, Echo GPIO input + interrupt, Timer)
 */
hal_status_t bsp_hcsr04_init(void);

/**
 * @brief  Deinitialize HC-SR04
 */
hal_status_t bsp_hcsr04_deinit(void);

/**
 * @brief  Trigger a measurement and wait for result (blocking)
 * @param  distance_mm  Pointer to store distance in millimeters
 * @param  timeout_ms   Maximum wait time for echo
 * @return HAL_OK on success, HAL_TIMEOUT if no echo received
 */
hal_status_t bsp_hcsr04_get_distance(uint32_t *distance_mm, uint32_t timeout_ms);

/**
 * @brief  Get distance in centimeters (blocking)
 * @param  distance_cm  Pointer to store distance in centimeters
 * @param  timeout_ms   Maximum wait time for echo
 */
hal_status_t bsp_hcsr04_get_distance_cm(uint32_t *distance_cm, uint32_t timeout_ms);

/**
 * @brief  Start an async measurement (non-blocking)
 * @note   Result delivered via registered callback
 */
hal_status_t bsp_hcsr04_start_measure(void);

/**
 * @brief  Register callback for async measurement completion
 * @param  callback  Function called when measurement completes
 */
hal_status_t bsp_hcsr04_register_callback(bsp_hcsr04_callback_t callback);

#ifdef __cplusplus
}
#endif

#endif /* BSP_HCSR04_H */
