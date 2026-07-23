/**
 * @file    app_pid.c
 * @brief   PID控制器实现(纯算法,无硬件依赖)
 * @note    从旧bsp_pid.c重构,修正以下问题:
 *          1. 去除全局变量pid_lf/lb/rf/rb,改为多实例
 *          2. 去除extern GetUs()硬件依赖,由调用者传入dt
 *          3. 修复旧代码中error_integral/error_derivative
 *             局部变量遮蔽外层变量的bug(第77~79行)
 *          4. 统一位置式/增量式PID于同一接口
 *          5. 增加抗积分饱和和微分滤波
 */
#include "app_pid.h"
#include <math.h>

/* ======================== 私有函数 ======================== */

/**
 * @brief  限幅函数
 * @param  val 输入值
 * @param  min_val 下限
 * @param  max_val 上限
 * @retval 限幅后的值
 */
static inline float clamp_f(float val, float min_val,
                              float max_val)
{
    if (val < min_val) { return min_val; }
    if (val > max_val) { return max_val; }
    return val;
}

/* ======================== 公共函数实现 ======================== */

void app_pid_init(app_pid_t *pid, float kp, float ki,
                   float kd, app_pid_mode_t mode,
                   float out_min, float out_max)
{
    if (!pid) { return; }
    pid->kp   = kp;
    pid->ki   = ki;
    pid->kd   = kd;
    pid->mode = mode;

    /* FF模式默认参数(禁用,参数与普通模式相同) */
    pid->ff_kp = kp;
    pid->ff_ki = ki;
    pid->ff_kd = 0.0f;
    pid->use_ff = false;

    pid->out_min = out_min;
    pid->out_max = out_max;
    /* 积分限幅默认等于输出限幅 */
    pid->integral_min = out_min;
    pid->integral_max = out_max;
    /* FF模式积分限幅默认与普通模式相同 */
    pid->ff_integral_min = out_min;
    pid->ff_integral_max = out_max;

    pid->d_filter_coeff = 0.0f;

    app_pid_reset(pid);
}

void app_pid_set_params(app_pid_t *pid, float kp, float ki,
                         float kd)
{
    pid->kp = kp;
    pid->ki = ki;
    pid->kd = kd;
}

void app_pid_set_integral_limit(app_pid_t *pid,
                                 float integral_min,
                                 float integral_max)
{
    pid->integral_min = integral_min;
    pid->integral_max = integral_max;
}

void app_pid_set_d_filter(app_pid_t *pid, float coeff)
{
    pid->d_filter_coeff = clamp_f(coeff, 0.0f, 1.0f);
}

void app_pid_set_setpoint(app_pid_t *pid, float setpoint)
{
    pid->setpoint = setpoint;
}

float app_pid_compute(app_pid_t *pid, float feedback,
                       float dt_s)
{
    if (!pid) { return 0.0f; }
    if (!isfinite(feedback) || !isfinite(dt_s) || dt_s <= 0.0f) {
        return isfinite(pid->last_output) ? pid->last_output : 0.0f;
    }

    float error = pid->setpoint - feedback;
    float output = 0.0f;

    /* 根据模式选择参数和积分限幅 */
    float kp, ki, kd, int_min, int_max;
    if (pid->use_ff) {
        kp = pid->ff_kp;
        ki = pid->ff_ki;
        kd = pid->ff_kd;
        int_min = pid->ff_integral_min;
        int_max = pid->ff_integral_max;
    } else {
        kp = pid->kp;
        ki = pid->ki;
        kd = pid->kd;
        int_min = pid->integral_min;
        int_max = pid->integral_max;
    }

    if (pid->use_ff) {
        /* ---- FF模式: 位置式PID(输出修正量) ---- */
        /* 积分项: 梯形积分 */
        if (!pid->is_first_run && dt_s > 0.0f) {
            pid->integral += (error + pid->last_error)
                             * 0.5f * dt_s;
        }
        pid->integral = clamp_f(pid->integral,
            int_min, int_max);

        /* 微分项 */
        float derivative = 0.0f;
        if (!pid->is_first_run && dt_s > 0.0f) {
            derivative = (error - pid->last_error) / dt_s;
            if (pid->d_filter_coeff > 0.0f) {
                derivative = pid->last_derivative
                    + (1.0f - pid->d_filter_coeff)
                    * (derivative - pid->last_derivative);
            }
        }
        pid->last_derivative = derivative;

        output = kp * error + ki * pid->integral
               + kd * derivative;

    } else if (pid->mode == APP_PID_MODE_POSITION) {
        /* ---- 位置式PID ---- */
        /* 积分项: 梯形积分 */
        if (!pid->is_first_run && dt_s > 0.0f) {
            pid->integral += (error + pid->last_error)
                             * 0.5f * dt_s;
        }
        /* 积分限幅(抗饱和) */
        pid->integral = clamp_f(pid->integral,
            int_min, int_max);

        /* 微分项 */
        float derivative = 0.0f;
        if (!pid->is_first_run && dt_s > 0.0f) {
            derivative = (error - pid->last_error)
                         / dt_s;
            /* 一阶低通滤波 */
            if (pid->d_filter_coeff > 0.0f) {
                derivative = pid->last_derivative
                    + (1.0f - pid->d_filter_coeff)
                    * (derivative - pid->last_derivative);
            }
        }
        pid->last_derivative = derivative;

        output = kp * error
               + ki * pid->integral
               + kd * derivative;

    } else {
        /*
         * ---- 增量式PID ----
         * 增量式PID公式: Δu = Kp*Δe + Ki*e + Kd*Δ²e
         *   Δe  = e(k) - e(k-1)          (偏差增量)
         *   Δ²e = e(k) - 2*e(k-1) + e(k-2) (偏差二阶差分)
         *   u(k) = u(k-1) + Δu            (输出累加)
         *
         * 本实现中, pid->integral 用于存储"上次输出 u(k-1)",
         * 而非传统位置式PID中的"积分累积和".
         * 因此 integral_min/max 的限幅实际约束的是输出幅度,
         * 这也是为什么在 app_main.c 中将 integral 限幅设为
         * 与输出限幅一致的原因.
         */
        if (pid->is_first_run) {
            /* 首帧无差分历史，仅施加比例项；积分从下一帧按 dt 累加。 */
            output = pid->last_output + kp * error;
        } else {
            /* 计算偏差增量和二阶差分 */
            float delta_error =
                error - pid->last_error;
            float delta2_error = error - 2.0f * pid->last_error
                               + pid->last_last_error;

            /* 计算输出增量 Δu */
            float delta_out =
                kp * delta_error
              + ki * error * dt_s
              + kd * delta2_error / dt_s;

            /* 累加到上次输出, 先限幅再累加(抗饱和) */
            pid->last_output = clamp_f(
                pid->last_output + delta_out,
                int_min, int_max);
            output = clamp_f(
                pid->last_output,
                pid->out_min, pid->out_max);
        }
        pid->last_last_error = pid->last_error;
    }

    /* 保存偏差 */
    pid->last_error = error;
    pid->is_first_run = false;

    /* 输出限幅 */
    output = clamp_f(output, pid->out_min, pid->out_max);
    pid->last_output = output;

    /*
     * NaN/Inf 保护:
     * 如果输出或内部状态出现 NaN/Inf, 说明输入异常(如编码器故障),
     * 重置PID状态并返回0, 防止异常值传播到电机驱动.
     */
    if (!isfinite(output) || !isfinite(pid->integral) ||
        !isfinite(pid->last_output) ||
        !isfinite(pid->last_error)) {
        app_pid_reset(pid);
        output = 0.0f;
    }

    return output;
}

void app_pid_reset(app_pid_t *pid)
{
    pid->last_error      = 0.0f;
    pid->last_last_error = 0.0f;
    pid->integral        = 0.0f;
    pid->last_output     = 0.0f;
    pid->last_derivative = 0.0f;
    pid->is_first_run    = true;
}

float app_pid_get_error(const app_pid_t *pid)
{
    return pid->last_error;
}

float app_pid_get_integral(const app_pid_t *pid)
{
    return pid->integral;
}
