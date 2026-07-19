/**
 * @file    filter_lkf.c
 * @brief   线性卡尔曼滤波器 (LKF) 实现
 *
 * 线性卡尔曼滤波器 - 6状态线性近似
 *
 * 状态向量：x = [pitch, roll, yaw, bias_x, bias_y, bias_z]
 *
 * 与EKF的区别：
 *   - 测量模型是线性的（直接观测 pitch/roll）
 *   - 不需要计算雅可比矩阵
 *   - 卡尔曼增益可离线预计算（稳态时）
 *   - 计算量比EKF小约30-50%
 *
 * 适用场景：
 *   - 单轴角度估计（线性近似）
 *   - 教学用途
 *   - 低算力场景的快速滤波
 */

#include "filter_internal.h"

#include <stdbool.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <stdio.h>

/* MATHACL 硬件加速支持 */
#include "bsp_mathacl.h"
#include "mathacl_matrix.h"

/* LKF 数值安全阈值 */
#define LKF_COV_MIN_LIMIT   1e-10f
#define LKF_COV_MAX_LIMIT   1e6f
#define LKF_NORM_EPS        1e-10f

void lkf_update(filter_t *self, const filter_input_t *in, filter_output_t *out)
{
    lkf_priv_t *p = (lkf_priv_t *)self->priv;
    float dt = in->dt;

    /* 输入验证：检查所有 6 轴传感器和 dt 范围 */
    if (dt <= 0.0f || dt > 1.0f ||
        isnan(in->ax) || isinf(in->ax) ||
        isnan(in->ay) || isinf(in->ay) ||
        isnan(in->az) || isinf(in->az) ||
        isnan(in->gx) || isinf(in->gx) ||
        isnan(in->gy) || isinf(in->gy) ||
        isnan(in->gz) || isinf(in->gz)) {
        out->pitch = p->x[0]; out->roll = p->x[1]; out->yaw = p->x[2];
        out->q0 = out->q1 = out->q2 = out->q3 = 0.0f;
        return;
    }

    /* 退化模式：HOLD_LAST */
    if (self->degrade == FILTER_DEGRADE_HOLD_LAST) {
        out->pitch = p->x[0]; out->roll = p->x[1]; out->yaw = p->x[2];
        out->q0 = out->q1 = out->q2 = out->q3 = 0.0f;
        return;
    }

    /* === 预测步骤 === */
    /* 状态转移：x = F*x + B*u
     * pitch' = pitch + gy*dt - by*dt   (by = 陀螺仪Y轴偏置)
     * roll'  = roll  - gx*dt - bx*dt   (bx = 陀螺仪X轴偏置)
     * yaw'   = yaw   + gz*dt - bz*dt
     * bias'  = bias  (假设偏置不变)
     */
    float pitch_pred = p->x[0] + (in->gy - p->x[4]) * dt;
    float roll_pred  = p->x[1] - (in->gx - p->x[3]) * dt;
    float yaw_pred   = p->x[2] + (in->gz - p->x[5]) * dt;

    /* 协方差预测：P = F*P*F' + Q
     * F = [1  0  0 -dt  0   0 ]
     *     [0  1  0   0 -dt  0 ]
     *     [0  0  1   0  0  -dt]
     *     [0  0  0   1   0   0 ]
     *     [0  0  0   0   1   0 ]
     *     [0  0  0   0   0   1 ]
     */
    /* 简化计算：只更新对角线和相关项 */
    float P[LKF_STATE_SIZE][LKF_STATE_SIZE];
    memcpy(P, p->P, sizeof(P));

    /* P_new = F*P*F' + Q (简化形式)
     * F = [1  0  0   0 -dt  0 ]
     *     [0  1  0 -dt  0   0 ]
     *     [0  0  1   0   0  -dt]
     *     [0  0  0   1   0   0 ]
     *     [0  0  0   0   1   0 ]
     *     [0  0  0   0   0   1 ]
     */
    float dt2 = dt * dt;
    /* 对角线元素更新 */
    P[0][0] += (P[4][4] * dt2 - 2.0f * P[0][4] * dt) + p->Q_angle;
    P[1][1] += (P[3][3] * dt2 - 2.0f * P[1][3] * dt) + p->Q_angle;
    P[2][2] += (P[5][5] * dt2 - 2.0f * P[2][5] * dt) + p->Q_angle;
    P[3][3] += p->Q_bias;
    P[4][4] += p->Q_bias;
    P[5][5] += p->Q_bias;

    /* 交叉项更新 */
    P[0][4] -= P[4][4] * dt;
    P[1][3] -= P[3][3] * dt;
    P[2][5] -= P[5][5] * dt;
    P[4][0] = P[0][4];
    P[3][1] = P[1][3];
    P[5][2] = P[2][5];

    /* === 更新步骤 === */
    if (self->degrade == FILTER_DEGRADE_GYRO_ONLY) {
        /* GYRO_ONLY: 仅预测，不更新 */
        p->x[0] = pitch_pred;
        p->x[1] = roll_pred;
        p->x[2] = yaw_pred;
    } else {
        /* 测量：acc_pitch, acc_roll */
        float acc_pitch = mathacl_atan2f(-in->ax, mathacl_sqrtf(in->ay * in->ay + in->az * in->az)) * 180.0f / M_PI;
        float acc_roll  = mathacl_atan2f( in->ay, mathacl_sqrtf(in->ax * in->ax + in->az * in->az)) * 180.0f / M_PI;

        if (self->degrade == FILTER_DEGRADE_ACC_ONLY) {
            /* ACC_ONLY: 直接使用ACC */
            p->x[0] = acc_pitch;
            p->x[1] = acc_roll;
        } else {
            /* 正常模式：卡尔曼更新 */
            /* 卡尔曼增益：K = P*H'/(H*P*H' + R)
             * H = [1 0 0 0 0 0]  (观测pitch)
             *     [0 1 0 0 0 0]  (观测roll)
             */
            float S0 = P[0][0] + p->R_measure;
            float S1 = P[1][1] + p->R_measure;
            float K0 = P[0][0] / S0;
            float K1 = P[1][1] / S1;

            /* 状态更新：x = x + K*(z - H*x) */
            p->x[0] = pitch_pred + K0 * (acc_pitch - pitch_pred);
            p->x[1] = roll_pred  + K1 * (acc_roll  - roll_pred);
            p->x[2] = yaw_pred;

            /* 协方差更新：P = (I - K*H)*P
             * H = [[1,0,0,0,0,0], [0,1,0,0,0,0]]
             * S = diag(S0, S1), K_i0 = P[i][0]/S0, K_i1 = P[i][1]/S1
             * P_new[i][j] = P[i][j] - K_i0*P[0][j] - K_i1*P[1][j]
             */
            float S0_inv = 1.0f / S0;
            float S1_inv = 1.0f / S1;
            float P_new_lkf[LKF_STATE_SIZE][LKF_STATE_SIZE];
            for (int i = 0; i < LKF_STATE_SIZE; i++) {
                float Ki0 = P[i][0] * S0_inv;
                float Ki1 = P[i][1] * S1_inv;
                for (int j = 0; j < LKF_STATE_SIZE; j++) {
                    P_new_lkf[i][j] = P[i][j] - Ki0 * P[0][j] - Ki1 * P[1][j];
                }
            }
            memcpy(P, P_new_lkf, sizeof(P_new_lkf));

            /* 强制对称和正定性 */
            for (int i = 0; i < LKF_STATE_SIZE; i++) {
                for (int j = i + 1; j < LKF_STATE_SIZE; j++) {
                    float avg = 0.5f * (P[i][j] + P[j][i]);
                    P[i][j] = avg;
                    P[j][i] = avg;
                }
                if (P[i][i] < LKF_COV_MIN_LIMIT) P[i][i] = LKF_COV_MIN_LIMIT;
                if (P[i][i] > LKF_COV_MAX_LIMIT) P[i][i] = LKF_COV_MAX_LIMIT;
            }
        }
    }

    /* Yaw wrap-around */
    while (p->x[2] > 180.0f)  p->x[2] -= 360.0f;
    while (p->x[2] < -180.0f) p->x[2] += 360.0f;

    /* 保存状态 */
    memcpy(p->P, P, sizeof(P));

    /* 数值异常保护：若状态或协方差发散，重置滤波器 */
    bool lkf_healthy = true;
    for (int i = 0; i < LKF_STATE_SIZE && lkf_healthy; i++) {
        if (isnan(p->x[i]) || isinf(p->x[i]) ||
            p->P[i][i] < -LKF_COV_MAX_LIMIT || p->P[i][i] > LKF_COV_MAX_LIMIT) {
            lkf_healthy = false;
            break;
        }
        for (int j = 0; j < LKF_STATE_SIZE; j++) {
            if (isnan(p->P[i][j]) || isinf(p->P[i][j])) {
                lkf_healthy = false;
                break;
            }
        }
    }
    if (!lkf_healthy) {
        lkf_reset(self);
    }

    /* 输出 */
    out->pitch = p->x[0];
    out->roll  = p->x[1];
    out->yaw   = p->x[2];
    out->q0 = out->q1 = out->q2 = out->q3 = 0.0f;
}

void lkf_reset(filter_t *self)
{
    lkf_priv_t *p = (lkf_priv_t *)self->priv;
    memset(p->x, 0, sizeof(p->x));
    memset(p->P, 0, sizeof(p->P));
    /* 初始化协方差为单位矩阵 */
    for (int i = 0; i < LKF_STATE_SIZE; i++) {
        p->P[i][i] = 1.0f;
    }
}

void lkf_set_param(filter_t *self, filter_param_t param, float value)
{
    if (validate_filter_param(param, value) != 0) {
        return;  /* 参数无效，拒绝设置 */
    }

    lkf_priv_t *p = (lkf_priv_t *)self->priv;
    switch (param) {
        case FILTER_PARAM_Q_ANGLE:  p->Q_angle  = value; break;
        case FILTER_PARAM_Q_BIAS:   p->Q_bias   = value; break;
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
    for (int i = 0; i < LKF_STATE_SIZE; i++) {
        p->P[i][i] = 1.0f;
    }
    p->Q_angle  = q_angle;
    p->Q_bias   = q_bias;
    p->R_measure = r_measure;

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
