/**
 * @file    app_key_events.c
 * @brief   应用层按键事件处理。
 */
#include "app_key_events.h"
#include "app_key_actions.h"

void app_key_event_callback(const key_t *key,
                            key_event_type_t event,
                            uint32_t timestamp_ms,
                            uint32_t pressed_duration_ms,
                            void *user_data)
{
    /* 应用层按键事件处理。 */
    (void)app_key_actions_dispatch(key,
                                   event,
                                   timestamp_ms,
                                   pressed_duration_ms,
                                   user_data);
}
