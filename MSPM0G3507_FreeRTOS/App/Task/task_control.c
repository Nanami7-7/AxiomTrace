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
#include "osal_api.h"
#include "bsp_motor.h"
#include "bsp_encoder.h"
#include "project_config.h"
#include "axiomtrace.h"
#include <math.h>
#include <stdio.h>

void app_control_task(void *param)
{
    app_shared_ctx_t *ctx = (app_shared_ctx_t *)param;

    const float dt_s = (float)APP_CONTROL_PERIOD_MS / 1000.0f;

    /* 堵转检测: >1s无脉冲强制归零 */
    static uint32_t s_last_edge_tick[BSP_ENCODER_COUNT] = {0};

    for (;;) {
        /* M/T法测速: 读取边沿数和时间戳, 换算RPM
         * 高速用M/T法, 低速用T法, 无脉冲时平滑衰减 */
        bool had_edge[BSP_ENCODER_COUNT];
        int32_t rpm_local[BSP_ENCODER_COUNT];
        (void)bsp_encoder_get_all_rpm_mt(rpm_local, had_edge);

        /* 堵转检测: >1s无脉冲强制归零 */
        {
            uint32_t now = xTaskGetTickCount();
            for (uint32_t i = 0; i < BSP_ENCODER_COUNT; i++) {
                if (had_edge[i]) {
                    s_last_edge_tick[i] = now;
                }
                if ((now - s_last_edge_tick[i])
                    > pdMS_TO_TICKS(1000)) {
                    rpm_local[i] = 0;
                }
            }
        }

        OSAL_CRITICAL_SECTION {
            for (uint32_t i = 0; i < BSP_ENCODER_COUNT; i++) {
                ctx->status.rpm[i] = rpm_local[i];
            }
        }

        /* 互补滤波: 融合编码器RPM和IMU数据 */
        {
            /* 编码器线速度: v = rpm * 2π * r / 60 (取4轮平均) */
            float avg_rpm = 0.0f;
            for (uint32_t i = 0; i < BSP_ENCODER_COUNT; i++) {
                avg_rpm += (float)rpm_local[i];
            }
            avg_rpm /= (float)BSP_ENCODER_COUNT;
            float encoder_vx = avg_rpm * 2.0f * 3.14159265f
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