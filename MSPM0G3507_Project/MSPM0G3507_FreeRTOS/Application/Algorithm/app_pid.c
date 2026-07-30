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
#include <stddef.h>

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

static float app_pid_compute_internal(app_pid_t *pid,
                                      float setpoint,
                                      float feedback,
                                      float dt_s,
                                      app_pid_terms_t *terms)
{
    float error = setpoint - feedback;
    float output = 0.0f;
    float output_raw = 0.0f;
    float p_term = 0.0f;
    float i_term = 0.0f;
    float d_term = 0.0f;

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

        p_term = kp * error;
        i_term = ki * pid->integral;
        d_term = kd * derivative;
        output_raw = p_term + i_term + d_term;
        output = output_raw;

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

        p_term = kp * error;
        i_term = ki * pid->integral;
        d_term = kd * derivative;
        output_raw = p_term + i_term + d_term;
        output = output_raw;

    } else {
        /*
         * ---- 增量式PID ----
         * 沿用本工程既有的“每采样周期增益”定义，避免调参功能改变普通控制行为：
         *   Δu = Kp*Δe + Ki*e + Kd*Δ²e
         *   Δe  = e(k) - e(k-1)
         *   Δ²e = e(k) - 2*e(k-1) + e(k-2)
         *   u(k) = u(k-1) + Δu
         *
         * pid->integral 在增量式模式中保存上次输出 u(k-1)，
         * 不是位置式PID的积分累计量。
         */
        if (pid->is_first_run) {
            /* 首次运行没有历史偏差，保持原有P+I启动算法。 */
            p_term = kp * error;
            i_term = ki * error;
            output_raw = p_term + i_term;
            output = output_raw;
        } else {
            const float delta_error = error - pid->last_error;
            const float delta2_error = error - 2.0f * pid->last_error
                                     + pid->last_last_error;

            p_term = kp * delta_error;
            i_term = ki * error;
            d_term = kd * delta2_error;
            const float delta_out = p_term + i_term + d_term;

            /* raw记录内部限幅前的候选输出，控制路径仍保持原有限幅顺序。 */
            output_raw = pid->integral + delta_out;
            pid->integral = clamp_f(output_raw, int_min, int_max);
            output = clamp_f(pid->integral, pid->out_min, pid->out_max);
        }
        /* 保持原有状态更新顺序，不让调试接口改变普通模式控制结果。 */
        pid->integral = output;
        pid->last_last_error = pid->last_error;
    }

    /* 保存偏差 */
    pid->last_error = error;
    pid->is_first_run = false;

    /* 输出限幅 */
    output = clamp_f(output, pid->out_min, pid->out_max);

    /*
     * NaN/Inf 保护:
     * 如果输出或内部状态出现 NaN/Inf, 说明输入异常(如编码器故障),
     * 重置PID状态并返回0, 防止异常值传播到电机驱动.
     */
    if (!isfinite(output) || !isfinite(pid->integral) ||
        !isfinite(pid->last_error) || !isfinite(p_term) ||
        !isfinite(i_term) || !isfinite(d_term)) {
        app_pid_reset(pid);
        output = 0.0f;
        p_term = 0.0f;
        i_term = 0.0f;
        d_term = 0.0f;
        output_raw = 0.0f;
    }

    if (terms != NULL) {
        terms->p_term = p_term;
        terms->i_term = i_term;
        terms->d_term = d_term;
        terms->output_raw = output_raw;
    }

    return output;
}

float app_pid_compute(app_pid_t *pid, float feedback,
                      float dt_s)
{
    return app_pid_compute_internal(pid, pid->setpoint, feedback, dt_s, NULL);
}

float app_pid_compute_target(app_pid_t *pid, float target,
                             float feedback, float dt_s)
{
    /*
     * target只参与本次计算，不改写pid->setpoint。
     * 这样控制任务可以使用斜坡目标和IMU差速修正，同时菜单/协议仍保存用户原始目标。
     */
    return app_pid_compute_internal(pid, target, feedback, dt_s, NULL);
}

float app_pid_compute_target_diag(app_pid_t *pid, float target,
                                  float feedback, float dt_s,
                                  app_pid_terms_t *terms)
{
    return app_pid_compute_internal(pid, target, feedback, dt_s, terms);
}

void app_pid_reset(app_pid_t *pid)
{
    pid->last_error      = 0.0f;
    pid->last_last_error = 0.0f;
    pid->integral        = 0.0f;
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
