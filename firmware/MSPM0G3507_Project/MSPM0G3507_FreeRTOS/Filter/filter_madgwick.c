/**
 * @file    filter_madgwick.c
 * @brief   Madgwick 滤波器实现
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

/* ============================================================
 * 5. Madgwick 滤波器
 * ============================================================ */

void madgwick_update(filter_t *self, const filter_input_t *in, filter_output_t *out)
{
    madgwick_priv_t *p = (madgwick_priv_t *)self->priv;
    float dt = in->dt;

    /* 输入验证 */
    if (dt <= 0.0f || isnan(in->ax) || isinf(in->ax) ||
        isnan(in->ay) || isinf(in->ay) ||
        isnan(in->az) || isinf(in->az) ||
        isnan(in->gx) || isinf(in->gx) ||
        isnan(in->gy) || isinf(in->gy) ||
        isnan(in->gz) || isinf(in->gz)) {
        filter_quat_to_euler(p->q0, p->q1, p->q2, p->q3, out);
        return;
    }
    /* 退化模式检查 */
    if (self->degrade == FILTER_DEGRADE_HOLD_LAST) {
        filter_quat_to_euler(p->q0, p->q1, p->q2, p->q3, out);
        return;
    }

    if (self->degrade == FILTER_DEGRADE_GYRO_ONLY) {
        /* 仅陀螺仪：跳过梯度下降修正，只做四元数积分 */
        float gx = in->gx * M_PI / 180.0f;
        float gy = in->gy * M_PI / 180.0f;
        float gz = in->gz * M_PI / 180.0f;
        filter_quat_integrate_gyro(&p->q0, &p->q1, &p->q2, &p->q3, gx, gy, gz, dt);
        filter_quat_to_euler(p->q0, p->q1, p->q2, p->q3, out);
        return;
    }

    if (self->degrade == FILTER_DEGRADE_ACC_ONLY) {
        /* 仅加速度计：从ACC计算姿态角并重置四元数 */
        filter_acc_to_quat(in->ax, in->ay, in->az,
                           &p->q0, &p->q1, &p->q2, &p->q3);
        filter_quat_to_euler(p->q0, p->q1, p->q2, p->q3, out);
        return;
    }

    float ax = in->ax, ay = in->ay, az = in->az;
    float gx = in->gx * M_PI / 180.0f;
    float gy = in->gy * M_PI / 180.0f;
    float gz = in->gz * M_PI / 180.0f;

    float q0 = p->q0, q1 = p->q1, q2 = p->q2, q3 = p->q3;

    /* 归一化加速度 */
    float norm = mathacl_sqrtf(ax * ax + ay * ay + az * az);
    if (norm > 0.0f) {
        ax /= norm; ay /= norm; az /= norm;
    }

    /* 梯度下降步 */
    float _2q0 = 2.0f * q0, _2q1 = 2.0f * q1, _2q2 = 2.0f * q2, _2q3 = 2.0f * q3;
    float _4q0 = 4.0f * q0, _4q1 = 4.0f * q1, _4q2 = 4.0f * q2;
    float _8q1 = 8.0f * q1, _8q2 = 8.0f * q2;
    float q0q0 = q0 * q0, q1q1 = q1 * q1, q2q2 = q2 * q2, q3q3 = q3 * q3;

    /* 梯度下降法修正 */
    float s0 = _4q0 * q2q2 + _2q2 * ax + _4q0 * q1q1 - _2q1 * ay;
    float s1 = _4q1 * q3q3 - _2q3 * ax + 4.0f * q0q0 * q1 - _2q0 * ay - _4q1 + _8q1 * q1q1 + _8q1 * q2q2 + _4q1 * az;
    float s2 = 4.0f * q0q0 * q2 + _2q0 * ax + _4q2 * q3q3 - _2q3 * ay - _4q2 + _8q2 * q1q1 + _8q2 * q2q2 + _4q2 * az;
    float s3 = 4.0f * q1q1 * q3 - _2q1 * ax + 4.0f * q2q2 * q3 - _2q2 * ay;

    norm = mathacl_sqrtf(s0 * s0 + s1 * s1 + s2 * s2 + s3 * s3);
    if (norm > 0.0f) {
        s0 /= norm; s1 /= norm; s2 /= norm; s3 /= norm;
    }

    /* 四元数微分方程 */
    float qDot0 = 0.5f * (-q1 * gx - q2 * gy - q3 * gz) - p->beta * s0;
    float qDot1 = 0.5f * ( q0 * gx + q2 * gz - q3 * gy) - p->beta * s1;
    float qDot2 = 0.5f * ( q0 * gy - q1 * gz + q3 * gx) - p->beta * s2;
    float qDot3 = 0.5f * ( q0 * gz + q1 * gy - q2 * gx) - p->beta * s3;

    /* 积分 */
    p->q0 += qDot0 * dt;
    p->q1 += qDot1 * dt;
    p->q2 += qDot2 * dt;
    p->q3 += qDot3 * dt;

    /* 归一化 */
    normalize_quaternion_internal(&p->q0, &p->q1, &p->q2, &p->q3);

    /* 输出 */
    filter_quat_to_euler(p->q0, p->q1, p->q2, p->q3, out);
}

void madgwick_reset(filter_t *self)
{
    madgwick_priv_t *p = (madgwick_priv_t *)self->priv;
    p->q0 = 1.0f; p->q1 = p->q2 = p->q3 = 0.0f;
}

void madgwick_set_param(filter_t *self, filter_param_t param, float value)
{
    if (validate_filter_param(param, value) != 0) {
        return;  /* 参数无效，拒绝设置 */
    }

    madgwick_priv_t *p = (madgwick_priv_t *)self->priv;
    if (param == FILTER_PARAM_KP) p->beta = value;  /* Madgwick 使用 beta */
}

static void madgwick_destroy(filter_t *self)
{
    if (self->is_static) return;
    free(self->priv);
    free(self);
}

filter_t* filter_create_madgwick(float beta)
{
    filter_t *f = (filter_t *)malloc(sizeof(filter_t));
    if (!f) return NULL;

    madgwick_priv_t *p = (madgwick_priv_t *)malloc(sizeof(madgwick_priv_t));
    if (!p) { free(f); return NULL; }

    p->beta = beta;
    p->q0 = 1.0f; p->q1 = p->q2 = p->q3 = 0.0f;

    f->update    = madgwick_update;
    f->reset     = madgwick_reset;
    f->set_param = madgwick_set_param;
    f->destroy   = madgwick_destroy;
    f->type      = FILTER_TYPE_MADGWICK;
    f->degrade   = FILTER_DEGRADE_NONE;
    f->priv      = p;
    f->is_static = 0;

    return f;
}
