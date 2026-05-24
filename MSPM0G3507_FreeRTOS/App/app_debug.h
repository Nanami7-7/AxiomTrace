#ifndef APP_DEBUG_H
#define APP_DEBUG_H

#include <stdint.h>
#include <stdbool.h>

struct app_shared_ctx_s;

void app_debug_encoder_stream(uint32_t period_ms);

void app_debug_encoder_diag(struct app_shared_ctx_s *ctx,
                             uint32_t motor_id);

void app_debug_adc_test(void);

#endif