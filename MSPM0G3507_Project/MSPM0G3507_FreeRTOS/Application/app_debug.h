#ifndef APP_DEBUG_H
#define APP_DEBUG_H

#include <stdint.h>
#include <stdbool.h>

struct app_shared_ctx_s;

void app_debug_encoder_stream(uint32_t period_ms);

void app_debug_encoder_diag(struct app_shared_ctx_s *ctx,
                             uint32_t motor_id);

/**
 * @brief Print one read-only hardware-mapping and encoder snapshot.
 * @note  Temporary board-verification diagnostic. It does not command motors,
 *        clear encoder counters, or create a periodic task.
 */
void app_debug_hwmap_snapshot(void);

/**
 * @brief Run a read-only CC0/CC1/LOAD capture diagnostic in factory mode.
 * @param motor_id 0..3 for one encoder, BSP_ENCODER_COUNT for all encoders
 * @param duration_ms observation window, clamped by the implementation
 */
void app_debug_encoder_capture_diag(uint32_t motor_id, uint32_t duration_ms);

void app_debug_adc_test(void);

/** 读取一次四路红外并通过 UART0 控制台打印结果。 */
void app_debug_ir_snapshot(void);

/** Start the persistent DRV8870 oscilloscope session (factory build only). */
void app_debug_drv8870_scope_start(struct app_shared_ctx_s *ctx);

/** Set one motor's direct PWM duty; duty_permille is 0..1000 (0.0..100.0%). */
void app_debug_drv8870_scope_set(uint32_t motor_id,
                                  uint32_t duty_permille);

/** Set every motor to one direct PWM duty; duty_permille is 0..1000. */
void app_debug_drv8870_scope_set_all(uint32_t duty_permille);

/** Print session, PB19, channel mapping, compare, and duty information. */
void app_debug_drv8870_scope_status(void);

/** Restore neutral PWM, switch PB19 off, and release the scope session. */
void app_debug_drv8870_scope_stop(void);

#endif
