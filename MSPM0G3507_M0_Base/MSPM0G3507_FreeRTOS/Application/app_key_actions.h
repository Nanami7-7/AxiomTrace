/**
 * @file    app_key_actions.h
 * @brief   应用层按键事件处理。
 * @details 提供本模块的基础功能实现。
 */
#ifndef APP_KEY_ACTIONS_H
#define APP_KEY_ACTIONS_H

#include <stdbool.h>
#include <stdint.h>
#include "key.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief   应用层按键事件处理。
 * @return 返回处理结果。
 */
bool app_key_actions_dispatch(const key_t *key,
                              key_event_type_t event,
                              uint32_t timestamp_ms,
                              uint32_t pressed_duration_ms,
                              void *user_data);

#ifdef __cplusplus
}
#endif

#endif /* APP_KEY_ACTIONS_H */
