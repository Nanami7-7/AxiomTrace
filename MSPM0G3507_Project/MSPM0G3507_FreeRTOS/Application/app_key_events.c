#include "app_key_events.h"
#include "app_main.h"
#include "app_position_control.h"
#include "app_pid.h"
#include "bsp_motor.h"
#include "key_config.h"
#include "osal_api.h"
#include <math.h>
#include <stdio.h>

static app_shared_ctx_t *s_ctx;
static volatile app_key_action_t s_pending_action = APP_KEY_ACTION_NONE;
static volatile uint32_t s_pending_timestamp_ms;
static app_key_motion_state_t s_state = APP_KEY_MOTION_IDLE;
static uint32_t s_motion_start_ms;
static uint32_t s_settle_start_ms;

static bool time_elapsed(uint32_t now_ms, uint32_t start_ms, uint32_t duration_ms)
{
    return (uint32_t)(now_ms - start_ms) >= duration_ms;
}

static void motion_stop(bool disable_power)
{
    float current_rpm[APP_POS_MOTOR_COUNT] = {0.0f};

    if (s_ctx == NULL) {
        return;
    }

    app_posctrl_emergency_stop(&s_ctx->posctrl);
    app_motor_stop_all(s_ctx);
    if (disable_power) {
        bsp_motor_power_disable();
    }
    app_posctrl_set_mode(&s_ctx->posctrl, APP_CTRL_MODE_SPEED,
                         current_rpm, osal_get_tick_count());
    s_state = APP_KEY_MOTION_IDLE;
}

static bool motion_start_forward(uint32_t now_ms)
{
    if (s_ctx == NULL || s_state != APP_KEY_MOTION_IDLE) {
        return false;
    }

    if (!bsp_motor_power_is_enabled() &&
        bsp_motor_power_enable() != BSP_OK) {
        s_state = APP_KEY_MOTION_FAULT;
        return false;
    }

    app_posctrl_set_mode(&s_ctx->posctrl, APP_CTRL_MODE_SPEED,
                         NULL, osal_get_tick_count());
    for (uint32_t i = 0U; i < BSP_MOTOR_COUNT; ++i) {
        app_pid_reset(&s_ctx->pid[i]);
        app_pid_set_setpoint(&s_ctx->pid[i], PRJ_KEY_FORWARD_RPM);
        s_ctx->motor_enabled[i] = true;
    }
    s_motion_start_ms = now_ms;
    s_state = APP_KEY_MOTION_FORWARD;
    (void)printf("[MOTION] FORWARD start rpm=%.0f timeout=%lu ms\r\n",
                 (double)PRJ_KEY_FORWARD_RPM,
                 (unsigned long)PRJ_KEY_FORWARD_TIMEOUT_MS);
    return true;
}

static bool motion_start_turn(uint32_t now_ms)
{
    float current_rpm[APP_POS_MOTOR_COUNT] = {0.0f};
    float current_yaw;

    if (s_ctx == NULL || s_state != APP_KEY_MOTION_IDLE) {
        return false;
    }

    current_yaw = s_ctx->imu.yaw;
    if (!isfinite(current_yaw)) {
        s_state = APP_KEY_MOTION_FAULT;
        (void)printf("[MOTION] TURN_LEFT rejected: invalid IMU yaw\r\n");
        return false;
    }

    for (uint32_t i = 0U; i < BSP_MOTOR_COUNT; ++i) {
        current_rpm[i] = (float)s_ctx->status.rpm[i];
    }
    if (!bsp_motor_power_is_enabled() &&
        bsp_motor_power_enable() != BSP_OK) {
        s_state = APP_KEY_MOTION_FAULT;
        return false;
    }

    app_posctrl_set_mode(&s_ctx->posctrl, APP_CTRL_MODE_ANGLE,
                         current_rpm, osal_get_tick_count());
    app_posctrl_start_angle(&s_ctx->posctrl, PRJ_KEY_TURN_TARGET_DEG,
                            PRJ_KEY_TURN_CRUISE_RPM, current_yaw);
    for (uint32_t i = 0U; i < BSP_MOTOR_COUNT; ++i) {
        app_pid_reset(&s_ctx->pid[i]);
        s_ctx->motor_enabled[i] = true;
    }
    s_motion_start_ms = now_ms;
    s_state = APP_KEY_MOTION_TURN_LEFT;
    (void)printf("[MOTION] TURN_LEFT start angle=%.0f cruise=%.0f timeout=%lu ms\r\n",
                 (double)PRJ_KEY_TURN_TARGET_DEG,
                 (double)PRJ_KEY_TURN_CRUISE_RPM,
                 (unsigned long)PRJ_KEY_TURN_TIMEOUT_MS);
    return true;
}

bool app_key_motion_init(app_shared_ctx_t *ctx)
{
    if (ctx == NULL) {
        return false;
    }
    s_ctx = ctx;
    s_state = APP_KEY_MOTION_IDLE;
    s_motion_start_ms = 0U;
    s_settle_start_ms = 0U;
    OSAL_CRITICAL_SECTION {
        s_pending_action = APP_KEY_ACTION_NONE;
        s_pending_timestamp_ms = 0U;
    }
    return true;
}

bool app_key_motion_post(app_key_action_t action, uint32_t timestamp_ms)
{
    bool accepted = false;

    if (s_ctx == NULL || action == APP_KEY_ACTION_NONE) {
        return false;
    }
    OSAL_CRITICAL_SECTION {
        if (action == APP_KEY_ACTION_STOP) {
            s_pending_action = action;
            s_pending_timestamp_ms = timestamp_ms;
            accepted = true;
        } else if (s_state == APP_KEY_MOTION_IDLE &&
                   s_pending_action == APP_KEY_ACTION_NONE) {
            s_pending_action = action;
            s_pending_timestamp_ms = timestamp_ms;
            accepted = true;
        }
    }
    return accepted;
}

void app_key_motion_emergency_stop(void)
{
    motion_stop(true);
}

void app_key_motion_process(uint32_t now_ms)
{
    app_key_action_t action = APP_KEY_ACTION_NONE;
    uint32_t action_timestamp = 0U;

    if (s_ctx == NULL) {
        return;
    }

    OSAL_CRITICAL_SECTION {
        action = s_pending_action;
        action_timestamp = s_pending_timestamp_ms;
        s_pending_action = APP_KEY_ACTION_NONE;
    }
    (void)action_timestamp;

    if (action == APP_KEY_ACTION_STOP) {
        (void)printf("[MOTION] emergency stop\r\n");
        motion_stop(true);
        return;
    }

    if (action == APP_KEY_ACTION_FORWARD_200RPM) {
        (void)motion_start_forward(now_ms);
    } else if (action == APP_KEY_ACTION_TURN_LEFT_90) {
        (void)motion_start_turn(now_ms);
    }

    if (s_state == APP_KEY_MOTION_FORWARD &&
        time_elapsed(now_ms, s_motion_start_ms, PRJ_KEY_FORWARD_TIMEOUT_MS)) {
        (void)printf("[MOTION] FORWARD timeout\r\n");
        motion_stop(true);
    } else if (s_state == APP_KEY_MOTION_TURN_LEFT) {
        if (app_posctrl_is_reached(&s_ctx->posctrl)) {
            s_settle_start_ms = now_ms;
            s_state = APP_KEY_MOTION_STOPPING;
            (void)printf("[MOTION] TURN_LEFT reached, settling\r\n");
        } else if (time_elapsed(now_ms, s_motion_start_ms,
                               PRJ_KEY_TURN_TIMEOUT_MS)) {
            (void)printf("[MOTION] TURN_LEFT timeout\r\n");
            motion_stop(true);
        }
    } else if (s_state == APP_KEY_MOTION_STOPPING &&
               time_elapsed(now_ms, s_settle_start_ms,
                            PRJ_KEY_TURN_SETTLE_MS)) {
        (void)printf("[MOTION] TURN_LEFT complete\r\n");
        motion_stop(true);
    }
}

app_key_motion_state_t app_key_motion_get_state(void)
{
    return s_state;
}

bool app_key_motion_is_busy(void)
{
    return s_state != APP_KEY_MOTION_IDLE && s_state != APP_KEY_MOTION_FAULT;
}

void app_key_event_callback(const key_t *key,
                            key_event_type_t event,
                            uint32_t timestamp_ms,
                            uint32_t pressed_duration_ms,
                            void *user_data)
{
    (void)pressed_duration_ms;
    (void)user_data;

    if (key == NULL || key->id != 0U) {
        return;
    }

    switch (event) {
    case KEY_EVENT_SHORT_PRESS:
        (void)app_key_motion_post(APP_KEY_ACTION_FORWARD_200RPM, timestamp_ms);
        break;
    case KEY_EVENT_LONG_PRESS:
        (void)app_key_motion_post(APP_KEY_ACTION_TURN_LEFT_90, timestamp_ms);
        break;
    case KEY_EVENT_STUCK:
        (void)app_key_motion_post(APP_KEY_ACTION_STOP, timestamp_ms);
        break;
    default:
        break;
    }
}
