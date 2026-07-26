#ifndef APP_KEY_EVENTS_H
#define APP_KEY_EVENTS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>
#include "key.h"

struct app_shared_ctx_s;

typedef enum {
    APP_KEY_ACTION_NONE = 0,
    APP_KEY_ACTION_FORWARD_200RPM,
    APP_KEY_ACTION_TURN_LEFT_90,
    APP_KEY_ACTION_STOP
} app_key_action_t;

typedef enum {
    APP_KEY_MOTION_IDLE = 0,
    APP_KEY_MOTION_FORWARD,
    APP_KEY_MOTION_TURN_LEFT,
    APP_KEY_MOTION_STOPPING,
    APP_KEY_MOTION_FAULT
} app_key_motion_state_t;

bool app_key_motion_init(struct app_shared_ctx_s *ctx);
bool app_key_motion_post(app_key_action_t action, uint32_t timestamp_ms);
void app_key_motion_process(uint32_t now_ms);
void app_key_motion_emergency_stop(void);
app_key_motion_state_t app_key_motion_get_state(void);
bool app_key_motion_is_busy(void);

void app_key_event_callback(const key_t *key,
                            key_event_type_t event,
                            uint32_t timestamp_ms,
                            uint32_t pressed_duration_ms,
                            void *user_data);

#ifdef __cplusplus
}
#endif

#endif /* APP_KEY_EVENTS_H */

