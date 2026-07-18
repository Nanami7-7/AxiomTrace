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
#include "osal_api.h"
#include "bsp_motor.h"
#include "bsp_encoder.h"
#include "project_config.h"
#include "axiomtrace.h"
#include <math.h>
#include <stdio.h>

/** 位置/角度外环周期倍数(5ms×4=20ms) */
#define POSCTRL_OUTER_RATIO     (4U)

/** 将车轮逻辑顺序(LF/LB/RF/RB)映射为电机顺序(A/M1~D/M4)。 */
static const bsp_encoder_id_t s_motor_encoder_map[BSP_MOTOR_COUNT] =
    PRJ_MOTOR_ENCODER_MAP;

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

    const float dt_s = (float)APP_CONTROL_PERIOD_MS / (float)PRJ_MS_PER_S;
    /* 外环(20ms)时间步长 */
    const float outer_dt_s = (float)(APP_CONTROL_PERIOD_MS * POSCTRL_OUTER_RATIO)
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
        /* M/T法测速: 读取边沿数和时间戳, 换算RPM
         * 高速用M/T法, 低速用T法, 无脉冲时平滑衰减 */
        bool encoder_had_edge[BSP_ENCODER_COUNT];
        int32_t encoder_rpm[BSP_ENCODER_COUNT];
        bool had_edge[BSP_MOTOR_COUNT];
        int32_t rpm_local[BSP_MOTOR_COUNT];
        (void)bsp_encoder_get_all_rpm_mt(encoder_rpm, encoder_had_edge);

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

        OSAL_CRITICAL_SECTION {
            for (uint32_t i = 0; i < BSP_MOTOR_COUNT; i++) {
                ctx->status.rpm[i] = rpm_local[i];
            }
        }

        /* 互补滤波: 融合编码器RPM和IMU数据 */
        {
            /* 编码器线速度: v = rpm * 2π * r / 60 (取4轮平均) */
            float avg_rpm = 0.0f;
            for (uint32_t i = 0; i < BSP_MOTOR_COUNT; i++) {
                avg_rpm += (float)rpm_local[i];
            }
            avg_rpm /= (float)BSP_MOTOR_COUNT;
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
        app_id_cycle_out_t id_out;
        app_id_control_cycle(rpm_local, &id_out);
        bool id_active = (id_out.action != ID_ACTION_NONE);

        /* ---- 位置/角度外环(20ms, 4倍降频) ----
         * SPEED模式: 不修改速度环setpoint(保持现有行为)
         * POSITION/ANGLE模式: 计算target_rpm并写入速度环setpoint
         */
        if (++outer_counter >= POSCTRL_OUTER_RATIO) {
            outer_counter = 0U;

            /* 读取编码器累计脉冲(4路平均, 不清零) */
            int32_t enc_counts[BSP_ENCODER_COUNT];
            (void)bsp_encoder_get_all_counts(enc_counts);
            float enc_avg = 0.0f;
            for (uint32_t i = 0; i < BSP_ENCODER_COUNT; i++) {
                enc_avg += (float)enc_counts[i];
            }
            enc_avg /= (float)BSP_ENCODER_COUNT;

            /* 读取KF yaw(IMU任务写入) */
            float kf_yaw;
            OSAL_CRITICAL_SECTION {
                kf_yaw = ctx->imu.yaw;
            }

            /* 外环更新(临界区保护, 防止与menu/vofa的set_mode冲突) */
            const app_pos_output_t *pos_out;
            uint32_t now_tick = osal_get_tick_count();
            OSAL_CRITICAL_SECTION {
                pos_out = app_posctrl_update(&ctx->posctrl,
                    enc_avg, kf_yaw, outer_dt_s, now_tick);
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
                    uint32_t wheel_id = (uint32_t)s_motor_encoder_map[i];
                    if (wheel_id < APP_POS_MOTOR_COUNT) {
                        app_pid_set_setpoint(&ctx->pid[i],
                            pos_out->target_rpm[wheel_id]);
                    } else {
                        /* 配置异常时安全降为零目标；正常构建不会进入。 */
                        app_pid_set_setpoint(&ctx->pid[i], 0.0f);
                    }
                }
            }
        }

        /* PID计算 + 电机输出(仅使能电机) */
        for (uint32_t i = 0; i < BSP_MOTOR_COUNT; i++) {
            /* 辨识模式下跳过目标电机(PWM由id_out控制) */
            if (id_active && i == id_out.motor_id) {
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
                enabled = ctx->motor_enabled[i];
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

                    if (!isfinite(ff_duty) || !isfinite(pid_corr)) {
                        nan_detected = true;
                        ff_duty = 0.0f;
                        pid_corr = 0.0f;
                        OSAL_CRITICAL_SECTION {
                            app_pid_reset(&ctx->pid[i]);
                        }
                    }

                    ctx->status.pid_correction[i] = pid_corr;
                    output_local = ff_duty + pid_corr;
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
        osal_task_delay_ms(APP_CONTROL_PERIOD_MS);
    }
}