/**
 * @file    filter_lkf.c
 * @brief   线性卡尔曼滤波器 (LKF) 实现 — 协方差正定性修复版
 *
 * 状态向量: x = [pitch, roll, yaw, bias_x, bias_y, bias_z] (6维)
 *
 * ## 算法结构
 *   预测:
 *     x' = F*x   (F: 6x6, 含 -dt*bias 交叉项)
 *     P' = F*P*F^T + Q
 *   更新 (仅 pitch/roll 有 ACC 观测):
 *     H = [[1,0,0,0,0,0], [0,1,0,0,0,0]]
 *     S = H*P*H^T + R   (2x2 对角)
 *     K = P*H^T * S^-1  (6x2)
 *     x = x + K*y
 *     P = (I-KH)*P*(I-KH)^T + K*R*K^T   // Joseph 形式
 *
 * ## v2 修复 (协方差正定性):
 *   [Fix-1] Joseph 形式: 替代原来的 P=(I-KH)P, 保证对称正定
 *   [Fix-2] 每 500 帧: 检查对角 > 1e-10, 交叉项 <= sqrt(Pii*Pjj)
 *   [Fix-3] 近奇异保护: trace(P) < 1e-4 → P *= 10 膨胀
 *   [Fix-4] 最小 Q 防护: Q*dt >= 1e-10, 防止协方差收敛到零
 *
 * 与 ESKF/EKF 的区别:
 *   - 无需四元数/雅可比: 状态直接欧拉角, 线性观测模型
 *   - 运算量小: 6x6 vs 7x7 矩阵, 无矩阵求逆 (S 为对角)
 *   - 适用: 低算力/教学/快速原型
 */

#include "filter_internal.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "bsp_mathacl.h"

/* ---- 数值安全阈值 ---- */
#define LKF_COV_MIN         1e-10f   /* 最小对角元素 */
#define LKF_COV_MAX         1e6f     /* 最大对角元素 */
#define LKF_TRACE_REINFLATE 1e-4f    /* 迹低于此值→膨胀10倍 */
#define LKF_MIN_Q_ANGLE     1e-10f   /* dt 已乘，防止零 Q */
#define LKF_MIN_Q_BIAS      1e-10f
#define LKF_CHECK_INTERVAL  500      /* 正定性检查间隔 (帧) */

/*
 * [Fix-2] 检查协方差正确性并修复
 * - 对角线: P[i][i] 必须在 [LKF_COV_MIN, LKF_COV_MAX]
 * - 交叉项: |P[i][j]| <= sqrt(P[i][i] * P[j][j]) (Cauchy-Schwarz)
 * - 近奇异: trace < LKF_TRACE_REINFLATE → 膨胀 P * 10
 */
static void lkf_ensure_positive_definite(float P[][6])
{
    /* 计算迹并检查 */
    float trace = 0.0f;
    for (int i = 0; i < 6; i++) {
        if (P[i][i] < LKF_COV_MIN) P[i][i] = LKF_COV_MIN;
        if (P[i][i] > LKF_COV_MAX) P[i][i] = LKF_COV_MAX;
        trace += P[i][i];
    }

    /* [Fix-3] 近奇异: 协方差过小导致滤波器"过于自信", 膨胀 */
    if (trace < LKF_TRACE_REINFLATE) {
        for (int i = 0; i < 6; i++)
            P[i][i] *= 10.0f;
    }

    /* Cauchy-Schwarz 约束交叉项 */
    for (int i = 0; i < 6; i++) {
        for (int j = i + 1; j < 6; j++) {
            float bound = sqrtf(fmaxf(0.0f, P[i][i] * P[j][j]));
            float avg = 0.5f * (P[i][j] + P[j][i]);
            if (avg >  bound) avg =  bound;
            if (avg < -bound) avg = -bound;
            P[i][j] = avg;
            P[j][i] = avg;
        }
    }
}

void lkf_update(filter_t *self, const filter_input_t *in, filter_output_t *out)
{
    lkf_priv_t *p = (lkf_priv_t *)self->priv;
    float dt = in->dt;

    /* === 输入验证 === */
    if (dt <= 0.0f || dt > 1.0f ||
        isnan(in->ax) || isinf(in->ax) ||
        isnan(in->ay) || isinf(in->ay) ||
        isnan(in->az) || isinf(in->az) ||
        isnan(in->gx) || isinf(in->gx) ||
        isnan(in->gy) || isinf(in->gy) ||
        isnan(in->gz) || isinf(in->gz)) {
        out->pitch = p->x[0]; out->roll = p->x[1]; out->yaw = p->x[2];
        out->q0 = 1.0f; out->q1 = out->q2 = out->q3 = 0.0f;
        return;
    }
    if (self->degrade == FILTER_DEGRADE_HOLD_LAST) {
        out->pitch = p->x[0]; out->roll = p->x[1]; out->yaw = p->x[2];
        out->q0 = 1.0f; out->q1 = out->q2 = out->q3 = 0.0f;
        return;
    }

    /*
     * === 1. 状态预测 (线性定常系统) ===
     * x' = F * x  (展开形式, 省去完整矩阵乘法)
     *
     * F 矩阵结构:
     *   [1  0  0   0 -dt  0 ]   角度行: angle' = angle + (gyro_rate) * dt
     *   [0  1  0 -dt   0  0 ]          = angle + (gyro - bias) * dt
     *   [0  0  1   0   0 -dt]   偏置行: bias' = bias
     *   [0  0  0   1   0  0 ]
     *   [0  0  0   0   1  0 ]
     *   [0  0  0   0   0  1 ]
     */
    float pitch_pred = p->x[0] + (in->gy - p->x[4]) * dt;
    float roll_pred  = p->x[1] - (in->gx - p->x[3]) * dt;
    float yaw_pred   = p->x[2] + (in->gz - p->x[5]) * dt;

    /*
     * === 2. 协方差预测: P' = F*P*F^T + Q ===
     * 展开为解析形式 (省去 6x6 完整矩阵乘法)
     *
     * 关键推导:
     *   P_new[0][0] = P[0][0] + dt²*P[4][4] - 2*dt*P[0][4] + Q_angle
     *   P_new[0][4] = P[0][4] - dt*P[4][4]
     *   ... (利用 F 的稀疏性: 仅对角线 + 2 处 -dt)
     */
    float P[6][6];
    memcpy(P, p->P, sizeof(P));

    float dt2 = dt * dt;

    /* [Fix-4] 最小 Q 防护: 确保 Q*dt 不退化 */
    float Qa = fmaxf(p->Q_angle * dt, LKF_MIN_Q_ANGLE);
    float Qb = fmaxf(p->Q_bias * dt, LKF_MIN_Q_BIAS);

    /* 对角线预测 (含 Q) */
    P[0][0] += P[4][4] * dt2 - 2.0f * P[0][4] * dt + Qa;
    P[1][1] += P[3][3] * dt2 - 2.0f * P[1][3] * dt + Qa;
    P[2][2] += P[5][5] * dt2 - 2.0f * P[2][5] * dt + Qa;
    P[3][3] += Qb;
    P[4][4] += Qb;
    P[5][5] += Qb;

    /* 交叉项预测 (对称维护) */
    P[0][4] -= P[4][4] * dt;  P[4][0] = P[0][4];
    P[1][3] -= P[3][3] * dt;  P[3][1] = P[1][3];
    P[2][5] -= P[5][5] * dt;  P[5][2] = P[2][5];

    /* === 3. 测量更新 === */
    if (self->degrade == FILTER_DEGRADE_GYRO_ONLY) {
        /* GYRO_ONLY: 仅预测 */
        p->x[0] = pitch_pred; p->x[1] = roll_pred; p->x[2] = yaw_pred;
    } else {
        float acc_norm = sqrtf(in->ax * in->ax + in->ay * in->ay + in->az * in->az);

        if (self->degrade != FILTER_DEGRADE_ACC_ONLY &&
            (acc_norm < DEGRADE_ACC_LOW || acc_norm > DEGRADE_ACC_HIGH)) {
            p->x[0] = pitch_pred; p->x[1] = roll_pred; p->x[2] = yaw_pred;
            goto lkf_update_done;
        }

        /* 加速度计反算 pitch/roll (atan2 公式, 与 KF 一致) */
        float acc_pitch = atan2f(-in->ax, sqrtf(in->ay * in->ay + in->az * in->az)) * 180.0f / M_PI;
        float acc_roll  = atan2f( in->ay, sqrtf(in->ax * in->ax + in->az * in->az)) * 180.0f / M_PI;

        if (self->degrade == FILTER_DEGRADE_ACC_ONLY) {
            p->x[0] = acc_pitch; p->x[1] = acc_roll;
        } else {
            /*
             * === 3a. 卡尔曼增益 (解析形式) ===
             * H = [[1,0,0,0,0,0], [0,1,0,0,0,0]] → S 退化为对角
             * S[0][0] = P[0][0] + R   (pitch 观测)
             * S[1][1] = P[1][1] + R   (roll 观测)
             * K = P * H^T * S^-1
             *
             * 优化: 直接展开 6x2 增益 (H 的稀疏性: 2个1, 其余0)
             *   K[i][0] = P[i][0] / S0   (pitch 增益, 所有6个状态)
             *   K[i][1] = P[i][1] / S1   (roll 增益)
             */
            float S0 = P[0][0] + p->R_measure;
            float S1 = P[1][1] + p->R_measure;

            float Ky0[6], Ky1[6];  /* K[:,0] 和 K[:,1] */
            Ky0[0] = P[0][0] / S0; Ky1[0] = P[0][1] / S1;
            Ky0[1] = P[1][0] / S0; Ky1[1] = P[1][1] / S1;
            Ky0[2] = P[2][0] / S0; Ky1[2] = P[2][1] / S1;
            Ky0[3] = P[3][0] / S0; Ky1[3] = P[3][1] / S1;
            Ky0[4] = P[4][0] / S0; Ky1[4] = P[4][1] / S1;
            Ky0[5] = P[5][0] / S0; Ky1[5] = P[5][1] / S1;

            /* === 3b. 状态更新: x = x + K*y === */
            float y0 = acc_pitch - pitch_pred;
            float y1 = acc_roll  - roll_pred;
            p->x[0] = pitch_pred + Ky0[0] * y0 + Ky1[0] * y1;
            p->x[1] = roll_pred  + Ky0[1] * y0 + Ky1[1] * y1;
            p->x[2] = yaw_pred   + Ky0[2] * y0 + Ky1[2] * y1;
            p->x[3] += Ky0[3] * y0 + Ky1[3] * y1;
            p->x[4] += Ky0[4] * y0 + Ky1[4] * y1;
            p->x[5] += Ky0[5] * y0 + Ky1[5] * y1;

            /*
             * [Fix-1] === 3c. Joseph 形式协方差更新 ===
             * P = (I-KH)*P*(I-KH)^T + K*R*K^T
             *
             * 数学性质:
             *   - 二次型 (I-KH)*P*(I-KH)^T 保证对称
             *   - +K*R*K^T 保证正定 (R > 0 → K*R*K^T 半正定, P 为正定)
             *   - 数值无条件稳定 (不依赖 K 的正交性)
             *
             * 原公式 P = (I-KH)*P 在 float 精度下
             * 长期运行会丢失正定性, 导致协方差"萎缩"
             *
             * 展开 (利用 H 稀疏性):
             *   (I-KH) = I - Ky0*H[0,:] - Ky1*H[1,:]
             *          = I - Ky0*e0^T - Ky1*e1^T    (e0,e1 为基向量)
             *   (I-KH)[i][j] = 1(i==j) - Ky0[i]*1(j==0) - Ky1[i]*1(j==1)
             *
             * T = (I-KH)*P:
             *   T[i][j] = P[i][j] - Ky0[i]*P[0][j] - Ky1[i]*P[1][j]
             *
             * P_new = T * (I-KH)^T + K*R*K^T:
             *   P_new[i][j] = T[i][j] - T[i][0]*Ky0[j] - T[i][1]*Ky1[j]
             *                + Ky0[i]*R*Ky0[j] + Ky1[i]*R*Ky1[j]
             */
            {
                float R = p->R_measure;
                float P_new[6][6];

                /* 展开 (I-KH)^T 乘法: P_new[i][j] = T[i][j] - T[i][0]*Ky0[j] - T[i][1]*Ky1[j] + ... */
                for (int i = 0; i < 6; i++) {
                    for (int j = 0; j < 6; j++) {
                        float T_ij = P[i][j] - Ky0[i]*P[0][j] - Ky1[i]*P[1][j];
                        P_new[i][j] = T_ij
                                    - (P[i][0] - Ky0[i]*P[0][0] - Ky1[i]*P[1][0]) * Ky0[j]
                                    - (P[i][1] - Ky0[i]*P[0][1] - Ky1[i]*P[1][1]) * Ky1[j]
                                    + Ky0[i]*R*Ky0[j] + Ky1[i]*R*Ky1[j];
                    }
                }

                memcpy(P, P_new, sizeof(P));
            }
        }
    }

lkf_update_done:

    /* Yaw wrap-around */
    while (p->x[2] > 180.0f)  p->x[2] -= 360.0f;
    while (p->x[2] < -180.0f) p->x[2] += 360.0f;

    /* [Fix-2][Fix-3] 定期正定性检查 */
    p->update_count++;
    if (p->update_count >= LKF_CHECK_INTERVAL) {
        p->update_count = 0;
        lkf_ensure_positive_definite(P);
    }

    /* 保存协方差 */
    memcpy(p->P, P, sizeof(P));

    /* NaN/Inf 保护 */
    for (int i = 0; i < 6; i++) {
        if (isnan(p->x[i]) || isinf(p->x[i]) ||
            isnan(p->P[i][i]) || isinf(p->P[i][i])) {
            lkf_reset(self);
            break;
        }
    }

    /* 输出 */
    out->pitch = p->x[0]; out->roll = p->x[1]; out->yaw = p->x[2];
    out->q0 = 1.0f; out->q1 = out->q2 = out->q3 = 0.0f;
}

void lkf_reset(filter_t *self)
{
    lkf_priv_t *p = (lkf_priv_t *)self->priv;
    memset(p->x, 0, sizeof(p->x));
    memset(p->P, 0, sizeof(p->P));
    for (int i = 0; i < 6; i++) p->P[i][i] = 1.0f;
    p->update_count = 0;
}

void lkf_set_param(filter_t *self, filter_param_t param, float value)
{
    if (validate_filter_param(param, value) != 0) return;
    lkf_priv_t *p = (lkf_priv_t *)self->priv;
    switch (param) {
        case FILTER_PARAM_Q_ANGLE:   p->Q_angle   = value; break;
        case FILTER_PARAM_Q_BIAS:    p->Q_bias    = value; break;
        case FILTER_PARAM_R_MEASURE: p->R_measure = value; break;
        default: break;
    }
}

static void lkf_destroy(filter_t *self)
{
    if (self->is_static) return;
    free(self->priv);
    free(self);
}

filter_t* filter_create_lkf(float q_angle, float q_bias, float r_measure)
{
    filter_t *f = (filter_t *)malloc(sizeof(filter_t));
    if (!f) return NULL;
    lkf_priv_t *p = (lkf_priv_t *)malloc(sizeof(lkf_priv_t));
    if (!p) { free(f); return NULL; }

    memset(p->x, 0, sizeof(p->x));
    memset(p->P, 0, sizeof(p->P));
    for (int i = 0; i < 6; i++) p->P[i][i] = 1.0f;
    p->Q_angle  = q_angle;
    p->Q_bias   = q_bias;
    p->R_measure = r_measure;
    p->update_count = 0;

    f->update    = lkf_update;
    f->reset     = lkf_reset;
    f->set_param = lkf_set_param;
    f->destroy   = lkf_destroy;
    f->type      = FILTER_TYPE_LKF;
    f->degrade   = FILTER_DEGRADE_NONE;
    f->priv      = p;
    f->is_static = 0;
    return f;
}
