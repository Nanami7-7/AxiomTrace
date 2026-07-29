/**
 * @file    app_position_control.c
 * @brief   位置-速度串级控制实现(纯算法,无硬件依赖)
 * @note    在现有速度环(5ms)之上增加位置环/角度环(20ms).
 *
 *          位置模式(POSITION):
 *            梯形规划器(脉冲单位) → planned_speed(脉冲/s)
 *            转换为RPM → 速度环setpoint
 *            位置环PID修正 → 速度环setpoint叠加
 *
 *          角度模式(ANGLE):
 *            角度环PID直接控制 → yaw_correction(差速RPM)
 *            输出斜坡限幅 → 平滑加减速
 *            left = -yaw_correction, right = +yaw_correction
 *
 *          模式切换(P0保护):
 *            切换瞬间外环PID复位
 *            速度环setpoint保持当前RPM
 *            经过渡期渐变到新目标
 */
#include "app_position_control.h"
#include <stddef.h>   /* NULL */
#include <math.h>
#include "project_config.h"

/* ======================== 私有常量 ======================== */

/** 到位判定: 误差<阈值持续200ms(20ms×10周期) */
#define REACHED_DEFAULT_COUNT     (10U)

/** 模式切换过渡默认时长(ms) */
#define TRANSITION_DEFAULT_MS      (1000U)

/** 角度环输出斜坡限幅默认值(RPM/s, 限制差速变化率) */
#define YAW_RATE_LIMIT_DEFAULT     (500.0f)

/** ======================== 私有函数 ======================== */

/**
 * @brief  限幅函数
 */
static inline float clamp_f(float val, float min_val, float max_val)
{
    if (val < min_val) { return min_val; }
    if (val > max_val) { return max_val; }
    return val;
}

/**
 * @brief  RPM → 脉冲/s 转换
 * @param  rpm      转速(RPM)
 * @param  pulses_per_rev 每转脉冲数
 * @retval 脉冲/秒
 */
static inline float rpm_to_pulse_rate(float rpm,
                                       uint32_t pulses_per_rev)
{
    return rpm * (float)pulses_per_rev / 60.0f;
}

/**
 * @brief  脉冲/s → RPM 转换
 */
static inline float pulse_rate_to_rpm(float pulse_rate,
                                        uint32_t pulses_per_rev)
{
    if (pulses_per_rev == 0U) { return 0.0f; }
    return pulse_rate * 60.0f / (float)pulses_per_rev;
}

/**
 * @brief  检查模式切换过渡是否完成
 */
static bool is_transition_done(const app_position_ctrl_t *ctrl,
                                 uint32_t now_tick)
{
    if (ctrl->phase != APP_CTRL_PHASE_TRANSITION) {
        return true;
    }
    return (now_tick - ctrl->transition_tick) >=
           ctrl->transition_ms;
}

/**
 * @brief  计算模式切换过渡系数(0→1)
 * @retval 0=切换开始, 1=切换完成
 */
static float transition_factor(const app_position_ctrl_t *ctrl,
                                 uint32_t now_tick)
{
    if (ctrl->phase != APP_CTRL_PHASE_TRANSITION) {
        return 1.0f;
    }
    uint32_t elapsed = now_tick - ctrl->transition_tick;
    if (elapsed >= ctrl->transition_ms) {
        return 1.0f;
    }
    return (float)elapsed / (float)ctrl->transition_ms;
}

/**
 * @brief  角度归一化到 [-180, 180]
 * @note   处理角度环绕(如 350° → -10°)
 */
static float normalize_angle(float angle)
{
    if (!isfinite(angle)) { return 0.0f; }
    if (angle > 180.0f) {
        angle = (angle <= 540.0f) ? angle - 360.0f
                                  : fmodf(angle + 180.0f, 360.0f) - 180.0f;
    } else if (angle < -180.0f) {
        angle = (angle >= -540.0f) ? angle + 360.0f
                                   : fmodf(angle - 180.0f, 360.0f) + 180.0f;
    }
    return angle;
}

/**
 * @brief  角度环输出斜坡限幅
 * @param  ctrl        控制器指针
 * @param  target      目标输出
 * @param  current     当前输出
 * @param  dt          时间步长
 * @retval 限幅后的输出
 */
static float yaw_rate_ramp_limit(app_position_ctrl_t *ctrl,
                                   float target,
                                   float current,
                                   float dt)
{
    (void)ctrl;
    if (dt <= 0.0f) { return current; }
    float max_change = YAW_RATE_LIMIT_DEFAULT * dt;
    float delta = target - current;
    delta = clamp_f(delta, -max_change, max_change);
    return current + delta;
}

/* ======================== 公共函数实现 ======================== */

void app_posctrl_init(app_position_ctrl_t *ctrl,
                      float pos_kp, float pos_ki, float pos_kd,
                      float yaw_kp, float yaw_ki, float yaw_kd,
                      float accel, float max_rpm,
                      uint32_t pulses_per_rev)
{
    if (ctrl == NULL) { return; }

    /* 默认SPEED模式 */
    ctrl->mode     = APP_CTRL_MODE_SPEED;
    ctrl->prev_mode = APP_CTRL_MODE_SPEED;
    ctrl->phase    = APP_CTRL_PHASE_STABLE;
    ctrl->transition_tick = 0U;
    ctrl->transition_ms  = PRJ_MODE_TRANSITION_MS;
    ctrl->transition_rpm = 0.0f;

    /* 每转脉冲数(用于位置模式RPM↔脉冲/s转换) */
    ctrl->pulses_per_rev = (pulses_per_rev > 0U) ?
                            pulses_per_rev : 1U;

    /*
     * 规划器(位置模式用, 脉冲单位)
     * RPM → 脉冲/s 转换: pulse_rate = rpm × PPR / 60
     * 这样 planner 内部 pos=∫speed dt = 脉冲(单位一致)
     */
    float accel_pulse = rpm_to_pulse_rate(accel,
                                          ctrl->pulses_per_rev);
    float max_pulse   = rpm_to_pulse_rate(max_rpm,
                                          ctrl->pulses_per_rev);
    app_planner_init(&ctrl->planner, accel_pulse,
                      -max_pulse, max_pulse);

    /* 位置环PID(位置式, 输出RPM修正) */
    app_pid_init(&ctrl->pos_pid,
                  pos_kp, pos_ki, pos_kd,
                  APP_PID_MODE_POSITION,
                  -max_rpm, max_rpm);

    /* 角度环PID(位置式, 输出差速RPM) */
    app_pid_init(&ctrl->yaw_pid,
                  yaw_kp, yaw_ki, yaw_kd,
                  APP_PID_MODE_POSITION,
                  -max_rpm, max_rpm);

    /* 到位检测 */
    ctrl->reached                = false;
    ctrl->reached_count          = 0U;
    ctrl->reached_threshold_count = REACHED_DEFAULT_COUNT;
    ctrl->reached_threshold      = 5.0f;

    /* 启动基准 */
    ctrl->pos_start    = 0.0f;
    ctrl->yaw_start    = 0.0f;
    ctrl->cruise_speed = 0.0f;

    /* 输出清零 */
    for (uint32_t i = 0; i < APP_POS_MOTOR_COUNT; i++) {
        ctrl->output.target_rpm[i] = 0.0f;
    }
    ctrl->output.planned_speed  = 0.0f;
    ctrl->output.planned_pos    = 0.0f;
    ctrl->output.pos_correction = 0.0f;
    ctrl->output.yaw_correction = 0.0f;
    ctrl->output.reached        = false;
    ctrl->output.mode           = APP_CTRL_MODE_SPEED;
    ctrl->output.planner_state  = APP_PLANNER_STATE_IDLE;

    ctrl->pos_correction = 0.0f;
    ctrl->sync_error = 0.0f;
    ctrl->sync_correction = 0.0f;
    ctrl->position_feedback_mask = 0U;
    for (uint32_t i = 0U; i < APP_POS_MOTOR_COUNT; ++i) {
        ctrl->position_start[i] = 0.0f;
    }
    ctrl->output.sync_correction = 0.0f;
    ctrl->output.pos_error = 0.0f;
    ctrl->output.sync_error = 0.0f;
    ctrl->yaw_correction = 0.0f;
}

void app_posctrl_set_mode(app_position_ctrl_t *ctrl,
                           app_ctrl_mode_t mode,
                           const float current_rpm[],
                           uint32_t now_tick)
{
    if (ctrl == NULL) { return; }
    if (mode == ctrl->mode) { return; }

    /* 记录切换前模式 */
    ctrl->prev_mode = ctrl->mode;

    /* 记录当前RPM(用于过渡) */
    if (current_rpm != NULL) {
        float avg = 0.0f;
        uint32_t active_count = 0U;
        for (uint32_t i = 0U; i < APP_POS_MOTOR_COUNT; i++) {
            if ((PRJ_MOTION_FEEDBACK_MASK & (1UL << i)) != 0UL) {
                avg += current_rpm[i];
                active_count++;
            }
        }
        ctrl->transition_rpm = active_count != 0U
            ? avg / (float)active_count : 0.0f;
    } else {
        ctrl->transition_rpm = 0.0f;
    }

    /* 外环PID复位 */
    app_pid_reset(&ctrl->pos_pid);
    app_pid_reset(&ctrl->yaw_pid);
    app_planner_reset(&ctrl->planner);

    /* 到位检测复位 */
    ctrl->reached       = false;
    ctrl->reached_count = 0U;

    /* 启动过渡 */
    ctrl->mode              = mode;
    ctrl->transition_ms     = (mode == APP_CTRL_MODE_POSITION) ?
                              PRJ_POSITION_TRANSITION_MS :
                              PRJ_MODE_TRANSITION_MS;
    ctrl->phase             = APP_CTRL_PHASE_TRANSITION;
    ctrl->transition_tick   = now_tick;
    ctrl->pos_correction    = 0.0f;
    ctrl->yaw_correction    = 0.0f;

    /* 切到SPEED模式时立即完成过渡(速度环setpoint由用户设置) */
    if (mode == APP_CTRL_MODE_SPEED) {
        ctrl->phase = APP_CTRL_PHASE_STABLE;
        for (uint32_t i = 0; i < APP_POS_MOTOR_COUNT; i++) {
            ctrl->output.target_rpm[i] = ctrl->transition_rpm;
        }
    }
}

void app_posctrl_start_position(app_position_ctrl_t *ctrl,
                                float target_pulses,
                                float cruise_speed,
                                float current_pos)
{
    app_posctrl_start_position_feedback(ctrl, target_pulses,
                                        cruise_speed, current_pos, NULL);
}

void app_posctrl_start_position_feedback(app_position_ctrl_t *ctrl,
                                          float target_pulses,
                                          float cruise_speed,
                                          float current_pos,
                                          const app_pos_feedback_t *feedback)
{
    if (ctrl == NULL) { return; }
    if (ctrl->mode != APP_CTRL_MODE_POSITION) { return; }
    if (!isfinite(target_pulses) || !isfinite(cruise_speed) ||
        !isfinite(current_pos)) {
        return;
    }

    ctrl->pos_start = current_pos;
    ctrl->cruise_speed = fabsf(cruise_speed);
    ctrl->reached = false;
    ctrl->reached_count = 0U;
    ctrl->sync_error = 0.0f;
    ctrl->sync_correction = 0.0f;
    ctrl->position_feedback_mask = 0U;
    for (uint32_t i = 0U; i < APP_POS_MOTOR_COUNT; ++i) {
        ctrl->position_start[i] = current_pos;
    }
    if (feedback != NULL) {
        ctrl->position_feedback_mask = feedback->valid_mask;
        for (uint32_t i = 0U; i < APP_POS_MOTOR_COUNT; ++i) {
            if ((feedback->valid_mask & (1UL << i)) != 0UL &&
                isfinite(feedback->position[i])) {
                ctrl->position_start[i] = feedback->position[i];
            } else {
                ctrl->position_feedback_mask &= ~(1UL << i);
            }
        }
    }

    /* 规划器启动(脉冲单位): RPM -> 脉冲/s。 */
    float cruise_pulse = rpm_to_pulse_rate(cruise_speed,
                                           ctrl->pulses_per_rev);
    app_planner_start(&ctrl->planner, target_pulses, cruise_pulse);

    /* 位置环PID: feedback 使用相对启动点的平均计数。 */
    app_pid_set_setpoint(&ctrl->pos_pid, target_pulses);
    app_pid_reset(&ctrl->pos_pid);
}

void app_posctrl_start_angle(app_position_ctrl_t *ctrl,
                              float target_angle,
                              float cruise_speed,
                              float current_yaw)
{
    if (ctrl == NULL) { return; }
    if (ctrl->mode != APP_CTRL_MODE_ANGLE) { return; }

    /* NaN/Inf 保护 */
    if (!isfinite(target_angle) || !isfinite(cruise_speed) ||
        !isfinite(current_yaw)) {
        return;
    }

    ctrl->yaw_start    = current_yaw;
    ctrl->cruise_speed = fabsf(cruise_speed);
    ctrl->reached      = false;
    ctrl->reached_count = 0U;

    /* 角度环PID: setpoint=目标yaw(绝对角度) */
    float target_yaw = normalize_angle(current_yaw + target_angle);
    app_pid_set_setpoint(&ctrl->yaw_pid, target_yaw);
    app_pid_reset(&ctrl->yaw_pid);
    ctrl->yaw_correction = 0.0f;
}

const app_pos_output_t *app_posctrl_update_feedback(
    app_position_ctrl_t *ctrl,
    const app_pos_feedback_t *feedback,
    float kf_yaw,
    float dt_s,
    uint32_t now_tick)
{
    if (ctrl == NULL || feedback == NULL) { return NULL; }
    float encoder_avg = feedback->average_position;
    if (!isfinite(encoder_avg)) {
        float sum = 0.0f;
        uint32_t count = 0U;
        for (uint32_t i = 0U; i < APP_POS_MOTOR_COUNT; ++i) {
            if ((feedback->valid_mask & (1UL << i)) != 0UL &&
                isfinite(feedback->position[i])) {
                sum += feedback->position[i];
                count++;
            }
        }
        encoder_avg = (count != 0U) ? (sum / (float)count) : 0.0f;
    }
    if (!isfinite(encoder_avg) || !isfinite(kf_yaw) ||
        !isfinite(dt_s) || dt_s <= 0.0f) {
        return &ctrl->output;
    }

    /* 更新输出模式 */
    ctrl->output.mode = ctrl->mode;

    /* SPEED模式: 不修改target_rpm(由用户直接设置速度环) */
    if (ctrl->mode == APP_CTRL_MODE_SPEED) {
        ctrl->output.reached       = false;
        ctrl->output.planner_state = APP_PLANNER_STATE_IDLE;
        return &ctrl->output;
    }

    /* 检查过渡是否完成 */
    if (is_transition_done(ctrl, now_tick)) {
        ctrl->phase = APP_CTRL_PHASE_STABLE;
    }
    float t_factor = transition_factor(ctrl, now_tick);

    if (ctrl->mode == APP_CTRL_MODE_POSITION) {
        /* ---- 位置模式: 梯形规划 + 平均位置 + 左右同步修正 ---- */
        float planned_pulse_rate = app_planner_update(&ctrl->planner, dt_s);
        float planned_rpm = pulse_rate_to_rpm(planned_pulse_rate,
                                               ctrl->pulses_per_rev);

        float relative_pos = encoder_avg - ctrl->pos_start;
        float correction = app_pid_compute(&ctrl->pos_pid,
                                           relative_pos, dt_s);
        if (!isfinite(correction)) {
            correction = 0.0f;
            app_pid_reset(&ctrl->pos_pid);
        }
        ctrl->pos_correction = correction;

        /* 右侧减左侧：正值表示右侧超前。 */
        float left_delta = 0.0f;
        float right_delta = 0.0f;
        uint32_t left_count = 0U;
        uint32_t right_count = 0U;
        const uint32_t feedback_mask = feedback->valid_mask &
                                        ctrl->position_feedback_mask;
        for (uint32_t i = 0U; i < APP_POS_MOTOR_COUNT; ++i) {
            if ((feedback_mask & (1UL << i)) == 0UL ||
                !isfinite(feedback->position[i])) {
                continue;
            }
            float delta = feedback->position[i] - ctrl->position_start[i];
            if (i == APP_POS_MOTOR_LF || i == APP_POS_MOTOR_LB) {
                left_delta += delta;
                left_count++;
            } else {
                right_delta += delta;
                right_count++;
            }
        }
        if (left_count != 0U) {
            left_delta /= (float)left_count;
        }
        if (right_count != 0U) {
            right_delta /= (float)right_count;
        }

        bool sync_valid = (left_count != 0U) && (right_count != 0U);
        float sync_error = sync_valid ? (right_delta - left_delta) : 0.0f;
        float sync_trim = 0.0f;
        if (sync_valid &&
            fabsf(sync_error) > PRJ_POSITION_SYNC_DEADBAND_COUNTS) {
            sync_trim = clamp_f(sync_error * PRJ_POSITION_SYNC_KP,
                                -PRJ_POSITION_SYNC_MAX_RPM,
                                PRJ_POSITION_SYNC_MAX_RPM);
        }
        ctrl->sync_error = sync_error;
        ctrl->sync_correction = sync_trim;

        float base_target_rpm = planned_rpm + correction;
        float left_target_rpm = base_target_rpm + sync_trim;
        float right_target_rpm = base_target_rpm - sync_trim;
        if (ctrl->phase == APP_CTRL_PHASE_TRANSITION) {
            left_target_rpm = ctrl->transition_rpm * (1.0f - t_factor)
                            + left_target_rpm * t_factor;
            right_target_rpm = ctrl->transition_rpm * (1.0f - t_factor)
                             + right_target_rpm * t_factor;
        }
        left_target_rpm = clamp_f(left_target_rpm,
                                  ctrl->pos_pid.out_min,
                                  ctrl->pos_pid.out_max);
        right_target_rpm = clamp_f(right_target_rpm,
                                   ctrl->pos_pid.out_min,
                                   ctrl->pos_pid.out_max);

        /*
         * 位置环同向保护：同步修正只能改变左右轮速度差，
         * 不能把同向前进/后退变成一侧反转。
         * 该保护只作用于位置环输出，不改测速、速度 PID 或角度环。
         */
        const float pos_error = ctrl->pos_pid.setpoint - relative_pos;
        if (fabsf(pos_error) > PRJ_POSITION_STOP_ERROR_COUNTS) {
            if (pos_error > 0.0f) {
                left_target_rpm = fmaxf(left_target_rpm, 0.0f);
                right_target_rpm = fmaxf(right_target_rpm, 0.0f);
            } else {
                left_target_rpm = fminf(left_target_rpm, 0.0f);
                right_target_rpm = fminf(right_target_rpm, 0.0f);
            }
        }

        /*
         * Avoid a fixed minimum RPM step near the target. A hard step between
         * zero and +/-MIN_ACTIVE_RPM makes the position loop repeatedly reverse.
         * The minimum-speed assist now ramps with position error:
         *   - inside stop error: command zero;
         *   - just outside stop error: assist starts at zero;
         *   - after the configured error span: assist reaches MIN_ACTIVE_RPM.
         * This changes position-loop output only; speed, encoder and angle loops
         * remain unchanged.
         */

        const float abs_pos_error = fabsf(pos_error);
        if (abs_pos_error <= PRJ_POSITION_STOP_ERROR_COUNTS) {
            left_target_rpm = 0.0f;
            right_target_rpm = 0.0f;
        } else if (PRJ_POSITION_MIN_ACTIVE_RPM > 0.0f) {
            const float error_span = fmaxf(
                PRJ_POSITION_MIN_ACTIVE_ERROR_COUNTS,
                PRJ_POSITION_STOP_ERROR_COUNTS + 1.0f);
            const float normalized_error = clamp_f(
                (abs_pos_error - PRJ_POSITION_STOP_ERROR_COUNTS) /
                error_span, 0.0f, 1.0f);
            const float active_floor =
                PRJ_POSITION_MIN_ACTIVE_RPM * normalized_error;

            if (fabsf(left_target_rpm) < active_floor) {
                const float sign = (fabsf(left_target_rpm) > 0.001f) ?
                                   left_target_rpm : pos_error;
                left_target_rpm = copysignf(active_floor, sign);
            }
            if (fabsf(right_target_rpm) < active_floor) {
                const float sign = (fabsf(right_target_rpm) > 0.001f) ?
                                   right_target_rpm : pos_error;
                right_target_rpm = copysignf(active_floor, sign);
            }
        }

        /* 输出仍覆盖四个接口；B/C 未接电机时由现有配置/硬件保持备用。 */
        ctrl->output.target_rpm[APP_POS_MOTOR_LF] = left_target_rpm;
        ctrl->output.target_rpm[APP_POS_MOTOR_LB] = left_target_rpm;
        ctrl->output.target_rpm[APP_POS_MOTOR_RF] = right_target_rpm;
        ctrl->output.target_rpm[APP_POS_MOTOR_RB] = right_target_rpm;

        float reached_threshold = fmaxf(ctrl->reached_threshold,
                                        PRJ_POSITION_STOP_ERROR_COUNTS);
        bool position_close = fabsf(pos_error) <= reached_threshold;
        bool sync_close = !sync_valid ||
                          fabsf(sync_error) <= PRJ_POSITION_SYNC_ERROR_COUNTS;
        /*
         * Position completion is encoder-error based.  Do not gate it on
         * the measured RPM: low-speed M/T feedback is intentionally not
         * used to decide whether the encoder target has been reached.
         */
        if (app_planner_is_done(&ctrl->planner) && position_close &&
            sync_close) {
            ctrl->reached_count++;
            if (ctrl->reached_count >= ctrl->reached_threshold_count) {
                ctrl->reached = true;
                for (uint32_t i = 0U; i < APP_POS_MOTOR_COUNT; ++i) {
                    ctrl->output.target_rpm[i] = 0.0f;
                }
            }
        } else {
            ctrl->reached_count = 0U;
            ctrl->reached = false;
        }

        ctrl->output.planned_speed = planned_rpm;
        ctrl->output.planned_pos = app_planner_get_pos(&ctrl->planner);
        ctrl->output.pos_correction = correction;
        ctrl->output.sync_correction = sync_trim;
        ctrl->output.pos_error = pos_error;
        ctrl->output.sync_error = sync_error;
        ctrl->output.yaw_correction = 0.0f;
        ctrl->output.reached = ctrl->reached;
        ctrl->output.planner_state = app_planner_get_state(&ctrl->planner);
    } else if (ctrl->mode == APP_CTRL_MODE_ANGLE) {
        /* ---- 角度模式: 角度环PID + 斜坡限幅 ---- */

        /* 1. 角度环PID计算 */
        /*    setpoint=目标yaw, feedback=当前kf_yaw */
        /* PID 必须看到最短圆弧误差，否则 179° -> -179° 会误走 358°。 */
        float yaw_error = normalize_angle(ctrl->yaw_pid.setpoint - kf_yaw);
        float wrapped_feedback = ctrl->yaw_pid.setpoint - yaw_error;
        float yaw_pid_out =
            app_pid_compute(&ctrl->yaw_pid, wrapped_feedback, dt_s);

        /* NaN保护 */
        if (!isfinite(yaw_pid_out)) {
            yaw_pid_out = 0.0f;
            app_pid_reset(&ctrl->yaw_pid);
        }

        /* 2. 输出斜坡限幅(实现软启停) */
        float target_yaw_rate = yaw_pid_out;
        if (ctrl->phase == APP_CTRL_PHASE_TRANSITION) {
            /* 过渡期: 从0渐变到PID输出 */
            target_yaw_rate = 0.0f * (1.0f - t_factor)
                            + yaw_pid_out * t_factor;
        }
        ctrl->yaw_correction =
            yaw_rate_ramp_limit(ctrl, target_yaw_rate,
                                 ctrl->yaw_correction, dt_s);

        /* 限幅(用角度环PID的限幅, RPM单位) */
        ctrl->yaw_correction = clamp_f(
            ctrl->yaw_correction,
            ctrl->yaw_pid.out_min,
            ctrl->yaw_pid.out_max);

        /* 3. 差速分配 */
        /*    左轮 = -yaw_correction (左转时左轮后退) */
        /*    右轮 = +yaw_correction (左转时右轮前进) */
        /*    注意: 实际方向由驱动层各路PRJ_MOTOR_x_INSTALL_DIR_SIGN修正。 */
        float left_target  = -ctrl->yaw_correction;
        float right_target =  ctrl->yaw_correction;

        /*
         * 只给当前配置为“驱动轮”的物理通道分配角度环目标。
         * B/C 接口仍然保留；以后装回电机时只需把对应配置改为 1。
         * 物理映射：A/M1=RB，B/M2=RF，C/M3=LF，D/M4=LB。
         */
        for (uint32_t i = 0U; i < APP_POS_MOTOR_COUNT; ++i) {
            ctrl->output.target_rpm[i] = 0.0f;
        }
#if (PRJ_MOTOR_C_MOTION_ACTIVE != 0U)
        ctrl->output.target_rpm[APP_POS_MOTOR_LF] = left_target;
#endif
#if (PRJ_MOTOR_D_MOTION_ACTIVE != 0U)
        ctrl->output.target_rpm[APP_POS_MOTOR_LB] = left_target;
#endif
#if (PRJ_MOTOR_B_MOTION_ACTIVE != 0U)
        ctrl->output.target_rpm[APP_POS_MOTOR_RF] = right_target;
#endif
#if (PRJ_MOTOR_A_MOTION_ACTIVE != 0U)
        ctrl->output.target_rpm[APP_POS_MOTOR_RB] = right_target;
#endif

        /* 4. 到位检测 */
        /* yaw_error 已在 PID 计算前归一化。 */
        if (fabsf(yaw_error) < ctrl->reached_threshold) {
            ctrl->reached_count++;
            if (ctrl->reached_count >=
                ctrl->reached_threshold_count) {
                ctrl->reached = true;
                /* 到位: 目标速度归零 */
                for (uint32_t i = 0; i < APP_POS_MOTOR_COUNT; i++){
                    ctrl->output.target_rpm[i] = 0.0f;
                }
            }
        } else {
            ctrl->reached_count = 0U;
            ctrl->reached = false;
        }

        /* 5. 更新输出 */
        ctrl->output.planned_speed  = 0.0f;
        ctrl->output.planned_pos    = 0.0f;
        ctrl->output.pos_correction = 0.0f;
        ctrl->output.yaw_correction = ctrl->yaw_correction;
        ctrl->output.reached        = ctrl->reached;
        ctrl->output.planner_state  = APP_PLANNER_STATE_IDLE;
    }

    return &ctrl->output;
}

const app_pos_output_t *app_posctrl_update(app_position_ctrl_t *ctrl,
                                             float encoder_avg,
                                             float kf_yaw,
                                             float dt_s,
                                             uint32_t now_tick)
{
    app_pos_feedback_t feedback = {0};
    feedback.average_position = encoder_avg;
    return app_posctrl_update_feedback(ctrl, &feedback, kf_yaw,
                                       dt_s, now_tick);
}

bool app_posctrl_is_reached(const app_position_ctrl_t *ctrl)
{
    if (ctrl == NULL) { return false; }
    return ctrl->reached;
}

app_ctrl_mode_t app_posctrl_get_mode(const app_position_ctrl_t *ctrl)
{
    if (ctrl == NULL) { return APP_CTRL_MODE_SPEED; }
    return ctrl->mode;
}

void app_posctrl_set_pos_pid(app_position_ctrl_t *ctrl,
                              float kp, float ki, float kd)
{
    if (ctrl == NULL) { return; }
    app_pid_set_params(&ctrl->pos_pid, kp, ki, kd);
}

void app_posctrl_set_yaw_pid(app_position_ctrl_t *ctrl,
                              float kp, float ki, float kd)
{
    if (ctrl == NULL) { return; }
    app_pid_set_params(&ctrl->yaw_pid, kp, ki, kd);
}

void app_posctrl_set_accel(app_position_ctrl_t *ctrl, float accel)
{
    if (ctrl == NULL) { return; }
    if (accel > 0.0f && isfinite(accel)) {
        /* RPM/s → 脉冲/s² (planner内部用脉冲单位) */
        ctrl->planner.accel =
            rpm_to_pulse_rate(accel, ctrl->pulses_per_rev);
    }
}

void app_posctrl_emergency_stop(app_position_ctrl_t *ctrl)
{
    if (ctrl == NULL) { return; }
    app_planner_abort(&ctrl->planner);
    app_pid_reset(&ctrl->pos_pid);
    app_pid_reset(&ctrl->yaw_pid);
    ctrl->reached       = true;
    ctrl->reached_count = ctrl->reached_threshold_count;
    ctrl->phase         = APP_CTRL_PHASE_STABLE;
    ctrl->mode          = APP_CTRL_MODE_SPEED;
    ctrl->pos_correction = 0.0f;
    ctrl->yaw_correction = 0.0f;
    for (uint32_t i = 0; i < APP_POS_MOTOR_COUNT; i++) {
        ctrl->output.target_rpm[i] = 0.0f;
    }
    ctrl->output.mode = APP_CTRL_MODE_SPEED;
}
