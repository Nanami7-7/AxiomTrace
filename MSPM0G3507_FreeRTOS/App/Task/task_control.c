/**
 * @file    task_control.c
 * @brief   控制任务实现
 * @note    5ms周期: 读取编码器→M法测速RPM→PID计算→设置电机duty
 */
#include "task_control.h"
#include "app_main.h"
#include "app_pid.h"
#include "app_complementary_filter.h"
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

    for (;;) {
        /*
         * M法测速: 读取并清零脉冲计数, 换算RPM
         * 在APP_CONTROL_PERIOD_MS窗口内计数编码器脉冲
         */
        int32_t deltas[BSP_ENCODER_COUNT];
        (void)bsp_encoder_get_and_clear_all(deltas);

        int32_t rpm_local[BSP_ENCODER_COUNT];
        for (uint32_t i = 0; i < BSP_ENCODER_COUNT; i++) {
            rpm_local[i] = bsp_encoder_counts_to_rpm(
                deltas[i], APP_CONTROL_PERIOD_MS);
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
            float imu_accel_x, imu_gyro_z;
            OSAL_CRITICAL_SECTION {
                imu_accel_x = ctx->imu.accel_x_g;
                imu_gyro_z  = ctx->imu.gyro_z_dps;
            }

            /* 更新互补滤波器 */
            app_cf_update(encoder_vx, imu_accel_x, imu_gyro_z, dt_s);
        }

        /* PID计算 + 电机输出(仅使能电机) */
        for (uint32_t i = 0; i < BSP_MOTOR_COUNT; i++) {
            float feedback = (float)rpm_local[i];
            float output_local;
            bool enabled;
            bool nan_detected = false;

            /*
             * 临界区: 读取使能标志 + PID计算 + 写入输出
             * 将motor_enabled判断、PID计算、输出写入合并到同一临界区，
             * 消除与menu任务的竞态条件(B2/B3)。
             * snprintf/AX_LOG_WARN移到临界区外(B1)。
             */
            OSAL_CRITICAL_SECTION {
                enabled = ctx->motor_enabled[i];
                if (!enabled) {
                    ctx->status.output[i] = 0;
                    output_local = 0.0f;
                } else {
                    output_local = app_pid_compute(
                        &ctx->pid[i], feedback, dt_s);

                    if (!isfinite(output_local)) {
                        nan_detected = true;
                        output_local = 0.0f;
                        app_pid_reset(&ctx->pid[i]);
                    }
                    ctx->status.output[i] = (int32_t)output_local;
                }
            }

            /* 临界区外: NaN警告日志(不阻塞中断) */
            if (nan_detected) {
                AX_LOG_WARN("PID NaN/Inf detected, reset");
            }

            /* 临界区外: 硬件操作 */
            if (enabled) {
                (void)bsp_motor_set_speed(
                    (bsp_motor_id_t)i, (int32_t)output_local);
            }
        }
        /* 周期延时 */
        osal_task_delay_ms(APP_CONTROL_PERIOD_MS);
    }
}