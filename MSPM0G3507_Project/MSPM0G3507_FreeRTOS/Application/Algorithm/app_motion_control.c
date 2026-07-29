#include "app_motion_control.h"
#include "app_main.h"
#include "app_pid.h"
#include "app_position_control.h"
#include "bsp_encoder.h"
#include "bsp_motor.h"
#include "osal_api.h"
#include "project_config.h"
#include <math.h>
#include <stdio.h>

/*
 * 本文件只保存当前运动模式和超时状态。
 * PID 计算、编码器测速和 PWM 更新仍由现有控制任务/BSP 负责。
 */
static app_shared_ctx_t *s_ctx;
static app_motion_state_t s_state = APP_MOTION_STATE_IDLE;
static uint32_t s_start_ms;
static uint32_t s_timeout_ms;

/* A-D 的物理电机索引与 PRJ_MOTION_MOTOR_ACTIVE_MASK 保持一致。 */
static const bsp_encoder_id_t s_motion_encoder_map[BSP_MOTOR_COUNT] =
    PRJ_MOTOR_ENCODER_MAP;

static bool motion_motor_is_active(uint32_t motor_index)
{
    return motor_index < BSP_MOTOR_COUNT &&
           ((PRJ_MOTION_MOTOR_ACTIVE_MASK & (1UL << motor_index)) != 0UL);
}

/* 判断某个电机是否参与车体 RPM/位置反馈；与输出是否开放相互独立。 */
static bool motion_feedback_is_active(uint32_t motor_index)
{
    return motor_index < BSP_MOTOR_COUNT &&
           ((PRJ_MOTION_FEEDBACK_MASK & (1UL << motor_index)) != 0UL);
}

/** 检查 RPM 是否有限且未超过统一规划器上限。 */
static bool valid_rpm(float rpm)
{
    return isfinite(rpm) &&
           fabsf(rpm) <= (float)PRJ_PLANNER_MAX_RPM;
}

/** 检查超时参数；0 表示持续运行，非 0 至少保留一个控制周期。 */
static bool valid_timeout(uint32_t timeout_ms)
{
    return timeout_ms == 0U || timeout_ms >= 20U;
}

/** 确保电机电源已打开，失败时由上层进入故障状态。 */
static bool ensure_power(void)
{
    if (bsp_motor_power_is_enabled()) {
        return true;
    }
    return bsp_motor_power_enable() == BSP_OK;
}

/** 读取当前四路 RPM，作为切换控制模式时的初始值。 */
static void read_current_rpm(float out[APP_MOTION_MOTOR_COUNT])
{
    for (uint32_t i = 0U; i < APP_MOTION_MOTOR_COUNT; ++i) {
        out[i] = (float)s_ctx->status.rpm[i];
    }
}

/** 清除四路速度 PID 的历史状态，避免旧模式残留。 */
static void reset_speed_controllers(void)
{
    for (uint32_t i = 0U; i < APP_MOTION_MOTOR_COUNT; ++i) {
        app_pid_reset(&s_ctx->pid[i]);
    }
}

/** 速度模式内部实现：校验目标、开电源、切换模式并写入 PID 目标。 */
static bool set_speed_targets(const float rpm[APP_MOTION_MOTOR_COUNT],
                              uint32_t timeout_ms)
{
    float current_rpm[APP_MOTION_MOTOR_COUNT];

    if (s_ctx == NULL || rpm == NULL || !valid_timeout(timeout_ms)) {
        return false;
    }
    for (uint32_t i = 0U; i < APP_MOTION_MOTOR_COUNT; ++i) {
        if (!valid_rpm(rpm[i])) {
            return false;
        }
    }
    if (!ensure_power()) {
        s_state = APP_MOTION_STATE_FAULT;
        return false;
    }

    read_current_rpm(current_rpm);
    OSAL_CRITICAL_SECTION {
        app_posctrl_set_mode(&s_ctx->posctrl, APP_CTRL_MODE_SPEED,
                             current_rpm, osal_get_tick_count());
        reset_speed_controllers();
        for (uint32_t i = 0U; i < APP_MOTION_MOTOR_COUNT; ++i) {
            /* 万向轮通道永远保持 0 RPM，避免被速度 API 意外驱动。 */
            const float target = motion_motor_is_active(i) ? rpm[i] : 0.0f;
            app_pid_set_setpoint(&s_ctx->pid[i], target);
            s_ctx->motor_enabled[i] = motion_motor_is_active(i) &&
                                      fabsf(target) > 0.01f;
        }
    }

    s_start_ms = osal_ticks_to_ms(osal_get_tick_count());
    s_timeout_ms = timeout_ms;
    s_state = APP_MOTION_STATE_SPEED;
    return true;
}

/** 初始化运动控制上下文和状态。 */
bool app_motion_control_init(struct app_shared_ctx_s *ctx)
{
    if (ctx == NULL) {
        return false;
    }
    s_ctx = (app_shared_ctx_t *)ctx;
    s_start_ms = 0U;
    s_timeout_ms = 0U;
    s_state = APP_MOTION_STATE_IDLE;
    return true;
}

/** 单电机速度控制；未指定的三路目标会被置零。 */
bool app_motion_speed_motor(app_motion_motor_id_t motor, float rpm)
{
    float targets[APP_MOTION_MOTOR_COUNT] = {0.0f};

    if ((uint32_t)motor >= APP_MOTION_MOTOR_COUNT ||
        !motion_motor_is_active((uint32_t)motor)) {
        return false;
    }
    targets[(uint32_t)motor] = rpm;
    return set_speed_targets(targets, 0U);
}

/** 四电机持续速度控制。 */
bool app_motion_speed_all(const float rpm[APP_MOTION_MOTOR_COUNT])
{
    return set_speed_targets(rpm, 0U);
}

bool app_motion_speed_all_timed(const float rpm[APP_MOTION_MOTOR_COUNT],
                                uint32_t timeout_ms)
{
    return set_speed_targets(rpm, timeout_ms);
}

/** 按当前 yaw 启动相对角度控制，底层目标由位置控制器执行。 */
bool app_motion_angle_start_relative(float delta_angle_deg,
                                     float cruise_rpm,
                                     uint32_t timeout_ms)
{
    float current_rpm[APP_MOTION_MOTOR_COUNT];
    float current_yaw;

    if (s_ctx == NULL || !isfinite(delta_angle_deg) ||
        !valid_rpm(cruise_rpm) || cruise_rpm <= 0.0f ||
        !valid_timeout(timeout_ms)) {
        return false;
    }
    current_yaw = s_ctx->imu.yaw;
    if (!isfinite(current_yaw) || !ensure_power()) {
        s_state = APP_MOTION_STATE_FAULT;
        return false;
    }

    read_current_rpm(current_rpm);
    OSAL_CRITICAL_SECTION {
        app_posctrl_set_mode(&s_ctx->posctrl, APP_CTRL_MODE_ANGLE,
                             current_rpm, osal_get_tick_count());
        app_posctrl_start_angle(&s_ctx->posctrl, delta_angle_deg,
                                cruise_rpm, current_yaw);
        reset_speed_controllers();
        for (uint32_t i = 0U; i < APP_MOTION_MOTOR_COUNT; ++i) {
            s_ctx->motor_enabled[i] = motion_motor_is_active(i);
        }
    }

    s_start_ms = osal_ticks_to_ms(osal_get_tick_count());
    s_timeout_ms = timeout_ms;
    s_state = APP_MOTION_STATE_ANGLE;
    return true;
}

bool app_motion_angle_start_absolute(float target_angle_deg,
                                     float cruise_rpm,
                                     uint32_t timeout_ms)
{
    float current_yaw;

    if (s_ctx == NULL || !isfinite(target_angle_deg)) {
        return false;
    }
    current_yaw = s_ctx->imu.yaw;
    if (!isfinite(current_yaw)) {
        return false;
    }
    return app_motion_angle_start_relative(target_angle_deg - current_yaw,
                                            cruise_rpm, timeout_ms);
}

/** 按轮径和编码器参数把距离换算为目标脉冲并启动位置控制。 */
bool app_motion_position_start(float distance_m,
                               float cruise_rpm,
                               uint32_t timeout_ms)
{
    int32_t totals[BSP_ENCODER_COUNT];
    float encoder_avg = 0.0f;
    app_pos_feedback_t start_feedback = {0};
    float target_pulses;
    float current_rpm[APP_MOTION_MOTOR_COUNT];

    if (s_ctx == NULL || !isfinite(distance_m) ||
        !valid_rpm(cruise_rpm) || cruise_rpm <= 0.0f ||
        !valid_timeout(timeout_ms) ||
        PRJ_MOTOR_WHEEL_DIAMETER_MM <= 0.0f ||
        PRJ_ENCODER_PULSES_PER_REV == 0U) {
        return false;
    }
    if (!ensure_power() ||
        bsp_encoder_get_all_totals(totals) != BSP_OK) {
        s_state = APP_MOTION_STATE_FAULT;
        return false;
    }
    {
        uint32_t active_count = 0U;
        for (uint32_t motor = 0U; motor < APP_MOTION_MOTOR_COUNT; ++motor) {
            if (motion_feedback_is_active(motor)) {
                const uint32_t encoder_id =
                    (uint32_t)s_motion_encoder_map[motor];
                if (encoder_id < BSP_ENCODER_COUNT) {
                    start_feedback.position[encoder_id] = (float)totals[encoder_id];
                    start_feedback.valid_mask |= (1UL << encoder_id);
                    encoder_avg += (float)totals[encoder_id];
                    active_count++;
                }
            }
        }
        if (active_count == 0U) {
            s_state = APP_MOTION_STATE_FAULT;
            return false;
        }
        encoder_avg /= (float)active_count;
        start_feedback.average_position = encoder_avg;
    }
    target_pulses = distance_m /
        ((float)PRJ_MOTOR_WHEEL_DIAMETER_MM * 0.001f * PRJ_PI_F) *
        (float)PRJ_ENCODER_PULSES_PER_REV;
    if (!isfinite(target_pulses)) {
        return false;
    }

    read_current_rpm(current_rpm);
    OSAL_CRITICAL_SECTION {
        app_posctrl_set_mode(&s_ctx->posctrl, APP_CTRL_MODE_POSITION,
                             current_rpm, osal_get_tick_count());
        app_posctrl_start_position_feedback(&s_ctx->posctrl, target_pulses,
                                             cruise_rpm, encoder_avg,
                                             &start_feedback);
        reset_speed_controllers();
        for (uint32_t i = 0U; i < APP_MOTION_MOTOR_COUNT; ++i) {
            s_ctx->motor_enabled[i] = motion_motor_is_active(i);
        }
    }

    s_start_ms = osal_ticks_to_ms(osal_get_tick_count());
    s_timeout_ms = timeout_ms;
    s_state = APP_MOTION_STATE_POSITION;
    return true;
}

/** 取消外环目标、停止所有电机并回到空闲状态。 */
void app_motion_stop(void)
{
    if (s_ctx == NULL) {
        return;
    }
    OSAL_CRITICAL_SECTION {
        app_posctrl_emergency_stop(&s_ctx->posctrl);
    }
    app_motor_stop_all(s_ctx);
    s_timeout_ms = 0U;
    s_state = APP_MOTION_STATE_IDLE;
}

/** 在控制任务中检查速度/位置/角度动作是否超时。 */
void app_motion_control_process(uint32_t now_ms)
{
    if (s_ctx == NULL || s_timeout_ms == 0U ||
        s_state == APP_MOTION_STATE_IDLE ||
        s_state == APP_MOTION_STATE_FAULT) {
        return;
    }
    if ((uint32_t)(now_ms - s_start_ms) >= s_timeout_ms) {
        (void)printf("[MOTION] timeout; stop\r\n");
        app_motion_stop();
    }
}

/** 返回当前运动状态，供按键、菜单和日志使用。 */
app_motion_state_t app_motion_get_state(void)
{
    return s_state;
}

/** 判断是否仍有运动动作在执行。 */
bool app_motion_is_busy(void)
{
    return s_state != APP_MOTION_STATE_IDLE &&
           s_state != APP_MOTION_STATE_FAULT;
}
