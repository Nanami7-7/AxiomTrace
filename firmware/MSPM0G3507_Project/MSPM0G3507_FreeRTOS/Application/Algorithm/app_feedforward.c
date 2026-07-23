/**
 * @file    app_feedforward.c
 * @brief   前馈控制器实现
 * @note    前馈+PID复合控制: PID增量式从FF基线起步, 逐步修正残差和扰动
 *          启动时: pid->last_output = FF(target), PID增量补偿
 *          简单线性模型: FF = k × RPM + b
 *          分段线性模型: 死区 + 线性区 (Phase 2)
 */
#include "app_feedforward.h"
#include "app_main.h"
#include "app_pid.h"
#include "osal_api.h"
#include <math.h>
#include <stdio.h>

/* ======================== 全局变量 ======================== */

/** Sweep 取消标志 */
volatile bool g_sweep_cancel = false;

/* ======================== 公共函数实现 ======================== */

void app_ff_init(app_ff_params_t *ff)
{
    if (ff == NULL) {
        return;
    }
    ff->k         = 0.0f;
    ff->b         = 0.0f;
    ff->rpm_dead  = 0.0f;
    ff->duty_dead = 0.0f;
    ff->rpm_min   = 0.0f;
    ff->enabled   = false;
}

float app_ff_compute(const app_ff_params_t *ff, float target_rpm)
{
    if (ff == NULL || !ff->enabled) {
        return 0.0f;
    }

    float abs_rpm = fabsf(target_rpm);

    /* Phase 2: 死区处理 (仅当参数 > 0 时生效) */
    if (ff->rpm_dead > 0.0f && abs_rpm < ff->rpm_dead) {
        return 0.0f;
    }
    if (ff->rpm_min > 0.0f && abs_rpm < ff->rpm_min) {
        return copysignf(ff->duty_dead, target_rpm);
    }

    /* 线性前馈: FF = k × RPM + b */
    float duty = ff->k * target_rpm + ff->b;
    return duty;
}

void app_ff_set_params(app_ff_params_t *ff, float k, float b)
{
    if (ff == NULL) {
        return;
    }
    ff->k = k;
    ff->b = b;
}

void app_ff_set_deadzone(app_ff_params_t *ff, float rpm_dead,
                          float duty_dead, float rpm_min)
{
    if (ff == NULL) {
        return;
    }
    ff->rpm_dead  = rpm_dead;
    ff->duty_dead = duty_dead;
    ff->rpm_min   = rpm_min;
}

void app_ff_set_enabled(app_ff_params_t *ff, bool enabled)
{
    if (ff == NULL) {
        return;
    }
    ff->enabled = enabled;
}

void app_ff_apply_to_pid(const app_ff_params_t *ff,
                          app_pid_t *pid, float target)
{
    if (ff == NULL || pid == NULL) {
        return;
    }
    if (ff->enabled) {
        /* 前馈使能: PID从FF值起步 */
        pid->last_output = app_ff_compute(ff, target);
        pid->is_first_run = true;
    } else {
        /* 前馈禁用: PID从0起步(首次运行逻辑) */
        pid->last_output = 0.0f;
        pid->is_first_run = true;
    }
}

bool app_ff_fit_linear(const app_ff_sweep_result_t *result,
                        float *k_out, float *b_out)
{
    if (result == NULL || k_out == NULL || b_out == NULL) {
        return false;
    }

    uint32_t n = result->count;
    if (n < 2U) {
        return false;
    }

    /* 最小二乘: k = (N*Σxy - Σx*Σy) / (N*Σx² - (Σx)²) */
    float sum_x  = 0.0f;
    float sum_y  = 0.0f;
    float sum_xy = 0.0f;
    float sum_xx = 0.0f;

    for (uint32_t i = 0; i < n; i++) {
        float x = result->rpm[i];
        float y = result->duty[i];
        sum_x  += x;
        sum_y  += y;
        sum_xy += x * y;
        sum_xx += x * x;
    }

    float fn    = (float)n;
    float denom = fn * sum_xx - sum_x * sum_x;
    if (fabsf(denom) < 1e-6f) {
        return false;  /* 退化(数据点共线) */
    }

    *k_out = (fn * sum_xy - sum_x * sum_y) / denom;
    *b_out = (sum_y - (*k_out) * sum_x) / fn;

    return true;
}

/* ======================== 扫频功能 ======================== */

/** 扫频RPM序列 */
static const float s_sweep_rpm[FF_SWEEP_POINTS] = {
    50.0f, 100.0f, 150.0f, 200.0f, 300.0f,
    400.0f, 500.0f, 600.0f, 700.0f, 800.0f
};

bool app_ff_sweep(void *ctx, uint32_t motor_id,
                   app_ff_sweep_result_t *result)
{
    app_shared_ctx_t *p_ctx = (app_shared_ctx_t *)ctx;
    if (p_ctx == NULL || result == NULL ||
        motor_id >= BSP_MOTOR_COUNT) {
        return false;
    }

    g_sweep_cancel = false;
    result->count = 0U;
    (void)printf("[SWEEP] Motor %lu start\r\n",
        (unsigned long)motor_id);

    for (uint32_t i = 0U; i < FF_SWEEP_POINTS; i++) {
        /* 检查取消标志 */
        if (g_sweep_cancel) {
            (void)printf("[SWEEP] Cancelled\r\n");
            app_motor_stop(p_ctx, motor_id);
            return false;
        }

        float target = s_sweep_rpm[i];

        /* 设置目标RPM并启动电机 */
        OSAL_CRITICAL_SECTION {
            app_pid_set_setpoint(&p_ctx->pid[motor_id], target);
            p_ctx->motor_enabled[motor_id] = true;
        }

        /* 等待稳态(分段检查取消标志) */
        for (uint32_t k = 0U; k < 60U; k++) {  // 60 * 50ms = 3000ms
            if (g_sweep_cancel) {
                (void)printf("[SWEEP] Cancelled\r\n");
                app_motor_stop(p_ctx, motor_id);
                return false;
            }
            osal_task_delay_ms(50U);
        }

        /* 采集稳态数据(取最后1秒平均) */
        float sum_duty = 0.0f;
        float sum_rpm  = 0.0f;
        for (uint32_t j = 0U; j < FF_SWEEP_SAMPLE_COUNT; j++) {
            OSAL_CRITICAL_SECTION {
                sum_duty += (float)p_ctx->status.output[motor_id];
                sum_rpm  += (float)p_ctx->status.rpm[motor_id];
            }
            osal_task_delay_ms(FF_SWEEP_CTRL_PERIOD_MS);
        }

        float fn = (float)FF_SWEEP_SAMPLE_COUNT;
        result->rpm[i]  = sum_rpm  / fn;
        result->duty[i] = sum_duty / fn;
        result->count++;

        (void)printf("[SWEEP] RPM=%.0f -> DUTY=%.0f\r\n",
            (double)result->rpm[i], (double)result->duty[i]);
    }

    /* 停止电机 */
    app_motor_stop(p_ctx, motor_id);
    (void)printf("[SWEEP] Done\r\n");

    return true;
}
