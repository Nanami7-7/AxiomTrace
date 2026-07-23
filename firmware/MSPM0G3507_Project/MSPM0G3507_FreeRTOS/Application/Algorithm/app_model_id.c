/**
 * @file    app_model_id.c
 * @brief   电机模型参数辨识纯算法实现
 * @note    仅依赖 <math.h>, 可独立于硬件单元测试
 *
 *          Phase 1: 阶跃响应辨识 (Step Response)
 *            - 63.2% 穿越点法 + 线性插值
 *            - 拟合质量 NRMSE 评估
 *          Phase 2: ARX + PRBS 最小二乘法 (预留)
 */
#include "app_model_id.h"
#include <math.h>
#include <stddef.h>

/* ======================== 全局变量 ======================== */

app_id_data_t g_id_data = {0};

/* ---- 内部状态 ---- */
static volatile app_id_state_t s_state = ID_STATE_IDLE;
static          uint32_t        s_motor_id = 0;
static          int32_t         s_pwm_step = ID_DEFAULT_PWM_STEP;
static          uint32_t        s_stop_cnt = 0;

/* ======================== 内部辅助 ======================== */

static float clamp_f(float val, float min_val, float max_val)
{
    if (val < min_val) return min_val;
    if (val > max_val) return max_val;
    return val;
}

/* ======================== 公共函数实现 ======================== */

void app_id_init(void)
{
    s_state    = ID_STATE_IDLE;
    s_motor_id = 0;
    s_pwm_step = ID_DEFAULT_PWM_STEP;
    s_stop_cnt = 0;
    g_id_data.write_idx = 0;
    g_id_data.done      = false;
}

void app_id_start_step(uint32_t motor_id, int32_t pwm_step)
{
    /* 限幅 PWM */
    if (pwm_step < ID_PWM_MIN) pwm_step = ID_PWM_MIN;
    if (pwm_step >  ID_PWM_MAX) pwm_step =  ID_PWM_MAX;
    if (pwm_step > -ID_MIN_PWM && pwm_step < ID_MIN_PWM) {
        pwm_step = (pwm_step >= 0) ? ID_MIN_PWM : -ID_MIN_PWM;
    }

    s_motor_id     = motor_id;
    s_pwm_step     = pwm_step;
    s_stop_cnt     = 0;
    g_id_data.write_idx = 0;
    g_id_data.done      = false;
    s_state        = ID_STATE_STEP_PRE_STOP;
}

void app_id_abort(void)
{
    s_state = ID_STATE_IDLE;
    g_id_data.done = false;
}

void app_id_control_cycle(const int32_t *rpm, app_id_cycle_out_t *out)
{
    if (rpm == NULL || out == NULL) return;

    out->action   = ID_ACTION_NONE;
    out->pwm      = 0;
    out->motor_id = s_motor_id;

    app_id_state_t state = s_state;
    if (state == ID_STATE_IDLE) return;

    if (s_motor_id >= ID_MOTOR_COUNT) {
        s_state = ID_STATE_IDLE;
        return;
    }

    switch (state) {
    case ID_STATE_STEP_PRE_STOP: {
        out->action = ID_ACTION_BRAKE;
        out->pwm    = 0;

        if (rpm[s_motor_id] > -(int32_t)ID_STOP_RPM_THRESH &&
            rpm[s_motor_id] <  (int32_t)ID_STOP_RPM_THRESH) {
            s_stop_cnt++;
            if (s_stop_cnt >= ID_STOP_CONFIRM_CYCLES) {
                g_id_data.write_idx = 0;
                s_state = ID_STATE_STEP_RECORDING;
                s_stop_cnt = 0;
            }
        } else {
            s_stop_cnt = 0;
        }
        break;
    }

    case ID_STATE_STEP_RECORDING: {
        uint32_t idx = g_id_data.write_idx;
        if (idx < ID_STEP_MAX_SAMPLES) {
            g_id_data.rpm_buf[idx] = (float)rpm[s_motor_id];
            g_id_data.write_idx = idx + 1U;

            out->action = ID_ACTION_APPLY_PWM;
            out->pwm    = s_pwm_step;
        } else {
            s_state = ID_STATE_IDLE;
            g_id_data.done = true;
            out->action = ID_ACTION_BRAKE;
            out->pwm    = 0;
        }
        break;
    }

    default:
        break;
    }
}

bool app_id_process_step(const float *rpm_buf, uint32_t count,
                          int32_t pwm_step, float dt_s,
                          app_id_step_result_t *result)
{
    if (rpm_buf == NULL || result == NULL) return false;
    if (count < 10U || dt_s <= 0.0f) return false;
    if (pwm_step == 0) return false;

    float abs_pwm = (float)(pwm_step > 0 ? pwm_step : -pwm_step);

    /* 1. 稳态转速: 最后25%样本均值 */
    uint32_t steady_start = count - (count / 4U);
    if (steady_start >= count) steady_start = count / 2U;

    float sum_omega = 0.0f;
    for (uint32_t i = steady_start; i < count; i++) {
        sum_omega += rpm_buf[i];
    }
    float n_steady = (float)(count - steady_start);
    float omega_ss = sum_omega / n_steady;

    if (fabsf(omega_ss) < 1.0f) return false;

    /* 2. 直流增益 */
    float K = omega_ss / abs_pwm;
    float omega_sign = (omega_ss >= 0.0f) ? 1.0f : -1.0f;

    /* 3. 63.2% 穿越点搜索 + 线性插值 */
    float target = 0.632f * omega_ss;
    float T = 0.0f;
    bool found = false;

    for (uint32_t i = 1U; i < count; i++) {
        float prev = rpm_buf[i - 1U];
        float curr = rpm_buf[i];
        if ((prev < target && curr >= target) ||
            (prev > target && curr <= target)) {
            float fraction = (target - prev) / (curr - prev);
            T = ((float)(i - 1U) + fraction) * dt_s;
            found = true;
            break;
        }
    }

    if (!found) {
        if (fabsf(rpm_buf[0]) >= fabsf(target)) {
            T = dt_s * 0.5f;
        } else if (fabsf(rpm_buf[0]) >= fabsf(omega_ss) * 0.8f) {
            T = dt_s * 0.3f;
        } else {
            return false;
        }
        found = true;
    }

    /* 4. 拟合质量: NRMSE = 1 - ||y - y_hat|| / ||y - mean(y)|| */
    double sum_sq_err = 0.0;
    double sum_sq_tot = 0.0;
    double data_mean = 0.0;
    for (uint32_t i = 0U; i < count; i++) {
        data_mean += (double)rpm_buf[i];
    }
    data_mean /= (double)count;

    for (uint32_t i = 0U; i < count; i++) {
        double t = (double)i * (double)dt_s;
        double expected = (double)omega_ss * (1.0 - exp(-t / (double)T));
        double err = (double)rpm_buf[i] - expected;
        sum_sq_err += err * err;
        sum_sq_tot  += ((double)rpm_buf[i] - data_mean)
                     * ((double)rpm_buf[i] - data_mean);
    }

    float fit = 0.0f;
    if (sum_sq_tot > 1e-12) {
        fit = 1.0f - (float)sqrt(sum_sq_err / sum_sq_tot);
    }
    if (fit < 0.0f) fit = 0.0f;

    result->K            = K * omega_sign;
    result->T_s          = T;
    result->omega_ss     = omega_ss;
    result->fit_quality  = fit;
    result->sample_count = count;

    return true;
}

bool app_id_pole_placement(const app_id_step_result_t *id,
                            float bandwidth_hz, float damping,
                            float *kp_out, float *ki_out)
{
    (void)damping;

    if (id == NULL || kp_out == NULL || ki_out == NULL) return false;
    if (id->K <= 0.0f || id->T_s <= 0.0f) return false;
    if (bandwidth_hz <= 0.0f) return false;

    float K = id->K;
    float T = id->T_s;

    float tau_cl = 1.0f / (6.2831853f * bandwidth_hz);
    float tau_min = 0.01f;
    float tau_max = T * 0.5f;
    if (tau_max < tau_min) tau_max = tau_min;
    tau_cl = clamp_f(tau_cl, tau_min, tau_max);

    *kp_out = T / (K * tau_cl);
    *ki_out = 1.0f / (K * tau_cl);

    return true;
}
