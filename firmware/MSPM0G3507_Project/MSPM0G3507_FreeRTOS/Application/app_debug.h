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
void app_debug_adc_test(void);

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
