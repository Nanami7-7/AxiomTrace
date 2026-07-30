/**
 * @file    task_control.c
 * @brief   控制任务实现
 * @note    2ms周期(500Hz): 读取编码器→M/T法测速RPM→PID计算→设置电机命令
 */
#include "task_control.h"
#include "app_main.h"
#include "app_pid.h"
#include "app_feedforward.h"
#include "app_complementary_filter.h"
#include "app_chassis_assist.h"
#include "app_speed_planner.h"
#include "app_model_id.h"
#include "app_position_control.h"
#include "app_key_events.h"
#include "app_encoder_telemetry.h"
#include "app_protocol_user.h"
#include "osal_api.h"
#include "bsp_motor.h"
#include "bsp_encoder.h"
#include "bsp_adc.h"
#include "project_config.h"
#if (PRJ_UART1_BLE_DEBUG_ENABLE != 0U)
#include "app_uart1_ble_debug.h"
#endif
#include "axiomtrace.h"
#include <math.h>
#include <stdio.h>

#ifndef PRJ_LINE_TRACK_ENABLE
#define PRJ_LINE_TRACK_ENABLE (1U)
#endif
#if PRJ_LINE_TRACK_ENABLE
#include "app_line_track.h"
#endif

/** 规划/位置外环固定为20ms(50Hz)，速度PID内环固定为2ms(500Hz)。 */
#define POSCTRL_OUTER_PERIOD_MS (20U)
#if ((POSCTRL_OUTER_PERIOD_MS % PRJ_CONTROL_PERIOD_MS) != 0U)
#error "POSCTRL_OUTER_PERIOD_MS must be divisible by PRJ_CONTROL_PERIOD_MS"
#endif
#define POSCTRL_OUTER_RATIO \
    (POSCTRL_OUTER_PERIOD_MS / PRJ_CONTROL_PERIOD_MS)

#if (PRJ_SPEED_SCURVE_ENABLE != 0U)
#define SPEED_PLANNER_RATIO \
    (PRJ_SPEED_PLANNER_PERIOD_MS / PRJ_CONTROL_PERIOD_MS)
#endif

/** 将车轮逻辑顺序(LF/LB/RF/RB)映射为电机顺序(A/M1~D/M4)。 */
static const bsp_encoder_id_t s_motor_encoder_map[BSP_MOTOR_COUNT] =
    PRJ_MOTOR_ENCODER_MAP;

/** 调参请求和运行状态只由本模块维护。 */
typedef struct {
    app_pid_tune_status_t status;
    bool start_pending;
    bool stop_pending;
    uint32_t requested_motor_id;
    float requested_target_rpm;
    uint32_t started_ms;
    uint32_t phase_started_ms;
    bool saved_ff_enabled;
} app_pid_tune_runtime_t;

static app_pid_tune_runtime_t s_pid_tune;

bool app_control_pid_tune_start(uint32_t motor_id, float target_rpm)
{
    if (motor_id >= BSP_MOTOR_COUNT ||
        ((PRJ_MOTION_MOTOR_ACTIVE_MASK & (1UL << motor_id)) == 0UL) ||
        !isfinite(target_rpm) || fabsf(target_rpm) < 0.5f ||
        fabsf(target_rpm) > PRJ_PID_TUNE_TARGET_RPM_MAX) {
        return false;
    }

    bool accepted = false;
    OSAL_CRITICAL_SECTION {
        if (!s_pid_tune.status.active && !s_pid_tune.start_pending) {
            s_pid_tune.requested_motor_id = motor_id;
            s_pid_tune.requested_target_rpm = target_rpm;
            s_pid_tune.stop_pending = false;
            s_pid_tune.start_pending = true;
            accepted = true;
        }
    }
    return accepted;
}

void app_control_pid_tune_stop(void)
{
    OSAL_CRITICAL_SECTION {
        s_pid_tune.start_pending = false;
        if (s_pid_tune.status.active) {
            s_pid_tune.stop_pending = true;
        }
    }
}

void app_control_pid_tune_get_status(app_pid_tune_status_t *status)
{
    if (status == NULL) {
        return;
    }
    OSAL_CRITICAL_SECTION {
        *status = s_pid_tune.status;
    }
}

bool app_control_pid_tune_is_active(void)
{
    bool active;
    OSAL_CRITICAL_SECTION {
        active = s_pid_tune.status.active || s_pid_tune.start_pending;
    }
    return active;
}

#if (PRJ_SPEED_RPM_FILTER_ENABLE != 0U)
static float control_filter_alpha(float alpha_ref, float dt_s)
{
    const float dt_ref_s = PRJ_SPEED_RPM_FILTER_REFERENCE_PERIOD_MS /
                           (float)PRJ_MS_PER_S;
    if (alpha_ref <= 0.0f) return 0.0f;
    if (alpha_ref >= 1.0f) return 1.0f;
    const float tau_s = dt_ref_s * (1.0f - alpha_ref) / alpha_ref;
    return dt_s / (tau_s + dt_s);
}
#endif

#if PRJ_LINE_TRACK_ENABLE
/* 循迹外环只生成目标RPM，实际输出统一由本任务的速度PI完成。 */

typedef enum {
    LINE_LAP_IDLE = 0,
    LINE_LAP_WAIT_LEAVE_START,
    LINE_LAP_RUNNING,
    LINE_LAP_FINISH_ARMED,
    LINE_LAP_FINISH_ADVANCE
} line_lap_state_t;

typedef struct {
    line_lap_state_t state;
    uint32_t start_ms;
    uint32_t state_since_ms;
    uint32_t lost_since_ms;
    bool point_b_reported;
    int32_t start_counts[BSP_ENCODER_COUNT];
    float finish_trigger_distance_m;
} line_lap_manager_t;

static line_lap_manager_t s_line_lap;

/* 根据参与车体反馈的编码器累计值计算本次运行的平均行驶里程。 */
static float line_lap_get_distance_m(void)
{
    int32_t totals[BSP_ENCODER_COUNT];
    int64_t abs_count_sum = 0;
    uint32_t valid_count = 0U;

    if (bsp_encoder_get_all_totals(totals) != BSP_OK) {
        return 0.0f;
    }

    for (uint32_t motor = 0U; motor < BSP_MOTOR_COUNT; motor++) {
        if ((PRJ_MOTION_FEEDBACK_MASK & (1UL << motor)) == 0UL) {
            continue;
        }

        const uint32_t encoder = (uint32_t)s_motor_encoder_map[motor];
        if (encoder < BSP_ENCODER_COUNT) {
            int64_t delta = (int64_t)totals[encoder]
                          - (int64_t)s_line_lap.start_counts[encoder];
            if (delta < 0) {
                delta = -delta;
            }
            abs_count_sum += delta;
            valid_count++;
        }
    }

    if ((valid_count == 0U) || (PRJ_ENCODER_PULSES_PER_REV == 0U)) {
        return 0.0f;
    }

    const float average_counts = (float)abs_count_sum / (float)valid_count;
    return average_counts * (2.0f * PRJ_PI_F * PRJ_CF_WHEEL_RADIUS_M)
         / (float)PRJ_ENCODER_PULSES_PER_REV;
}

static void line_lap_start(uint32_t now_ms)
{
    (void)bsp_encoder_get_all_totals(s_line_lap.start_counts);
    s_line_lap.state = LINE_LAP_WAIT_LEAVE_START;
    s_line_lap.start_ms = now_ms;
    s_line_lap.state_since_ms = now_ms;
    s_line_lap.lost_since_ms = 0U;
    s_line_lap.point_b_reported = false;
    s_line_lap.finish_trigger_distance_m = 0.0f;
#if (LINE_TRACK_AUTO_STOP_ENABLE != 0U)
    (void)printf("[LINE] 单圈循迹启动，等待离开 A 点横线\r\n");
#else
    (void)printf("[LINE] 持续循迹启动，关闭一圈自动停车\r\n");
#endif
}

static void line_lap_reset(void)
{
    s_line_lap.state = LINE_LAP_IDLE;
    s_line_lap.start_ms = 0U;
    s_line_lap.state_since_ms = 0U;
    s_line_lap.lost_since_ms = 0U;
    s_line_lap.point_b_reported = false;
    s_line_lap.finish_trigger_distance_m = 0.0f;
}

static void line_lap_stop(app_shared_ctx_t *ctx, const char *reason,
                          float distance_m, uint32_t now_ms)
{
    const uint32_t elapsed_ms = now_ms - s_line_lap.start_ms;

    app_id_abort();
    app_line_track_stop();
    app_protocol_user_line_track_force_stop();
    app_motor_stop_all(ctx);

    OSAL_CRITICAL_SECTION {
        for (uint32_t i = 0U; i < BSP_MOTOR_COUNT; i++) {
            ctx->status.output[i] = 0;
            ctx->status.pid_correction[i] = 0.0f;
        }
    }

    (void)printf("[LINE] 停止：%s，里程=%.3fm，用时=%lu.%03lus\r\n",
        reason, (double)distance_m,
        (unsigned long)(elapsed_ms / 1000U),
        (unsigned long)(elapsed_ms % 1000U));
    line_lap_reset();
}

/* 返回 true 表示本周期已经执行停车。 */
static bool line_lap_update(app_shared_ctx_t *ctx,
                            const line_track_output_t *out,
                            uint32_t now_ms)
{
    if ((ctx == NULL) || (out == NULL) ||
        (s_line_lap.state == LINE_LAP_IDLE)) {
        return false;
    }

    const float distance_m = line_lap_get_distance_m();
    const uint32_t elapsed_ms = now_ms - s_line_lap.start_ms;

#if (LINE_TRACK_AUTO_STOP_ENABLE != 0U)
    if (elapsed_ms >= LINE_TRACK_MAX_RUN_MS) {
        line_lap_stop(ctx, "运行超时", distance_m, now_ms);
        return true;
    }
#endif

    /* 赛题A到B为1.5m，打印估算通过时间，便于检查“AB≤8s”。 */
    if ((!s_line_lap.point_b_reported) &&
        (distance_m >= LINE_TRACK_POINT_B_DISTANCE_M)) {
        s_line_lap.point_b_reported = true;
        (void)printf("[LINE] 估算通过 B 点，用时=%lu.%03lus，里程=%.3fm\r\n",
            (unsigned long)(elapsed_ms / 1000U),
            (unsigned long)(elapsed_ms % 1000U),
            (double)distance_m);
    }

    if (out->current_state == LINE_TRACK_STATE_LOST) {
        if (s_line_lap.lost_since_ms == 0U) {
            s_line_lap.lost_since_ms = now_ms;
        } else if ((now_ms - s_line_lap.lost_since_ms) >=
                   LINE_TRACK_LOST_STOP_MS) {
            line_lap_stop(ctx, "持续丢线", distance_m, now_ms);
            return true;
        }
    } else {
        s_line_lap.lost_since_ms = 0U;
    }

#if (LINE_TRACK_AUTO_STOP_ENABLE != 0U)
    switch (s_line_lap.state) {
    case LINE_LAP_WAIT_LEAVE_START:
        if (out->current_state != LINE_TRACK_STATE_CROSS) {
            if ((now_ms - s_line_lap.state_since_ms) >=
                LINE_TRACK_START_LEAVE_CONFIRM_MS) {
                s_line_lap.state = LINE_LAP_RUNNING;
                s_line_lap.state_since_ms = now_ms;
                (void)printf("[LINE] 已离开起点横线\r\n");
            }
        } else {
            s_line_lap.state_since_ms = now_ms;
        }
        break;

    case LINE_LAP_RUNNING:
        if (distance_m >= LINE_TRACK_LAP_ARM_DISTANCE_M) {
            s_line_lap.state = LINE_LAP_FINISH_ARMED;
            s_line_lap.state_since_ms = now_ms;
            (void)printf("[LINE] 终点识别已使能，里程=%.3fm\r\n",
                (double)distance_m);
        }
        break;

    case LINE_LAP_FINISH_ARMED:
        if (out->current_state == LINE_TRACK_STATE_CROSS) {
            if ((now_ms - s_line_lap.state_since_ms) >=
                LINE_TRACK_FINISH_CONFIRM_MS) {
                if (LINE_TRACK_FINISH_ADVANCE_M <= 0.0f) {
                    line_lap_stop(ctx, "完成一圈", distance_m, now_ms);
                    return true;
                }
                s_line_lap.finish_trigger_distance_m = distance_m;
                s_line_lap.state = LINE_LAP_FINISH_ADVANCE;
                s_line_lap.state_since_ms = now_ms;
            }
        } else {
            s_line_lap.state_since_ms = now_ms;
        }
        break;

    case LINE_LAP_FINISH_ADVANCE:
        if ((distance_m - s_line_lap.finish_trigger_distance_m) >=
            LINE_TRACK_FINISH_ADVANCE_M) {
            line_lap_stop(ctx, "完成一圈", distance_m, now_ms);
            return true;
        }
        break;

    case LINE_LAP_IDLE:
    default:
        break;
    }

#endif
    return false;
}
#endif

/** 完成或中止调参：立即停车，并恢复该电机原来的前馈开关。 */
static void pid_tune_finish(app_shared_ctx_t *ctx, uint32_t now_ms)
{
    uint32_t motor_id;
    bool restore_ff;
    bool was_active;

    OSAL_CRITICAL_SECTION {
        was_active = s_pid_tune.status.active;
        motor_id = s_pid_tune.status.motor_id;
        restore_ff = s_pid_tune.saved_ff_enabled;
        s_pid_tune.start_pending = false;
        s_pid_tune.stop_pending = false;
    }

    if (!was_active || motor_id >= BSP_MOTOR_COUNT) {
        return;
    }

    app_motor_stop_all(ctx);
    OSAL_CRITICAL_SECTION {
        app_ff_set_enabled(&ctx->ff[motor_id], restore_ff);
        ctx->pid[motor_id].use_ff = restore_ff;
        s_pid_tune.status.active = false;
        s_pid_tune.status.phase = APP_PID_TUNE_PHASE_COMPLETE;
        s_pid_tune.status.elapsed_ms =
            (uint32_t)(now_ms - s_pid_tune.started_ms);
        s_pid_tune.status.applied_pid_target_rpm = 0.0f;
        s_pid_tune.status.motor_command_applied = 0.0f;
    }
}

/** 在控制任务上下文中处理菜单提交的启动/停止请求。 */
static void pid_tune_process_requests(app_shared_ctx_t *ctx, uint32_t now_ms)
{
    bool start_pending;
    bool stop_pending;
    uint32_t motor_id;
    float target_rpm;

    OSAL_CRITICAL_SECTION {
        start_pending = s_pid_tune.start_pending;
        stop_pending = s_pid_tune.stop_pending;
        motor_id = s_pid_tune.requested_motor_id;
        target_rpm = s_pid_tune.requested_target_rpm;
    }

    if (stop_pending) {
        pid_tune_finish(ctx, now_ms);
        return;
    }
    if (!start_pending) {
        return;
    }

    bool saved_ff = false;
    OSAL_CRITICAL_SECTION {
        if (motor_id < BSP_MOTOR_COUNT) {
            saved_ff = ctx->ff[motor_id].enabled;
        }
    }

#if PRJ_LINE_TRACK_ENABLE
    app_line_track_stop();
    line_lap_reset();
#endif
    app_protocol_user_line_track_force_stop();
    app_id_abort();
    OSAL_CRITICAL_SECTION {
        float stopped_rpm[APP_POS_MOTOR_COUNT] = {0.0f};
        app_posctrl_emergency_stop(&ctx->posctrl);
        app_posctrl_set_mode(&ctx->posctrl, APP_CTRL_MODE_SPEED,
                             stopped_rpm, osal_get_tick_count());
    }
    app_motor_stop_all(ctx);

    OSAL_CRITICAL_SECTION {
        s_pid_tune.start_pending = false;
        s_pid_tune.stop_pending = false;
        s_pid_tune.saved_ff_enabled = saved_ff;
        s_pid_tune.started_ms = now_ms;
        s_pid_tune.phase_started_ms = now_ms;
        s_pid_tune.status.active = true;
        s_pid_tune.status.phase = APP_PID_TUNE_PHASE_SETTLE;
        s_pid_tune.status.motor_id = motor_id;
        s_pid_tune.status.elapsed_ms = 0U;
        s_pid_tune.status.requested_target_rpm = target_rpm;
        s_pid_tune.status.applied_pid_target_rpm = 0.0f;
        s_pid_tune.status.measured_rpm = 0.0f;
        s_pid_tune.status.error_rpm = 0.0f;
        s_pid_tune.status.controller_output_raw = 0.0f;
        s_pid_tune.status.motor_command_applied = 0.0f;
        s_pid_tune.status.p_term = 0.0f;
        s_pid_tune.status.i_term = 0.0f;
        s_pid_tune.status.d_term = 0.0f;
        s_pid_tune.status.kp = ctx->pid[motor_id].kp;
        s_pid_tune.status.ki = ctx->pid[motor_id].ki;
        s_pid_tune.status.kd = ctx->pid[motor_id].kd;
        app_ff_set_enabled(&ctx->ff[motor_id], false);
        ctx->pid[motor_id].use_ff = false;
    }
}

/** 更新固定静止等待和阶跃计时。 */
static void pid_tune_update_phase(app_shared_ctx_t *ctx, uint32_t now_ms,
                                  const int32_t rpm[BSP_MOTOR_COUNT])
{
    app_pid_tune_status_t status;
    app_control_pid_tune_get_status(&status);
    if (!status.active || status.motor_id >= BSP_MOTOR_COUNT) {
        return;
    }

    const uint32_t phase_elapsed_ms =
        (uint32_t)(now_ms - s_pid_tune.phase_started_ms);

    if (status.phase == APP_PID_TUNE_PHASE_SETTLE &&
        phase_elapsed_ms >= PRJ_PID_TUNE_SETTLE_MS) {
        OSAL_CRITICAL_SECTION {
            app_pid_reset(&ctx->pid[status.motor_id]);
            app_pid_set_setpoint(&ctx->pid[status.motor_id],
                                 status.requested_target_rpm);
            ctx->motor_enabled[status.motor_id] = true;
            s_pid_tune.phase_started_ms = now_ms;
            s_pid_tune.status.phase = APP_PID_TUNE_PHASE_STEP;
            s_pid_tune.status.applied_pid_target_rpm =
                status.requested_target_rpm;
        }
        status.phase = APP_PID_TUNE_PHASE_STEP;
    } else if (status.phase == APP_PID_TUNE_PHASE_STEP &&
               phase_elapsed_ms >= PRJ_PID_TUNE_STEP_MS) {
        pid_tune_finish(ctx, now_ms);
        return;
    }

    OSAL_CRITICAL_SECTION {
        s_pid_tune.status.elapsed_ms =
            (uint32_t)(now_ms - s_pid_tune.started_ms);
        s_pid_tune.status.measured_rpm = (float)rpm[status.motor_id];
        if (s_pid_tune.status.phase == APP_PID_TUNE_PHASE_SETTLE) {
            s_pid_tune.status.error_rpm = -(float)rpm[status.motor_id];
            s_pid_tune.status.applied_pid_target_rpm = 0.0f;
        }
    }
}

/*
 * 位置控制输出与编码器BSP都使用LF/LB/RF/RB车轮顺序。若任一模块改变
 * 索引约定，编译必须失败，避免角度模式把左右轮目标写到错误电机。
 */
typedef char posctrl_lf_index_must_match_encoder[
    (APP_POS_MOTOR_LF == BSP_ENCODER_LF) ? 1 : -1];
typedef char posctrl_lb_index_must_match_encoder[
    (APP_POS_MOTOR_LB == BSP_ENCODER_LB) ? 1 : -1];
typedef char posctrl_rf_index_must_match_encoder[
    (APP_POS_MOTOR_RF == BSP_ENCODER_RF) ? 1 : -1];
typedef char posctrl_rb_index_must_match_encoder[
    (APP_POS_MOTOR_RB == BSP_ENCODER_RB) ? 1 : -1];

void app_control_task(void *param)
{
    app_shared_ctx_t *ctx = (app_shared_ctx_t *)param;

    const float dt_s = (float)PRJ_CONTROL_PERIOD_MS / (float)PRJ_MS_PER_S;
    const float outer_dt_s = (float)POSCTRL_OUTER_PERIOD_MS /
                             (float)PRJ_MS_PER_S;
#if (PRJ_SPEED_SCURVE_ENABLE != 0U)
    const float speed_planner_dt_s = (float)PRJ_SPEED_PLANNER_PERIOD_MS /
                                     (float)PRJ_MS_PER_S;
#endif
#if (PRJ_SPEED_RPM_FILTER_ENABLE != 0U)
    const float rpm_filter_alpha_low = control_filter_alpha(
        PRJ_SPEED_RPM_FILTER_ALPHA_LOW, dt_s);
    const float rpm_filter_alpha_high = control_filter_alpha(
        PRJ_SPEED_RPM_FILTER_ALPHA_HIGH, dt_s);
#endif

    /* 堵转检测: >1s无脉冲强制归零 */
    static uint32_t s_last_edge_tick[BSP_MOTOR_COUNT] = {0};

    /*
     * 速度环整形状态：
     * - target_applied保存斜坡后的目标，避免80/100RPM阶跃直接冲击机械死区；
     * - output_applied限制PWM命令变化率，降低轮胎变形和齿隙引起的低速顿挫；
     * - heading_assist仅在双轮直行时使用yaw/gyro_z修正左右目标差。
     */
    static float s_target_applied[BSP_MOTOR_COUNT] = {0.0f};
    static float s_output_applied[BSP_MOTOR_COUNT] = {0.0f};
    static app_chassis_assist_t s_heading_assist = {0};
    static app_ctrl_mode_t s_previous_control_mode = APP_CTRL_MODE_SPEED;
    static bool s_previous_motor_enabled[BSP_MOTOR_COUNT] = {false};
#if (PRJ_SPEED_SCURVE_ENABLE != 0U)
    static app_speed_planner_t s_speed_planner[BSP_MOTOR_COUNT];
#endif

    /* 外环周期计数器（2ms × 10 = 20ms）。 */
    uint32_t outer_counter = 0U;
#if (PRJ_SPEED_SCURVE_ENABLE != 0U)
    /* 使第一次控制循环立即执行一次规划更新。 */
    uint32_t speed_planner_counter = SPEED_PLANNER_RATIO - 1U;
#endif
    uint32_t last_imu_timestamp_ms = 0U;

#if (PRJ_SPEED_SCURVE_ENABLE != 0U)
    for (uint32_t i = 0U; i < BSP_MOTOR_COUNT; i++) {
        app_speed_planner_init(&s_speed_planner[i],
            PRJ_SPEED_SCURVE_MAX_ACCEL_RPM_S,
            PRJ_SPEED_SCURVE_MAX_JERK_RPM_S2);
        app_speed_planner_reset(&s_speed_planner[i], 0.0f);
    }
#endif

#if (PRJ_DRV8870_FACTORY_TEST_ENABLE == 0U)
    /* Production: enable once in task context, then wait before any command. */
    if (bsp_motor_power_enable() != BSP_OK) {
        bsp_motor_power_disable();
        AX_LOG_ERROR("Motor power enable failed; control task inhibited");
        for (;;) {
            osal_task_delay_ms(1000U);
        }
    }
    osal_task_delay_ms(PRJ_MOTOR_POWER_STARTUP_MS);
#endif

    uint32_t last_wake_tick = osal_get_tick_count();

    for (;;) {
        uint32_t control_now_ms = osal_ticks_to_ms(osal_get_tick_count());
        const bool outer_update_due = (++outer_counter >= POSCTRL_OUTER_RATIO);
        if (outer_update_due) outer_counter = 0U;
#if (PRJ_SPEED_SCURVE_ENABLE != 0U)
        const bool speed_planner_update_due =
            (++speed_planner_counter >= SPEED_PLANNER_RATIO);
        if (speed_planner_update_due) speed_planner_counter = 0U;
#endif
        bool line_track_active = false;
        bool line_track_fault_stopped = false;

        pid_tune_process_requests(ctx, control_now_ms);
        bool pid_tune_active = app_control_pid_tune_is_active();
#if (PRJ_KEY_ENABLE != 0U)
        /* 调参期间屏蔽按键运动请求，避免改变固定初始条件。 */
        if (!pid_tune_active) {
            app_key_motion_process(control_now_ms);
        }
#endif
        /* Position-loop telemetry is independent of RPM calculation and PWM control. */
        app_encoder_telemetry_process(control_now_ms);

#if PRJ_LINE_TRACK_ENABLE
        /*
         * 模块4是独立的循迹控制模式：
         * - 协议任务只修改请求标志，不直接操作电机；
         * - 控制任务在2ms周期内完成启停切换和循迹目标更新；
         * - 循迹外环给出左右轮目标RPM，现有增量式速度PI负责最终输出。
         */
        if (pid_tune_active) {
            app_protocol_user_line_track_force_stop();
        }
        const bool line_track_requested =
            app_protocol_user_line_track_is_enabled();

        if (app_protocol_user_line_track_take_reset_request()) {
            app_line_track_reset();
        }

        if (line_track_requested && !app_line_track_is_running()) {
            /* 切换控制器前先清除旧PID目标和正在进行的模型辨识。 */
            app_id_abort();
            app_motor_stop_all(ctx);
            app_posctrl_set_mode(
                &ctx->posctrl, APP_CTRL_MODE_SPEED, NULL, control_now_ms);

            /*
             * app_motor_stop_all()会清除motor_enabled。循迹改用速度PI后，
             * 必须只重新使能实际参与运动的电机，并清空旧PID历史。
             */
            OSAL_CRITICAL_SECTION {
                for (uint32_t i = 0U; i < BSP_MOTOR_COUNT; i++) {
                    app_pid_reset(&ctx->pid[i]);
                    app_pid_set_setpoint(&ctx->pid[i], 0.0f);
                    ctx->motor_enabled[i] =
                        ((PRJ_MOTION_MOTOR_ACTIVE_MASK & (1UL << i)) != 0UL);
                }
            }

            app_line_track_start();
            line_lap_start(control_now_ms);
        } else if (!line_track_requested && app_line_track_is_running()) {
            /* 停止循迹后不自动恢复旧目标，等待后续标准使能/运行流程。 */
            app_id_abort();
            app_line_track_stop();
            app_motor_stop_all(ctx);
            line_lap_reset();
        }

        /*
         * 循迹IMU辅助由LINE_TRACK_IMU_ASSIST_ENABLE控制。
         * 先传最新快照再输出，避免起步后多等一个控制周期才开始抑制偏航。
         */
        app_imu_data_t line_imu_local;
        OSAL_CRITICAL_SECTION {
            line_imu_local = ctx->imu;
        }
        const uint32_t line_imu_age_ms =
            (line_imu_local.timestamp_ms == 0U) ? UINT32_MAX :
            (uint32_t)(control_now_ms - line_imu_local.timestamp_ms);
        app_line_track_set_imu_feedback(
            line_imu_local.yaw,
            line_imu_local.gyro_z_dps,
            line_imu_local.accel_x_g,
            line_imu_local.accel_y_g,
            line_imu_local.accel_z_g,
            line_imu_local.timestamp_ms,
            line_imu_age_ms);

        line_track_active = app_line_track_is_running();
        if (line_track_active && app_line_track_step()) {
            const line_track_output_t *line_out =
                app_line_track_get_output();

            /* 电机状态由后面的速度PI输出阶段统一更新。 */
            if (line_lap_update(ctx, line_out, control_now_ms)) {
                line_track_fault_stopped = true;
                line_track_active = false;
            }
        }
#endif

        /* M/T法测速: 读取边沿数和时间戳, 换算RPM
         * 高速用M/T法, 低速用T法, 无脉冲时平滑衰减 */
        bool encoder_had_edge[BSP_ENCODER_COUNT];
        int32_t encoder_rpm[BSP_ENCODER_COUNT];
        bool had_edge[BSP_MOTOR_COUNT];
        int32_t rpm_local[BSP_MOTOR_COUNT];
        (void)bsp_encoder_get_all_rpm_mt(encoder_rpm, encoder_had_edge);

        /* 启动五通道ADC序列，不阻塞2ms控制环。 */
        (void)bsp_adc_start_all();

        /*
         * BSP编码器数组按车轮位置排列，控制器数组按电机接口排列。
         * 显式重排可防止M1输出错误地闭环到M3/LF编码器。
         */
        for (uint32_t i = 0U; i < BSP_MOTOR_COUNT; i++) {
            uint32_t encoder_id = (uint32_t)s_motor_encoder_map[i];
            if (encoder_id < BSP_ENCODER_COUNT) {
                rpm_local[i] = encoder_rpm[encoder_id];
                had_edge[i] = encoder_had_edge[encoder_id];
            } else {
                rpm_local[i] = 0;
                had_edge[i] = false;
            }
        }

        /* 堵转检测: >1s无脉冲强制归零 */
        {
            uint32_t now = osal_get_tick_count();
            for (uint32_t i = 0; i < BSP_MOTOR_COUNT; i++) {
                if (had_edge[i]) {
                    s_last_edge_tick[i] = now;
                }
                if ((now - s_last_edge_tick[i])
                    > osal_ms_to_ticks(1000)) {
                    rpm_local[i] = 0;
                }
            }
        }

#if (PRJ_SPEED_RPM_FILTER_ENABLE != 0U)
        /* 一阶低通抑制低速量化跳变；高通会放大边沿噪声，因此不采用。 */
        {
            static float rpm_filtered[BSP_MOTOR_COUNT] = {0.0f};
            static bool filter_initialized = false;

            for (uint32_t i = 0U; i < BSP_MOTOR_COUNT; i++) {
                const float raw = (float)rpm_local[i];
                const float alpha = (fabsf(raw) < PRJ_SPEED_RPM_FILTER_SWITCH_RPM) ?
                    rpm_filter_alpha_low : rpm_filter_alpha_high;

                if (!filter_initialized) {
                    rpm_filtered[i] = raw;
                } else {
                    rpm_filtered[i] += alpha * (raw - rpm_filtered[i]);
                }

                rpm_local[i] = (int32_t)(rpm_filtered[i] +
                    ((rpm_filtered[i] >= 0.0f) ? 0.5f : -0.5f));
            }
            filter_initialized = true;
        }
#endif

#if PRJ_LINE_TRACK_ENABLE
        /*
         * A/M1是右驱动轮，D/M4是左驱动轮。反馈保留给可选辅助模块；
         * 实际左右轮速度闭环由下方现有增量式PI统一完成。
         */
        app_line_track_set_speed_feedback(
            rpm_local[BSP_MOTOR_D], rpm_local[BSP_MOTOR_A]);
#endif

        OSAL_CRITICAL_SECTION {
            for (uint32_t i = 0; i < BSP_MOTOR_COUNT; i++) {
                ctx->status.rpm[i] = rpm_local[i];
            }
        }

        /* The ADC sequence has run while encoder and filter work was executing. */
        float current_ma_local[BSP_MOTOR_COUNT];
        bsp_adc_get_all_currents_ma(current_ma_local);
        const uint32_t bus_voltage_mv = bsp_adc_get_bus_voltage_mv();

        OSAL_CRITICAL_SECTION {
            for (uint32_t i = 0U; i < BSP_MOTOR_COUNT; i++) {
                ctx->status.current_ma[i] = current_ma_local[i];
            }
            ctx->status.bus_voltage_mv = bus_voltage_mv;
        }

        /* 连续过流达到阈值后停止；循迹模式必须一次性停止全部电机。 */
        for (uint32_t i = 0U; i < BSP_MOTOR_COUNT; i++) {
            if (current_ma_local[i] > (float)PRJ_ADC_CURRENT_OVERLOAD_MA) {
                if (ctx->overload_cnt[i] < PRJ_ADC_CURRENT_OVERLOAD_TICKS) {
                    ctx->overload_cnt[i]++;
                }
                if (ctx->overload_cnt[i] == PRJ_ADC_CURRENT_OVERLOAD_TICKS) {
#if PRJ_LINE_TRACK_ENABLE
                    if (line_track_active && !line_track_fault_stopped) {
                        /* 过流时撤销循迹请求，防止下一周期按旧请求自动重启。 */
                        line_track_fault_stopped = true;
                        line_track_active = false;
                        app_id_abort();
                        app_line_track_stop();
                        app_protocol_user_line_track_force_stop();
                        app_motor_stop_all(ctx);
                        OSAL_CRITICAL_SECTION {
                            for (uint32_t j = 0U; j < BSP_MOTOR_COUNT; j++) {
                                ctx->status.output[j] = 0;
                                ctx->status.pid_correction[j] = 0.0f;
                            }
                        }
                        (void)printf("[WARN] Line track overcurrent: M%lu=%dmA, all motors stopped\r\n",
                            (unsigned long)i, (int)current_ma_local[i]);
                    } else
#endif
                    {
                        /* 普通速度/PID模式保持原有的单路停机行为。 */
                        app_motor_stop(ctx, i);

                        /* 调参电机过流时立即结束本次测试并恢复前馈配置。 */
                        app_pid_tune_status_t tune_status;
                        app_control_pid_tune_get_status(&tune_status);
                        if (tune_status.active && tune_status.motor_id == i) {
                            pid_tune_finish(ctx, control_now_ms);
                            (void)printf("[PIDTUNE] aborted: overcurrent\r\n");
                        }

                        (void)printf("[WARN] Motor %lu overcurrent: %dmA\r\n",
                            (unsigned long)i, (int)current_ma_local[i]);
                    }
                }
            } else {
                ctx->overload_cnt[i] = 0U;
            }
        }

        /* 互补滤波: 融合编码器RPM和IMU数据 */
        {
            /* 编码器线速度: v = rpm * 2π * r / 60 (按反馈轮平均) */
            float avg_rpm = 0.0f;
            uint32_t active_count = 0U;
            for (uint32_t i = 0U; i < BSP_MOTOR_COUNT; i++) {
                if ((PRJ_MOTION_FEEDBACK_MASK & (1UL << i)) != 0UL) {
                    avg_rpm += (float)rpm_local[i];
                    active_count++;
                }
            }
            if (active_count != 0U) {
                avg_rpm /= (float)active_count;
            }
            float encoder_vx = avg_rpm * 2.0f * PRJ_PI_F
                * PRJ_CF_WHEEL_RADIUS_M / 60.0f;

            /* 读取IMU数据(临界区保护) */
            float imu_accel_x, imu_yaw;
            OSAL_CRITICAL_SECTION {
                imu_accel_x = ctx->imu.accel_x_g;
                imu_yaw     = ctx->imu.yaw;
            }

            /* 更新互补滤波器 */
            app_cf_update(encoder_vx, imu_accel_x, imu_yaw, dt_s);
        }

        pid_tune_update_phase(ctx, control_now_ms, rpm_local);
        app_pid_tune_status_t pid_tune_status;
        app_control_pid_tune_get_status(&pid_tune_status);
        pid_tune_active = pid_tune_status.active;

        /* ---- 模型参数辨识: 控制任务周期调用(纯算法) ---- */
        app_id_cycle_out_t id_out = { ID_ACTION_NONE, 0, 0U };
        if (!pid_tune_active && !line_track_active && !line_track_fault_stopped) {
            app_id_control_cycle(rpm_local, &id_out);
        }
        bool id_active = !line_track_active && !line_track_fault_stopped &&
                         (id_out.action != ID_ACTION_NONE);

        /* ---- 位置/角度外环(20ms, 相对500Hz内环10倍降频) ----
         * SPEED模式: 不修改速度环setpoint(保持现有行为)
         * POSITION/ANGLE模式: 计算target_rpm并写入速度环setpoint
         */
        if (!pid_tune_active && !line_track_active &&
            !line_track_fault_stopped && outer_update_due) {

            /* 读取编码器累计脉冲(反馈轮平均, 不清零) */
            int32_t enc_totals[BSP_ENCODER_COUNT];
            (void)bsp_encoder_get_all_totals(enc_totals);
            float enc_avg = 0.0f;
            app_pos_feedback_t pos_feedback = {0};
            uint32_t active_encoder_count = 0U;
            for (uint32_t i = 0U; i < BSP_MOTOR_COUNT; i++) {
                if ((PRJ_MOTION_FEEDBACK_MASK & (1UL << i)) != 0UL) {
                    const uint32_t encoder_id = (uint32_t)s_motor_encoder_map[i];
                    if (encoder_id < BSP_ENCODER_COUNT) {
                        pos_feedback.position[encoder_id] =
                            (float)enc_totals[encoder_id];
                        pos_feedback.rpm[encoder_id] = (float)rpm_local[i];
                        pos_feedback.valid_mask |= (1UL << encoder_id);
                        enc_avg += (float)enc_totals[encoder_id];
                        active_encoder_count++;
                    }
                }
            }
            if (active_encoder_count != 0U) {
                enc_avg /= (float)active_encoder_count;
            }
            pos_feedback.average_position = enc_avg;

            /* 读取KF yaw(IMU任务写入) */
            float kf_yaw;
            OSAL_CRITICAL_SECTION {
                kf_yaw = ctx->imu.yaw;
            }

            /* 外环更新(临界区保护, 防止与menu/vofa的set_mode冲突) */
            const app_pos_output_t *pos_out;
            uint32_t now_tick = osal_get_tick_count();
            OSAL_CRITICAL_SECTION {
                pos_out = app_posctrl_update_feedback(&ctx->posctrl,
                    &pos_feedback, kf_yaw, outer_dt_s, now_tick);
            }

            /*
             * POSITION/ANGLE模式:
             * pos_out按车轮顺序(LF/LB/RF/RB)，PID按电机接口顺序(A/B/C/D)。
             * 复用电机-车轮映射，确保A/M1(RB)、B/M2(RF)、C/M3(LF)、
             * D/M4(LB)取得各自物理车轮的目标RPM。
             */
            if (pos_out != NULL &&
                pos_out->mode != APP_CTRL_MODE_SPEED) {
                for (uint32_t i = 0U; i < BSP_MOTOR_COUNT; i++) {
                    if ((PRJ_MOTION_MOTOR_ACTIVE_MASK & (1UL << i)) == 0UL) {
                        /* 未开放的输出通道安全置零。 */
                        app_pid_set_setpoint(&ctx->pid[i], 0.0f);
                        ctx->motor_enabled[i] = false;
                        continue;
                    }
                    uint32_t wheel_id = (uint32_t)s_motor_encoder_map[i];
                    if (wheel_id < APP_POS_MOTOR_COUNT) {
                        app_pid_set_setpoint(&ctx->pid[i],
                            pos_out->target_rpm[wheel_id]);
                    } else {
                        app_pid_set_setpoint(&ctx->pid[i], 0.0f);
                    }
                }
            }
        }

        /*
         * 生成本周期真正送入速度 PID 的目标：
         * 1. 用户/协议设置的 pid.setpoint 保持不变；
         * 2. SPEED 模式按宏选择 S 型规划或原线性斜坡；
         * 3. 双轮直行时再叠加 IMU 航向修正；
         * 4. POSITION/ANGLE 模式保持原有外环输出，不重复规划。
         */
        float requested_target[BSP_MOTOR_COUNT];
        float control_target[BSP_MOTOR_COUNT];
        bool motor_enabled_local[BSP_MOTOR_COUNT];
        app_ctrl_mode_t control_mode;
        app_imu_data_t imu_local;

        OSAL_CRITICAL_SECTION {
            control_mode = app_posctrl_get_mode(&ctx->posctrl);
            imu_local = ctx->imu;
            for (uint32_t i = 0U; i < BSP_MOTOR_COUNT; i++) {
                const float setpoint = ctx->pid[i].setpoint;
                /* 非法目标按 0 处理，禁止 NaN 进入规划器、PID 和电机输出。 */
                requested_target[i] = isfinite(setpoint) ? setpoint : 0.0f;
                motor_enabled_local[i] = ctx->motor_enabled[i] &&
                    ((PRJ_MOTION_MOTOR_ACTIVE_MASK & (1UL << i)) != 0UL);
            }
        }

        const bool entering_speed_mode =
            (control_mode == APP_CTRL_MODE_SPEED) &&
            (s_previous_control_mode != APP_CTRL_MODE_SPEED);

        if (pid_tune_active) {
            /* 调参只运行一个电机，并直接给PID阶跃目标。 */
            app_chassis_assist_reset(&s_heading_assist);
            for (uint32_t i = 0U; i < BSP_MOTOR_COUNT; i++) {
#if (PRJ_SPEED_SCURVE_ENABLE != 0U)
                app_speed_planner_reset(&s_speed_planner[i], 0.0f);
#endif
                s_target_applied[i] = 0.0f;
                s_output_applied[i] = 0.0f;
                control_target[i] =
                    (pid_tune_status.phase == APP_PID_TUNE_PHASE_STEP &&
                     i == pid_tune_status.motor_id && motor_enabled_local[i]) ?
                    pid_tune_status.requested_target_rpm : 0.0f;
            }
#if PRJ_LINE_TRACK_ENABLE
        } else if (line_track_active) {
            const line_track_output_t *line_out =
                app_line_track_get_output();

            /*
             * 循迹目标直接进入速度PI，不经过S型规划或线性目标斜坡。
             * A/B映射右轮，C/D映射左轮；未启用通道始终保持0RPM。
             */
            app_chassis_assist_reset(&s_heading_assist);
            for (uint32_t i = 0U; i < BSP_MOTOR_COUNT; i++) {
                float target_rpm = 0.0f;
#if (PRJ_SPEED_SCURVE_ENABLE != 0U)
                app_speed_planner_reset(&s_speed_planner[i], 0.0f);
#endif
                if (line_out != NULL) {
                    target_rpm = (i < 2U) ?
                        line_out->right_target_rpm :
                        line_out->left_target_rpm;
                }
                if ((PRJ_MOTION_MOTOR_ACTIVE_MASK & (1UL << i)) == 0UL) {
                    target_rpm = 0.0f;
                }

                s_target_applied[i] = target_rpm;
                control_target[i] = target_rpm;
            }
#endif
        } else if (line_track_fault_stopped || id_active) {
            /* 停车或辨识接管时清空普通控制器状态。 */
            app_chassis_assist_reset(&s_heading_assist);
            for (uint32_t i = 0U; i < BSP_MOTOR_COUNT; i++) {
#if (PRJ_SPEED_SCURVE_ENABLE != 0U)
                app_speed_planner_reset(&s_speed_planner[i], 0.0f);
#endif
                s_target_applied[i] = 0.0f;
                s_output_applied[i] = 0.0f;
                control_target[i] = 0.0f;
            }
        } else {
            const float linear_target_step =
                APP_CHASSIS_TARGET_SLEW_RPM_PER_S * dt_s;

            for (uint32_t i = 0U; i < BSP_MOTOR_COUNT; i++) {
                if (!motor_enabled_local[i]) {
                    /* 禁用、STOP 和过流均先由电机层立即停车，这里只清规划状态。 */
#if (PRJ_SPEED_SCURVE_ENABLE != 0U)
                    app_speed_planner_reset(&s_speed_planner[i], 0.0f);
#endif
                    s_target_applied[i] = 0.0f;
                    s_output_applied[i] = 0.0f;
                    control_target[i] = 0.0f;
                } else if (control_mode == APP_CTRL_MODE_SPEED) {
#if (PRJ_SPEED_SCURVE_ENABLE != 0U)
                    /*
                     * 从位置/角度模式切回，或电机重新使能时，以实测 RPM 为起点，
                     * 避免目标从 0 或旧状态突然跳变。
                     */
                    if (entering_speed_mode || !s_previous_motor_enabled[i]) {
                        app_speed_planner_reset(
                            &s_speed_planner[i], (float)rpm_local[i]);
                        s_target_applied[i] = (float)rpm_local[i];
                    }

                    if (app_speed_planner_is_valid(&s_speed_planner[i]) &&
                        speed_planner_update_due) {
                        (void)app_speed_planner_update(
                            &s_speed_planner[i], requested_target[i],
                            speed_planner_dt_s);
                    }

                    if (app_speed_planner_is_valid(&s_speed_planner[i])) {
                        s_target_applied[i] = app_speed_planner_get_speed(
                            &s_speed_planner[i]);
                    } else {
                        /*
                         * 参数或运行状态异常时自动回退到原线性斜坡。
                         * 该回退不需要重启，也不会绕过目标变化率限制。
                         */
                        s_target_applied[i] = app_chassis_slew(
                            s_target_applied[i], requested_target[i],
                            linear_target_step);
                    }
#else
                    /* 编译期回退：完全保留原线性目标斜坡。 */
                    s_target_applied[i] = app_chassis_slew(
                        s_target_applied[i], requested_target[i],
                        linear_target_step);
#endif
                    control_target[i] = s_target_applied[i];
                } else {
                    /* 位置/角度外环已有规划器，不重复限速。 */
#if (PRJ_SPEED_SCURVE_ENABLE != 0U)
                    app_speed_planner_reset(&s_speed_planner[i], 0.0f);
#endif
                    s_target_applied[i] = requested_target[i];
                    control_target[i] = requested_target[i];
                    s_output_applied[i] = 0.0f;
                }
            }

            if ((control_mode == APP_CTRL_MODE_SPEED) &&
                motor_enabled_local[BSP_MOTOR_A] &&
                motor_enabled_local[BSP_MOTOR_D]) {
                const float left_target = control_target[BSP_MOTOR_D];
                const float right_target = control_target[BSP_MOTOR_A];
                const float average_target = 0.5f * (left_target + right_target);
                const bool straight_request =
                    ((left_target * right_target) > 0.0f) &&
                    (fabsf(average_target) >= APP_CHASSIS_ASSIST_MIN_RPM) &&
                    (fabsf(left_target - right_target) <=
                        APP_CHASSIS_ASSIST_TARGET_MATCH_RPM);
                const uint32_t imu_age_ms = (imu_local.timestamp_ms == 0U) ?
                    UINT32_MAX : (uint32_t)(control_now_ms - imu_local.timestamp_ms);

                if (!straight_request ||
                    (imu_age_ms > APP_CHASSIS_ASSIST_IMU_MAX_AGE_MS)) {
                    app_chassis_assist_reset(&s_heading_assist);
                } else if (imu_local.timestamp_ms != last_imu_timestamp_ms) {
                    /* IMU 为 100Hz，只在出现新样本时更新滤波和航向环。 */
                    float imu_dt_s = (float)PRJ_IMU_TASK_PERIOD_MS /
                                     (float)PRJ_MS_PER_S;
                    if (last_imu_timestamp_ms != 0U) {
                        const uint32_t imu_dt_ms =
                            (uint32_t)(imu_local.timestamp_ms - last_imu_timestamp_ms);
                        if ((imu_dt_ms > 0U) && (imu_dt_ms <= 100U)) {
                            imu_dt_s = (float)imu_dt_ms / (float)PRJ_MS_PER_S;
                        }
                    }
                    last_imu_timestamp_ms = imu_local.timestamp_ms;
                    (void)app_chassis_assist_update(
                        &s_heading_assist, left_target, right_target,
                        imu_local.yaw, imu_local.gyro_z_dps,
                        imu_local.accel_x_g, imu_local.accel_y_g,
                        imu_local.accel_z_g, imu_age_ms, imu_dt_s);
                }

                /* 无新 IMU 样本时保持上次修正；速度 PID 仍按 500Hz 执行。 */
                const float correction_rpm = s_heading_assist.correction_rpm;
                control_target[BSP_MOTOR_D] -= correction_rpm;
                control_target[BSP_MOTOR_A] += correction_rpm;
            } else {
                app_chassis_assist_reset(&s_heading_assist);
            }
        }

        /* 保存边沿状态，下一周期用于无冲击模式切换和重新使能。 */
        s_previous_control_mode = control_mode;
        for (uint32_t i = 0U; i < BSP_MOTOR_COUNT; i++) {
            s_previous_motor_enabled[i] = motor_enabled_local[i];
        }

        /* PID计算 + 电机输出(仅使能电机) */
        for (uint32_t i = 0; i < BSP_MOTOR_COUNT; i++) {
            /* 安全停车后禁止继续输出；正常循迹必须进入速度PI。 */
            if (line_track_fault_stopped) {
                continue;
            }
            /* 辨识模式下跳过目标电机(PWM由id_out控制) */
            if (!pid_tune_active && id_active && i == id_out.motor_id &&
                ((PRJ_MOTION_MOTOR_ACTIVE_MASK & (1UL << i)) != 0UL)) {
                OSAL_CRITICAL_SECTION {
                    ctx->status.output[i] = id_out.pwm;
                }
                if (id_out.action == ID_ACTION_BRAKE) {
                    (void)bsp_motor_stop((bsp_motor_id_t)i,
                        BSP_MOTOR_MODE_BRAKE);
                } else if (id_out.action == ID_ACTION_APPLY_PWM) {
                    (void)bsp_motor_set_speed((bsp_motor_id_t)i,
                        id_out.pwm);
                }
                continue;
            }
            const float feedback = (float)rpm_local[i];
            const float target = control_target[i];
            float output_local = 0.0f;
            float controller_output_raw = 0.0f;
            app_pid_terms_t tune_terms = {0};
            const bool tune_motor = pid_tune_active &&
                pid_tune_status.phase == APP_PID_TUNE_PHASE_STEP &&
                i == pid_tune_status.motor_id;
            const bool enabled = motor_enabled_local[i];
            bool nan_detected = false;

            if (!enabled) {
                s_output_applied[i] = 0.0f;
                OSAL_CRITICAL_SECTION {
                    ctx->status.output[i] = 0;
                    ctx->status.pid_correction[i] = 0.0f;
                }
            } else if (fabsf(target) < 0.5f) {
                /*
                 * 目标经过斜坡到 0 后立即清空增量式 PID 状态。
                 * 禁止在零速附近累加并反复跨过机械死区。
                 */
                if ((control_mode == APP_CTRL_MODE_SPEED) &&
                    (fabsf(requested_target[i]) < 0.5f)) {
#if (PRJ_SPEED_SCURVE_ENABLE != 0U)
                    app_speed_planner_reset(&s_speed_planner[i], 0.0f);
#endif
                    s_target_applied[i] = 0.0f;
                }
                app_pid_reset(&ctx->pid[i]);
                s_output_applied[i] = 0.0f;
                OSAL_CRITICAL_SECTION {
                    ctx->status.output[i] = 0;
                    ctx->status.pid_correction[i] = 0.0f;
                }
            } else {
                /* 循迹统一使用标准增量式PI，避免旧前馈参数改变左右轮响应。 */
                const bool ff_enabled = line_track_active ?
                    false : ctx->ff[i].enabled;
                ctx->pid[i].use_ff = ff_enabled;

                if (ff_enabled) {
                    /* FF模式仍使用本周期斜坡/航向修正后的目标。 */
                    float ff_duty = app_ff_compute(&ctx->ff[i], target);
                    float pid_corr = app_pid_compute_target(
                        &ctx->pid[i], target, feedback, dt_s);
                    float current_corr = app_ff_compute_current_correction(
                        &ctx->ff[i], target, current_ma_local[i]);

                    if (!isfinite(ff_duty) || !isfinite(pid_corr) ||
                        !isfinite(current_corr)) {
                        nan_detected = true;
                        ff_duty = 0.0f;
                        pid_corr = 0.0f;
                        current_corr = 0.0f;
                        app_pid_reset(&ctx->pid[i]);
                    }

                    OSAL_CRITICAL_SECTION {
                        ctx->status.pid_correction[i] = pid_corr;
                    }
                    output_local = ff_duty + pid_corr + current_corr;
                } else {
                    /* 调参时额外返回本周期P/I/D诊断项；普通模式保持原接口。 */
                    if (tune_motor) {
                        output_local = app_pid_compute_target_diag(
                            &ctx->pid[i], target, feedback, dt_s, &tune_terms);
                    } else {
                        output_local = app_pid_compute_target(
                            &ctx->pid[i], target, feedback, dt_s);
                    }

                    if (!isfinite(output_local)) {
                        nan_detected = true;
                        output_local = 0.0f;
                        app_pid_reset(&ctx->pid[i]);
                    }
                    OSAL_CRITICAL_SECTION {
                        ctx->status.pid_correction[i] = 0.0f;
                    }
                }

                controller_output_raw = tune_motor ?
                    tune_terms.output_raw : output_local;

                /*
                 * 正速度目标禁止PID瞬间给出反向命令，负目标同理。
                 * 这能避免编码器量化噪声使电机在低速时正反打齿抖动。
                 */
                if (((target > 0.0f) && (output_local < 0.0f)) ||
                    ((target < 0.0f) && (output_local > 0.0f))) {
                    output_local = 0.0f;
                }

                const float command_max = (float)bsp_motor_get_command_max();
                output_local = app_chassis_clamp(
                    output_local, -command_max, command_max);

                if ((control_mode == APP_CTRL_MODE_SPEED) &&
                    !line_track_active && !tune_motor) {
                    output_local = app_chassis_slew(
                        s_output_applied[i],
                        output_local,
                        APP_CHASSIS_OUTPUT_SLEW_PER_S * dt_s);
                    s_output_applied[i] = output_local;

                    /* 增量式PID内部保存上次输出，必须与实际限速后的命令同步。 */
                    if (!ff_enabled &&
                        (ctx->pid[i].mode == APP_PID_MODE_INCREMENT)) {
                        ctx->pid[i].integral = output_local;
                    }
                } else {
                    s_output_applied[i] = output_local;
                }

                OSAL_CRITICAL_SECTION {
                    ctx->status.output[i] = (int32_t)output_local;
                    if (tune_motor) {
                        s_pid_tune.status.applied_pid_target_rpm = target;
                        s_pid_tune.status.measured_rpm = feedback;
                        s_pid_tune.status.error_rpm = ctx->pid[i].last_error;
                        s_pid_tune.status.controller_output_raw =
                            controller_output_raw;
                        s_pid_tune.status.motor_command_applied = output_local;
                        s_pid_tune.status.p_term = tune_terms.p_term;
                        s_pid_tune.status.i_term = tune_terms.i_term;
                        s_pid_tune.status.d_term = tune_terms.d_term;
                        s_pid_tune.status.kp = ctx->pid[i].kp;
                        s_pid_tune.status.ki = ctx->pid[i].ki;
                        s_pid_tune.status.kd = ctx->pid[i].kd;
                    }
                }
            }

            if (nan_detected) {
                /* 算法异常时本周期输出为 0，并清除规划状态，禁止下次沿用坏状态。 */
#if (PRJ_SPEED_SCURVE_ENABLE != 0U)
                app_speed_planner_reset(&s_speed_planner[i], 0.0f);
#endif
                s_target_applied[i] = 0.0f;
                s_output_applied[i] = 0.0f;
                output_local = 0.0f;
                OSAL_CRITICAL_SECTION {
                    ctx->status.output[i] = 0;
                    ctx->status.pid_correction[i] = 0.0f;
                }
                AX_LOG_WARN("PID NaN/Inf detected, reset");
            }

            if (enabled) {
                (void)bsp_motor_set_speed(
                    (bsp_motor_id_t)i, (int32_t)output_local);
            }
        }
        /* 固定500Hz绝对周期延时。 */
#if (PRJ_UART1_BLE_DEBUG_ENABLE != 0U)
        /* 控制任务只复制循迹和控制状态快照，不在本任务中格式化或发送 UART。
         * UART1 低优先级任务负责后续日志输出，避免影响速度环和 IMU 处理。 */
#if (PRJ_LINE_TRACK_ENABLE != 0U)
        app_uart1_ble_debug_update_line(
            control_now_ms,
            line_track_active,
            app_line_track_get_output(),
            ctx);
#else
        app_uart1_ble_debug_update_line(
            control_now_ms,
            false,
            NULL,
            ctx);
#endif
#endif

        osal_task_delay_until_ms(&last_wake_tick, PRJ_CONTROL_PERIOD_MS);
    }
}
