/**
 * @file    filter_eskf.c
 * @brief   误差状态卡尔曼滤波器 (ESKF) 实现 — MCU 优化版
 *
 * ESKF 优于 EKF 的地方（MSPM0/Cortex-M0+ 场景）:
 *   - 6x6 协方差 (vs EKF 7x7) → 36% RAM 减少
 *   - 雅可比简化: I ± dt*skew (vs EKF 复杂 F_qb 推导)
 *   - 名义状态/误差状态分离: 四元数积分独立于 KF
 *   - 静态工厂路径零堆分配；动态工厂保留兼容性
 *   - 稀疏状态转移与 Joseph 协方差更新
 *
 * 状态向量:
 *   名义: 四元数 [q0, q1, q2, q3]
 *   误差: [dtheta_x, dtheta_y, dtheta_z, dbias_x, dbias_y, dbias_z]
 *
 * 模型:
 *   过程: dθ' = -[ω]×·dθ - db - n_g
 *         db' = n_b
 *   观测 (加速度计): z = R(q)^T·g + n_a
 *   H = [skew(h(q)), 0_3x3]
 *
 * Yaw 解耦: K[error_yaw][:] = 0, 禁止加速度计修正 yaw
 * ZUPT: 静止时 EMA 跟踪陀螺偏置
 */

#include "filter_internal.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

/* ---- ESKF 本地常量 ---- */
#define ESKF_COV_MIN        1e-10f
#define ESKF_COV_MAX        1e6f
#define ESKF_DET_EPS        1e-12f
#define ESKF_ACC_GATE_LOW   0.7f    /* g */
#define ESKF_ACC_GATE_HIGH  1.3f    /* g */
#define ESKF_ZUPT_GYRO_TH   2.0f    /* dps, static判定 */
#define ESKF_ZUPT_ACC_TH    0.05f   /* g, 静态加速度偏差 */
#define ESKF_ZUPT_RATE      0.04f   /* EMA系数 (20帧收敛) */
#define ESKF_REG_INTERVAL   100     /* 协方差正则化间隔(帧) */
#define ESKF_DEG2RAD        (M_PI / 180.0f)

/* ---- 内部辅助 ---- */

static inline void eskf_quat_norm(float *q0, float *q1, float *q2, float *q3)
{
    float n = sqrtf(*q0 * *q0 + *q1 * *q1 + *q2 * *q2 + *q3 * *q3);
    if (n < 1e-10f) { *q0 = 1.0f; *q1 = *q2 = *q3 = 0.0f; return; }
    n = 1.0f / n;
    *q0 *= n; *q1 *= n; *q2 *= n; *q3 *= n;
}

static inline float eskf_skew_3x3_rank_preserving_inv_det(
    float a, float b, float c,
    float d, float e, float f,
    float g, float h, float k)
{
    /* 解析 3x3 行列式: det = a(e*k - f*h) - b(d*k - f*g) + c(d*h - e*g) */
    return a * (e * k - f * h) - b * (d * k - f * g) + c * (d * h - e * g);
}

static void eskf_inv_3x3(float S[3][3], float S_inv[3][3])
{
    float det = eskf_skew_3x3_rank_preserving_inv_det(
        S[0][0], S[0][1], S[0][2],
        S[1][0], S[1][1], S[1][2],
        S[2][0], S[2][1], S[2][2]);
    float inv_det = 1.0f / det;
    S_inv[0][0] = (S[1][1]*S[2][2] - S[1][2]*S[2][1]) * inv_det;
    S_inv[0][1] = (S[0][2]*S[2][1] - S[0][1]*S[2][2]) * inv_det;
    S_inv[0][2] = (S[0][1]*S[1][2] - S[0][2]*S[1][1]) * inv_det;
    S_inv[1][0] = (S[1][2]*S[2][0] - S[1][0]*S[2][2]) * inv_det;
    S_inv[1][1] = (S[0][0]*S[2][2] - S[0][2]*S[2][0]) * inv_det;
    S_inv[1][2] = (S[0][2]*S[1][0] - S[0][0]*S[1][2]) * inv_det;
    S_inv[2][0] = (S[1][0]*S[2][1] - S[1][1]*S[2][0]) * inv_det;
    S_inv[2][1] = (S[0][1]*S[2][0] - S[0][0]*S[2][1]) * inv_det;
    S_inv[2][2] = (S[0][0]*S[1][1] - S[0][1]*S[1][0]) * inv_det;
}

static inline float eskf_max_abs_3(float a, float b, float c)
{
    float m = fabsf(a);
    if (fabsf(b) > m) m = fabsf(b);
    if (fabsf(c) > m) m = fabsf(c);
    return m;
}

/* ---- 主更新函数 ---- */

void eskf_update(filter_t *self, const filter_input_t *in, filter_output_t *out)
{
    eskf_priv_t *p = (eskf_priv_t *)self->priv;
    float dt = in->dt;

    /* === 1. 输入验证 === */
    if (dt <= 0.0f || dt > 0.5f ||
        isnan(in->ax) || isinf(in->ax) ||
        isnan(in->ay) || isinf(in->ay) ||
        isnan(in->az) || isinf(in->az) ||
        isnan(in->gx) || isinf(in->gx) ||
        isnan(in->gy) || isinf(in->gy) ||
        isnan(in->gz) || isinf(in->gz)) {
        filter_quat_to_euler(p->q0, p->q1, p->q2, p->q3, out);
        return;
    }

    if (self->degrade == FILTER_DEGRADE_HOLD_LAST) {
        filter_quat_to_euler(p->q0, p->q1, p->q2, p->q3, out);
        return;
    }

    if (self->degrade == FILTER_DEGRADE_ACC_ONLY) {
        filter_acc_to_quat(in->ax, in->ay, in->az,
                           &p->q0, &p->q1, &p->q2, &p->q3);
        filter_quat_to_euler(p->q0, p->q1, p->q2, p->q3, out);
        return;
    }

    /* === 2. 名义状态预测: q_new = q ⊗ q{ω*dt} === */
    float gx = (in->gx - p->bias_x) * M_PI / 180.0f;
    float gy = (in->gy - p->bias_y) * M_PI / 180.0f;
    float gz = (in->gz - p->bias_z) * M_PI / 180.0f;

    float q0v = p->q0, q1v = p->q1, q2v = p->q2, q3v = p->q3;
    p->q0 += 0.5f * (-q1v * gx - q2v * gy - q3v * gz) * dt;
    p->q1 += 0.5f * ( q0v * gx + q2v * gz - q3v * gy) * dt;
    p->q2 += 0.5f * ( q0v * gy - q1v * gz + q3v * gx) * dt;
    p->q3 += 0.5f * ( q0v * gz + q1v * gy - q2v * gx) * dt;
    eskf_quat_norm(&p->q0, &p->q1, &p->q2, &p->q3);

    /* === 3. 误差协方差预测: P = F*P*F^T + Q ===
     * 误差偏置状态使用 dps，因此 F(theta,bias) 必须包含 deg->rad。
     * 直接利用 F 的稀疏结构，避免在任务栈上构造额外 6x6 F 矩阵。 */
    const float bias_dt = dt * ESKF_DEG2RAD;
    float FP[6][6];
    for (int j = 0; j < 6; j++) {
        FP[0][j] = p->P[0][j] + dt*gz*p->P[1][j]
                  - dt*gy*p->P[2][j] - bias_dt*p->P[3][j];
        FP[1][j] = p->P[1][j] - dt*gz*p->P[0][j]
                  + dt*gx*p->P[2][j] - bias_dt*p->P[4][j];
        FP[2][j] = p->P[2][j] + dt*gy*p->P[0][j]
                  - dt*gx*p->P[1][j] - bias_dt*p->P[5][j];
        FP[3][j] = p->P[3][j];
        FP[4][j] = p->P[4][j];
        FP[5][j] = p->P[5][j];
    }

    /* 右乘 F^T；仍按 F 的行稀疏结构展开。 */
    float P_pred[6][6];
    for (int i = 0; i < 6; i++) {
        P_pred[i][0] = FP[i][0] + dt*gz*FP[i][1]
                     - dt*gy*FP[i][2] - bias_dt*FP[i][3];
        P_pred[i][1] = FP[i][1] - dt*gz*FP[i][0]
                     + dt*gx*FP[i][2] - bias_dt*FP[i][4];
        P_pred[i][2] = FP[i][2] + dt*gy*FP[i][0]
                     - dt*gx*FP[i][1] - bias_dt*FP[i][5];
        P_pred[i][3] = FP[i][3];
        P_pred[i][4] = FP[i][4];
        P_pred[i][5] = FP[i][5];
    }

    for (int i = 0; i < 3; i++) {
        P_pred[i][i]   += p->Q_angle * dt;
        P_pred[i+3][i+3] += p->Q_bias * dt;
    }

    memcpy(p->P, P_pred, sizeof(p->P));

    /* === 4. GYRO_ONLY 退化检查 === */
    if (self->degrade == FILTER_DEGRADE_GYRO_ONLY) {
        filter_quat_to_euler(p->q0, p->q1, p->q2, p->q3, out);
        return;
    }

    /* === 5. 测量更新 (加速度计重力观测) === */

    /* 5a. ACC 归一化 + 门控 */
    float ax = in->ax, ay = in->ay, az = in->az;
    float acc_norm = sqrtf(ax * ax + ay * ay + az * az);
    if (acc_norm < ESKF_ACC_GATE_LOW || acc_norm > ESKF_ACC_GATE_HIGH) {
        filter_quat_to_euler(p->q0, p->q1, p->q2, p->q3, out);
        return;
    }
    ax /= acc_norm; ay /= acc_norm; az /= acc_norm;

    /* 5b. 预测的重力方向 h(q) = R(q)^T * [0,0,1] */
    float hx = 2.0f * (p->q1 * p->q3 - p->q0 * p->q2);
    float hy = 2.0f * (p->q0 * p->q1 + p->q2 * p->q3);
    float hz = 1.0f - 2.0f * (p->q1 * p->q1 + p->q2 * p->q2);

    /* 5c. 创新量 y = z - h */
    float y[3] = { ax - hx, ay - hy, az - hz };

    /* 5d. H = [skew(h), 0_3x3] */
    float H[3][6];
    memset(H, 0, sizeof(H));
    H[0][1] = -hz;  H[0][2] =  hy;
    H[1][0] =  hz;  H[1][2] = -hx;
    H[2][0] = -hy;  H[2][1] =  hx;

    /* 5e. PHt = P * H^T  (6x3), H nonzero only cols 0..2 */
    float PHt[6][3];
    for (int i = 0; i < 6; i++)
        for (int j = 0; j < 3; j++) {
            PHt[i][j] = p->P[i][0] * H[j][0]
                      + p->P[i][1] * H[j][1]
                      + p->P[i][2] * H[j][2];
        }

    /* 5f. S = H * PHt + R  (3x3) */
    float S[3][3];
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++) {
            S[i][j] = H[i][0] * PHt[0][j]
                    + H[i][1] * PHt[1][j]
                    + H[i][2] * PHt[2][j];
            if (i == j) S[i][j] += p->R_measure;
        }
    /* 强制对称 */
    for (int i = 0; i < 3; i++)
        for (int j = i + 1; j < 3; j++)
            S[i][j] = S[j][i] = 0.5f * (S[i][j] + S[j][i]);

    /* 5g. 条件数检查 */
    float max_S = eskf_max_abs_3(S[0][0], S[1][1], S[2][2]);
    if (max_S < ESKF_COV_MIN) max_S = ESKF_COV_MIN;
    float S00 = S[0][0], S01 = S[0][1], S02 = S[0][2];
    float S10 = S[1][0], S11 = S[1][1], S12 = S[1][2];
    float S20 = S[2][0], S21 = S[2][1], S22 = S[2][2];
    float det = S00 * (S11*S22 - S12*S21)
              - S01 * (S10*S22 - S12*S20)
              + S02 * (S10*S21 - S11*S20);
    if (fabsf(det) < ESKF_DET_EPS * max_S * max_S * max_S) {
        filter_quat_to_euler(p->q0, p->q1, p->q2, p->q3, out);
        return;
    }

    float S_inv[3][3];
    eskf_inv_3x3(S, S_inv);

    /* 5h. K = PHt * S_inv  (6x3) */
    float K[6][3];
    for (int i = 0; i < 6; i++)
        for (int j = 0; j < 3; j++) {
            K[i][j] = PHt[i][0] * S_inv[0][j]
                    + PHt[i][1] * S_inv[1][j]
                    + PHt[i][2] * S_inv[2][j];
        }

    /* 5i. 不可观方向投影：ACC 不能观测绕重力向量 h 的旋转。
     * 固定清零误差 z 轴仅在水平姿态成立；这里使用 I-h*h' 投影。 */
    for (int j = 0; j < 3; j++) {
        float along_gravity = hx*K[0][j] + hy*K[1][j] + hz*K[2][j];
        K[0][j] -= hx * along_gravity;
        K[1][j] -= hy * along_gravity;
        K[2][j] -= hz * along_gravity;
    }

    /* 5j. 误差状态更新 dx = K*y */
    float dx[6];
    for (int i = 0; i < 6; i++)
        dx[i] = K[i][0]*y[0] + K[i][1]*y[1] + K[i][2]*y[2];

    /* 5k. 注入误差到名义状态 */
    /* q = q ⊗ [1, dx[0:2]/2] */
    float dq_w = 1.0f;
    float dq_x = dx[0] * 0.5f;
    float dq_y = dx[1] * 0.5f;
    float dq_z = dx[2] * 0.5f;
    float w = p->q0*dq_w - p->q1*dq_x - p->q2*dq_y - p->q3*dq_z;
    float x = p->q0*dq_x + p->q1*dq_w + p->q2*dq_z - p->q3*dq_y;
    float y2 = p->q0*dq_y - p->q1*dq_z + p->q2*dq_w + p->q3*dq_x;
    float z = p->q0*dq_z + p->q1*dq_y - p->q2*dq_x + p->q3*dq_w;
    p->q0 = w; p->q1 = x; p->q2 = y2; p->q3 = z;
    eskf_quat_norm(&p->q0, &p->q1, &p->q2, &p->q3);

    /* bias += db */
    p->bias_x += dx[3]; p->bias_y += dx[4]; p->bias_z += dx[5];
    float bl = p->bias_limit_dps;
    if (p->bias_x >  bl) p->bias_x =  bl; else if (p->bias_x < -bl) p->bias_x = -bl;
    if (p->bias_y >  bl) p->bias_y =  bl; else if (p->bias_y < -bl) p->bias_y = -bl;
    if (p->bias_z >  bl) p->bias_z =  bl; else if (p->bias_z < -bl) p->bias_z = -bl;

    /* 5l. Joseph 协方差更新，保证单精度下的半正定性。
     * 复用预测阶段的 FP/P_pred 工作区，峰值栈占用不再增加。 */
    memset(FP, 0, sizeof(FP));              /* FP 此处复用为 A = I-KH */
    for (int i = 0; i < 6; i++) {
        FP[i][i] = 1.0f;
        for (int j = 0; j < 3; j++) {
            for (int kk = 0; kk < 3; kk++) {
                FP[i][j] -= K[i][kk] * H[kk][j];
            }
        }
    }

    /* P_pred 此处复用为 A*P。 */
    for (int i = 0; i < 6; i++) {
        for (int j = 0; j < 6; j++) {
            float sum = 0.0f;
            for (int kk = 0; kk < 6; kk++) sum += FP[i][kk] * p->P[kk][j];
            P_pred[i][j] = sum;
        }
    }

    for (int i = 0; i < 6; i++) {
        for (int j = 0; j < 6; j++) {
            float sum = 0.0f;
            for (int kk = 0; kk < 6; kk++) sum += P_pred[i][kk] * FP[j][kk];
            for (int kk = 0; kk < 3; kk++)
                sum += K[i][kk] * p->R_measure * K[j][kk];
            p->P[i][j] = sum;
        }
    }

    /* 强制对称 + 对角线保护 */
    for (int i = 0; i < 6; i++) {
        for (int j = i + 1; j < 6; j++) {
            float avg = 0.5f * (p->P[i][j] + p->P[j][i]);
            p->P[i][j] = p->P[j][i] = avg;
        }
        if (p->P[i][i] < ESKF_COV_MIN) p->P[i][i] = ESKF_COV_MIN;
        if (p->P[i][i] > ESKF_COV_MAX) p->P[i][i] = ESKF_COV_MAX;
    }

    /* === 6. ZUPT: 静止时 EMA 跟踪偏置 === */
    {
        float gyro_mag = sqrtf(in->gx*in->gx + in->gy*in->gy + in->gz*in->gz);
        float acc_err  = fabsf(acc_norm - 1.0f);
        if (gyro_mag < ESKF_ZUPT_GYRO_TH && acc_err < ESKF_ZUPT_ACC_TH) {
            p->bias_x += ESKF_ZUPT_RATE * (in->gx - p->bias_x);
            p->bias_y += ESKF_ZUPT_RATE * (in->gy - p->bias_y);
            p->bias_z += ESKF_ZUPT_RATE * (in->gz - p->bias_z);
            if (p->bias_x >  bl) p->bias_x =  bl; else if (p->bias_x < -bl) p->bias_x = -bl;
            if (p->bias_y >  bl) p->bias_y =  bl; else if (p->bias_y < -bl) p->bias_y = -bl;
            if (p->bias_z >  bl) p->bias_z =  bl; else if (p->bias_z < -bl) p->bias_z = -bl;
        }
    }

    /* === 7. 定期协方差正则化 === */
    p->update_count++;
    if (p->update_count >= ESKF_REG_INTERVAL) {
        p->update_count = 0;
        /* 重新强制对称 + 对角线裁剪交叉项 */
        for (int i = 0; i < 6; i++) {
            for (int j = i + 1; j < 6; j++) {
                float avg = 0.5f * (p->P[i][j] + p->P[j][i]);
                float bound = sqrtf(p->P[i][i] * p->P[j][j]);
                if (fabsf(avg) > bound)
                    avg = (avg > 0.0f ? bound : -bound);
                p->P[i][j] = p->P[j][i] = avg;
            }
            if (p->P[i][i] < ESKF_COV_MIN) p->P[i][i] = ESKF_COV_MIN;
        }
    }

    /* === 8. 输出 === */
    filter_quat_to_euler(p->q0, p->q1, p->q2, p->q3, out);
}

/* ---- 重置/参数/销毁 ---- */

void eskf_reset(filter_t *self)
{
    eskf_priv_t *p = (eskf_priv_t *)self->priv;
    p->q0 = 1.0f; p->q1 = p->q2 = p->q3 = 0.0f;
    p->bias_x = p->bias_y = p->bias_z = 0.0f;
    memset(p->P, 0, sizeof(p->P));
    for (int i = 0; i < 6; i++) p->P[i][i] = 1.0f;
    p->update_count = 0;
}

void eskf_set_param(filter_t *self, filter_param_t param, float value)
{
    if (validate_filter_param(param, value) != 0) return;
    eskf_priv_t *p = (eskf_priv_t *)self->priv;
    switch (param) {
        case FILTER_PARAM_Q_ANGLE:   p->Q_angle   = value; break;
        case FILTER_PARAM_Q_BIAS:    p->Q_bias    = value; break;
        case FILTER_PARAM_R_MEASURE: p->R_measure = value; break;
        case FILTER_PARAM_BIAS_LIMIT_DPS: p->bias_limit_dps = value; break;
        case FILTER_PARAM_CHI2_THRESHOLD:           break; /* ignored */
        default: break;
    }
}

static void eskf_destroy(filter_t *self)
{
    if (self->is_static) return;
    free(self->priv);
    free(self);
}

/* ---- 工厂函数 ---- */

filter_t* filter_create_eskf(float q_angle, float q_bias, float r_measure)
{
    filter_t *f = (filter_t *)malloc(sizeof(filter_t));
    if (!f) return NULL;
    eskf_priv_t *p = (eskf_priv_t *)malloc(sizeof(eskf_priv_t));
    if (!p) { free(f); return NULL; }

    p->q0 = 1.0f; p->q1 = p->q2 = p->q3 = 0.0f;
    p->bias_x = p->bias_y = p->bias_z = 0.0f;
    p->Q_angle = q_angle;
    p->Q_bias  = q_bias;
    p->R_measure = r_measure;
    p->bias_limit_dps = EKF_BIAS_LIMIT_DEFAULT;
    p->update_count = 0;
    memset(p->P, 0, sizeof(p->P));
    for (int i = 0; i < 6; i++) p->P[i][i] = 1.0f;

    f->update    = eskf_update;
    f->reset     = eskf_reset;
    f->set_param = eskf_set_param;
    f->destroy   = eskf_destroy;
    f->type      = FILTER_TYPE_ESKF;
    f->degrade   = FILTER_DEGRADE_NONE;
    f->priv      = p;
    f->is_static = 0;

    return f;
}
