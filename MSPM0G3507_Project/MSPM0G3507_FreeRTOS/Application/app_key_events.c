#include "app_key_events.h"
#include "app_main.h"
#include "app_protocol_user.h"
#include "key_config.h"
#include <stdio.h>

/*
 * 按键状态机只产生事件，本文件把事件翻译为运动目标。
 * 自定义按键功能时，优先修改下面三个 action 函数，不要改 BSP/Input。
 */
/** 把运动控制门面的状态转换成按键动作层状态。 */
static app_key_motion_state_t key_state_from_motion(app_motion_state_t state)
{
    switch (state) {
    case APP_MOTION_STATE_SPEED:
        return APP_KEY_MOTION_FORWARD;
    case APP_MOTION_STATE_ANGLE:
        return APP_KEY_MOTION_TURN_LEFT;
    case APP_MOTION_STATE_POSITION:
        return APP_KEY_MOTION_FORWARD;
    case APP_MOTION_STATE_FAULT:
        return APP_KEY_MOTION_FAULT;
    default:
        return APP_KEY_MOTION_IDLE;
    }
}

/** 初始化按键动作层，实际初始化由运动控制门面完成。 */
bool app_key_motion_init(struct app_shared_ctx_s *ctx)
{
    return app_motion_control_init(ctx);
}

/** 由控制任务周期调用，推进运动超时处理。 */
void app_key_motion_process(uint32_t now_ms)
{
    app_motion_control_process(now_ms);
}

/** 对外提供的按键紧急停车入口。 */
void app_key_motion_emergency_stop(void)
{
    app_motion_stop();
}

/** 查询当前按键运动状态。 */
app_key_motion_state_t app_key_motion_get_state(void)
{
    return key_state_from_motion(app_motion_get_state());
}

/** 查询按键动作是否仍在执行。 */
bool app_key_motion_is_busy(void)
{
    return app_motion_is_busy();
}

/*
 * 这三个函数是用户最常修改的入口：
 * - 短按：通常调用速度环 API；
 * - 长按：通常调用角度环 API；
 * - 卡键：通常调用停止或位置环 API。
 * 回调本身不直接操作 PWM，避免破坏现有控制任务时序。
 */
void app_key_short_press_action(const key_t *key,
                                uint32_t timestamp_ms,
                                uint32_t pressed_duration_ms,
                                void *user_data)
{
    (void)key;
    (void)timestamp_ms;
    (void)pressed_duration_ms;
    (void)user_data;

    /*
     * 原短按位置环暂时停用，保留为可恢复的参考代码：
     *
     * if (app_motion_position_start(1.0f, 120.0f, 10000U)) {
     *     app_encoder_telemetry_start();
     * }
     *
     * 现在短按只提交“启动循迹”的请求。真正的循迹启停和电机输出
     * 仍由控制任务在固定周期内执行，避免按键任务直接覆盖 PWM。
     */
    app_motion_stop();
    app_protocol_user_line_track_request_start();
    (void)printf("[KEY] SHORT_PRESS -> line tracking request\r\n");
}

void app_key_long_press_action(const key_t *key,
                               uint32_t timestamp_ms,
                               uint32_t pressed_duration_ms,
                               void *user_data)
{
    (void)key;
    (void)timestamp_ms;
    (void)pressed_duration_ms;
    (void)user_data;

    /* 角度环接管前，先撤销循迹请求，避免控制任务下一周期重新启动循迹。 */
    app_protocol_user_line_track_force_stop();

    /* 示例：相对当前 yaw 左转 90 度，巡航速度 120 RPM。 */
    (void)app_motion_angle_start_relative(
        PRJ_KEY_TURN_TARGET_DEG, PRJ_KEY_TURN_CRUISE_RPM,
        PRJ_KEY_TURN_TIMEOUT_MS);
    (void)printf("[KEY] LONG_PRESS -> angle API\r\n");
}

void app_key_stuck_action(const key_t *key,
                          uint32_t timestamp_ms,
                          uint32_t pressed_duration_ms,
                          void *user_data)
{
    (void)key;
    (void)timestamp_ms;
    (void)pressed_duration_ms;
    (void)user_data;

    /* 卡键时同时撤销循迹请求，确保安全停车后不会被自动重启。 */
    app_protocol_user_line_track_force_stop();

    /*
     * 如果需要把卡键改成位置环前进 2 米，可改为：
     * app_motion_position_start(2.0f, 120.0f, 10000U);
     * 当前默认先停车，避免按键卡住时车辆继续运动。
     */
    app_motion_stop();
    (void)printf("[KEY] STUCK -> motion stop\r\n");
}

/**
 * 统一按键事件回调：当前只处理 KEY_key(id=0)，KEY_switch 仅保留监测。
 */
void app_key_event_callback(const key_t *key,
                            key_event_type_t event,
                            uint32_t timestamp_ms,
                            uint32_t pressed_duration_ms,
                            void *user_data)
{
    /* id=0 是 KEY_key/PA7；PB3 的 KEY_switch 不直接控制电机。 */
    if (key == NULL || key->id != 0U) {
        return;
    }

    switch (event) {
    case KEY_EVENT_SHORT_PRESS:
        app_key_short_press_action(key, timestamp_ms,
                                   pressed_duration_ms, user_data);
        break;
    case KEY_EVENT_LONG_PRESS:
        app_key_long_press_action(key, timestamp_ms,
                                  pressed_duration_ms, user_data);
        break;
    case KEY_EVENT_STUCK:
        app_key_stuck_action(key, timestamp_ms,
                             pressed_duration_ms, user_data);
        break;
    default:
        break;
    }
}
