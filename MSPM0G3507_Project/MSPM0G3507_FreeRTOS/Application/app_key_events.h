/**
 * @file    app_key_events.h
 * @brief   按键事件到运动控制动作的应用层适配接口。
 * @details key.c 负责消抖和事件识别，本文件负责决定事件发生后做什么。
 *          KEY_key(PA7) 的短按、长按、卡键分别进入三个独立函数；以后自定义
 *          功能时只需修改这三个函数体，不需要改底层按键状态机。
 *
 * 回调中可以调用 app_motion_* 目标设置 API，但不要直接写 PWM 或长时间阻塞。
 */
#ifndef APP_KEY_EVENTS_H
#define APP_KEY_EVENTS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>
#include "key.h"
#include "app_motion_control.h"

struct app_shared_ctx_s;

/** 预留的按键动作枚举，便于以后扩展事件队列或日志。 */
typedef enum {
    APP_KEY_ACTION_NONE = 0,       /**< 无动作。 */
    APP_KEY_ACTION_FORWARD_200RPM, /**< 默认 200 RPM 前进。 */
    APP_KEY_ACTION_TURN_LEFT_90,   /**< 默认 90 度左转。 */
    APP_KEY_ACTION_STOP             /**< 停止所有运动。 */
} app_key_action_t;

/** 按键动作层对外提供的运动状态。 */
typedef enum {
    APP_KEY_MOTION_IDLE = 0,  /**< 空闲。 */
    APP_KEY_MOTION_FORWARD,   /**< 正在前进。 */
    APP_KEY_MOTION_TURN_LEFT, /**< 正在左转。 */
    APP_KEY_MOTION_STOPPING,  /**< 正在停车。 */
    APP_KEY_MOTION_FAULT      /**< 动作执行失败。 */
} app_key_motion_state_t;

/** 初始化按键动作层，实际运动上下文由 app_motion_control 管理。 */
bool app_key_motion_init(struct app_shared_ctx_s *ctx);

/** 由现有控制任务周期调用，负责动作超时检查。 */
void app_key_motion_process(uint32_t now_ms);

/** 紧急停止按键触发的运动并清除当前目标。 */
void app_key_motion_emergency_stop(void);

/** 查询当前按键运动状态。 */
app_key_motion_state_t app_key_motion_get_state(void);

/** 查询按键动作是否仍在执行。 */
bool app_key_motion_is_busy(void);

/*
 * 用户可直接编辑的三个动作入口：短按通常调用速度环，长按通常调用角度环，
 * 卡键通常停车或调用位置环。硬件/PWM 代码应留在 BSP 和控制任务中。
 */
/** KEY_key 释放后被识别为短按时调用。 */
void app_key_short_press_action(const key_t *key,
                                uint32_t timestamp_ms,
                                uint32_t pressed_duration_ms,
                                void *user_data);
/** KEY_key 按住达到长按阈值时调用一次。 */
void app_key_long_press_action(const key_t *key,
                               uint32_t timestamp_ms,
                               uint32_t pressed_duration_ms,
                               void *user_data);
/** KEY_key 持续按住超过 max_hold_ms 时调用一次。 */
void app_key_stuck_action(const key_t *key,
                          uint32_t timestamp_ms,
                          uint32_t pressed_duration_ms,
                          void *user_data);

/** 统一事件回调；当前只把 id=0 的 KEY_key 映射到运动动作。 */
void app_key_event_callback(const key_t *key,
                            key_event_type_t event,
                            uint32_t timestamp_ms,
                            uint32_t pressed_duration_ms,
                            void *user_data);

#ifdef __cplusplus
}
#endif

#endif /* APP_KEY_EVENTS_H */
