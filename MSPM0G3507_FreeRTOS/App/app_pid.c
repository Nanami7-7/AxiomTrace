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

    pid->out_min = out_min;
    pid->out_max = out_max;
    /* 积分限幅默认等于输出限幅 */
    pid->integral_min = out_min;
    pid->integral_max = out_max;

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
    float error = pid->setpoint - feedback;
    float output = 0.0f;

    if (pid->mode == APP_PID_MODE_POSITION) {
        /* ---- 位置式PID ---- */
        /* 积分项: 梯形积分 */
        if (!pid->is_first_run && dt_s > 0.0f) {
            pid->integral += (error + pid->last_error)
                             * 0.5f * dt_s;
        }
        /* 积分限幅(抗饱和) */
        pid->integral = clamp_f(pid->integral,
            pid->integral_min, pid->integral_max);

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

        output = pid->kp * error
               + pid->ki * pid->integral
               + pid->kd * derivative;

    } else {
        /* ---- 增量式PID ---- */
        if (pid->is_first_run) {
            /* 首次运行: 无历史偏差,输出为比例项 */
            output = pid->kp * error + pid->ki * error;
        } else {
            float delta_error =
                error - pid->last_error;
            float delta2_error = 0.0f;
            if (dt_s > 0.0f) {
                delta2_error =
                    error - 2.0f * pid->last_error
                    + pid->last_last_error;
            }

            /* 增量 */
            float delta_out =
                pid->kp * delta_error
              + pid->ki * error
              + pid->kd * delta2_error;

            /* 累加到上次输出(先限幅再累加,抗饱和) */
            pid->integral = clamp_f(
                pid->integral + delta_out,
                pid->integral_min, pid->integral_max);
            output = clamp_f(
                pid->integral,
                pid->out_min, pid->out_max);
        }
        /* 增量式中integral存储上次输出 */
        pid->integral = output;
        pid->last_last_error = pid->last_error;
    }

    /* 保存偏差 */
    pid->last_error = error;
    pid->is_first_run = false;

    /* 输出限幅 */
    return clamp_f(output, pid->out_min, pid->out_max);
}

void app_pid_reset(app_pid_t *pid)
{
    pid->setpoint        = 0.0f;
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
