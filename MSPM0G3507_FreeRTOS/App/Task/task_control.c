/**
 * @file    task_control.c
 * @brief   控制任务实现
 * @note    5ms周期: 读取编码器→M法测速RPM→PID计算→设置电机duty
 */
#include "task_control.h"
#include "app_main.h"
#include "app_pid.h"
#include "osal_api.h"
#include "bsp_motor.h"
#include "bsp_encoder.h"
#include <math.h>

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

        /* PID计算 + 电机输出(仅使能电机) */
        for (uint32_t i = 0; i < BSP_MOTOR_COUNT; i++) {
            if (!ctx->motor_enabled[i]) {
                /* 未使能时输出清零 */
                OSAL_CRITICAL_SECTION {
                    ctx->status.output[i] = 0;
                }
                continue;
            }

            float feedback = (float)rpm_local[i];
            float output = app_pid_compute(
                &ctx->pid[i], feedback, dt_s);

            /* NaN/Inf 保护 */
            if (!isfinite(output)) {
                output = 0.0f;
                app_pid_reset(&ctx->pid[i]);
            }

            (void)bsp_motor_set_speed(
                (bsp_motor_id_t)i, (int32_t)output);

            /* 将PID输出写入共享上下文(供VOFA+观察) */
            OSAL_CRITICAL_SECTION {
                ctx->status.output[i] = (int32_t)output;
            }
        }
        /* 周期延时 */
        osal_task_delay_ms(APP_CONTROL_PERIOD_MS);
    }
}