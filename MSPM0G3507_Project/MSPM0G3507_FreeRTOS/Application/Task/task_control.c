/**
 * @file    task_control.c
 * @brief   控制任务实现
 * @note    5ms周期: 读取编码器→M法测速RPM→PID计算→设置电机duty
 */
#include "task_control.h"
#include "app_main.h"
#include "app_pid.h"
#include "app_feedforward.h"
#include "app_complementary_filter.h"
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
#include "axiomtrace.h"
#include <math.h>
#include <stdio.h>

#ifndef PRJ_LINE_TRACK_ENABLE
#define PRJ_LINE_TRACK_ENABLE (1U)
#endif
#if PRJ_LINE_TRACK_ENABLE
#include "app_line_track.h"
#endif

/** 位置/角度外环周期倍数(5ms×4=20ms) */
#define POSCTRL_OUTER_RATIO     (4U)

/** 将车轮逻辑顺序(LF/LB/RF/RB)映射为电机顺序(A/M1~D/M4)。 */
static const bsp_encoder_id_t s_motor_encoder_map[BSP_MOTOR_COUNT] =
    PRJ_MOTOR_ENCODER_MAP;

#if PRJ_LINE_TRACK_ENABLE
/* 保存最近一次循迹结果；实际输出由 app_line_track_step() 复用电机 BSP 完成。 */
static line_track_output_t s_line_track_output;

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
    s_line_lap.finish_trigger_distance_m = 0.0f;
    (void)printf("[LINE] 单圈循迹启动，等待离开 A 点横线\r\n");
}

static void line_lap_reset(void)
{
    s_line_lap.state = LINE_LAP_IDLE;
    s_line_lap.start_ms = 0U;
    s_line_lap.state_since_ms = 0U;
    s_line_lap.lost_since_ms = 0U;
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

    if (elapsed_ms >= LINE_TRACK_MAX_RUN_MS) {
        line_lap_stop(ctx, "运行超时", distance_m, now_ms);
        return true;
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

    return false;
}
#endif

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
    /* 外环(20ms)时间步长 */
    const float outer_dt_s = (float)(PRJ_CONTROL_PERIOD_MS * POSCTRL_OUTER_RATIO)
                              / (float)PRJ_MS_PER_S;

    /* 堵转检测: >1s无脉冲强制归零 */
    static uint32_t s_last_edge_tick[BSP_MOTOR_COUNT] = {0};

    /* 外环周期计数器(5ms×4=20ms) */
    uint32_t outer_counter = 0U;

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

    for (;;) {
        uint32_t control_now_ms = osal_ticks_to_ms(osal_get_tick_count());
        bool line_track_active = false;
        bool line_track_fault_stopped = false;
#if (PRJ_KEY_ENABLE != 0U)
        /* Key callbacks only enqueue actions; execute motion in control context. */
        app_key_motion_process(control_now_ms);
#endif
        /* Position-loop telemetry is independent of RPM calculation and PWM control. */
        app_encoder_telemetry_process(control_now_ms);

#if PRJ_LINE_TRACK_ENABLE
        /*
         * 模块4是独立的循迹控制模式：
         * - 协议任务只修改请求标志，不直接操作电机；
         * - 控制任务在5ms周期内完成启停切换和循迹输出；
         * - 运行时跳过辨识/PID的最终输出，避免两个控制器互相覆盖。
         */
        const bool line_track_requested =
            app_protocol_user_line_track_is_enabled();

        if (app_protocol_user_line_track_take_reset_request()) {
            app_line_track_reset();
        }

        if (line_track_requested && !app_line_track_is_running()) {
            /* 切换控制器前先清除旧PID目标和正在进行的模型辨识。 */
            app_id_abort();
            app_motor_stop_all(ctx);
            app_line_track_start();
            line_lap_start(control_now_ms);
        } else if (!line_track_requested && app_line_track_is_running()) {
            /* 停止循迹后不自动恢复旧目标，等待后续标准使能/运行流程。 */
            app_id_abort();
            app_line_track_stop();
            app_motor_stop_all(ctx);
            line_lap_reset();
        }

        line_track_active = app_line_track_is_running();
        if (line_track_active && app_line_track_step()) {
            const line_track_output_t *line_out =
                app_line_track_get_output();

            /* 状态快照同步显示循迹实际命令；PID修正量在循迹模式下清零。 */
            if (line_out != NULL) {
                OSAL_CRITICAL_SECTION {
                    for (uint32_t i = 0U; i < BSP_MOTOR_COUNT; i++) {
                        const int32_t command = (i < 2U) ?
                            line_out->right_command : line_out->left_command;
                        ctx->status.output[i] =
                            ((PRJ_MOTION_MOTOR_ACTIVE_MASK & (1UL << i)) != 0UL) ?
                            command : 0;
                        ctx->status.pid_correction[i] = 0.0f;
                    }
                }
            }

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

        /* Start the five-channel ADC sequence without blocking the 5 ms loop. */
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
                    PRJ_SPEED_RPM_FILTER_ALPHA_LOW :
                    PRJ_SPEED_RPM_FILTER_ALPHA_HIGH;

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

        /* ---- 模型参数辨识: 控制任务周期调用(纯算法) ---- */
        app_id_cycle_out_t id_out = { ID_ACTION_NONE, 0, 0U };
        if (!line_track_active && !line_track_fault_stopped) {
            app_id_control_cycle(rpm_local, &id_out);
        }
        bool id_active = !line_track_active && !line_track_fault_stopped &&
                         (id_out.action != ID_ACTION_NONE);

        /* ---- 位置/角度外环(20ms, 4倍降频) ----
         * SPEED模式: 不修改速度环setpoint(保持现有行为)
         * POSITION/ANGLE模式: 计算target_rpm并写入速度环setpoint
         */
        if (!line_track_active && !line_track_fault_stopped && (++outer_counter >= POSCTRL_OUTER_RATIO)) {
            outer_counter = 0U;

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

        /* PID计算 + 电机输出(仅使能电机) */
        for (uint32_t i = 0; i < BSP_MOTOR_COUNT; i++) {
            /* 循迹模式已有独立输出，禁止PID/辨识再次覆盖电机命令。 */
            if (line_track_active || line_track_fault_stopped) {
                continue;
            }
            /* 辨识模式下跳过目标电机(PWM由id_out控制) */
            if (id_active && i == id_out.motor_id &&
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
            float feedback = (float)rpm_local[i];
            float output_local;
            bool enabled;
            bool nan_detected = false;

            OSAL_CRITICAL_SECTION {
                enabled = ctx->motor_enabled[i] &&
                    ((PRJ_MOTION_MOTOR_ACTIVE_MASK & (1UL << i)) != 0UL);
            }

            if (!enabled) {
                OSAL_CRITICAL_SECTION {
                    ctx->status.output[i] = 0;
                    ctx->status.pid_correction[i] = 0.0f;
                }
                output_local = 0.0f;
            } else {
                /* 同步FF模式标志 */
                ctx->pid[i].use_ff = ctx->ff[i].enabled;

                if (ctx->ff[i].enabled) {
                    /* FF模式: FF duty + 位置式PID修正量 */
                    float ff_duty = app_ff_compute(
                        &ctx->ff[i], ctx->pid[i].setpoint);
                    float pid_corr = app_pid_compute(
                        &ctx->pid[i], feedback, dt_s);
                    float current_corr = app_ff_compute_current_correction(
                        &ctx->ff[i], ctx->pid[i].setpoint,
                        current_ma_local[i]);

                    if (!isfinite(ff_duty) || !isfinite(pid_corr) ||
                        !isfinite(current_corr)) {
                        nan_detected = true;
                        ff_duty = 0.0f;
                        pid_corr = 0.0f;
                        current_corr = 0.0f;
                        OSAL_CRITICAL_SECTION {
                            app_pid_reset(&ctx->pid[i]);
                        }
                    }

                    ctx->status.pid_correction[i] = pid_corr;
                    output_local = ff_duty + pid_corr + current_corr;
                } else {
                    /* 普通模式: 增量式PID */
                    output_local = app_pid_compute(
                        &ctx->pid[i], feedback, dt_s);

                    if (!isfinite(output_local)) {
                        nan_detected = true;
                        output_local = 0.0f;
                        OSAL_CRITICAL_SECTION {
                            app_pid_reset(&ctx->pid[i]);
                        }
                    }
                    ctx->status.pid_correction[i] = 0.0f;
                }

                OSAL_CRITICAL_SECTION {
                    ctx->status.output[i] = (int32_t)output_local;
                }
            }

            if (nan_detected) {
                AX_LOG_WARN("PID NaN/Inf detected, reset");
            }

            if (enabled) {
                (void)bsp_motor_set_speed(
                    (bsp_motor_id_t)i, (int32_t)output_local);
            }
        }
        /* 周期延时 */
        osal_task_delay_ms(PRJ_CONTROL_PERIOD_MS);
    }
}
