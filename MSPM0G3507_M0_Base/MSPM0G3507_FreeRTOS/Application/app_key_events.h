/**
 * @file    app_key_events.h
 * @brief   应用层按键事件处理。
 */
#ifndef APP_KEY_EVENTS_H
#define APP_KEY_EVENTS_H

#include <stdint.h>
#include "key.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief   应用层按键事件处理。
 */
void app_key_event_callback(const key_t *key,
                            key_event_type_t event,
                            uint32_t timestamp_ms,
                            uint32_t pressed_duration_ms,
                            void *user_data);

#ifdef __cplusplus
}
#endif

#endif /* APP_KEY_EVENTS_H */
