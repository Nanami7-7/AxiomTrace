/**
 * @file    filter_lpf.c
 * @brief   一阶低通滤波器 (Low-Pass Filter) 实现
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
 * 2. 一阶低通滤波器 (Low-Pass Filter)
 * ============================================================ */

void lpf_update(filter_t *self, const filter_input_t *in, filter_output_t *out)
{
    lpf_priv_t *p = (lpf_priv_t *)self->priv;
    float dt = in->dt;  /* 接口为 double, 本地显式收窄为 float */

    /* 输入验证 */
    if (dt <= 0.0f || isnan(in->ax) || isinf(in->ax) ||
        isnan(in->ay) || isinf(in->ay) ||
        isnan(in->az) || isinf(in->az) ||
        isnan(in->gx) || isinf(in->gx) ||
        isnan(in->gy) || isinf(in->gy) ||
        isnan(in->gz) || isinf(in->gz)) {
        out->pitch = p->pitch;
        out->roll  = p->roll;
        out->yaw   = p->yaw;
        out->q0 = 1.0f; out->q1 = out->q2 = out->q3 = 0.0f;
        return;
    }
    /* 计算 ACC 姿态角 */
    float acc_pitch = mathacl_atan2f(-in->ax, mathacl_sqrtf(in->ay * in->ay + in->az * in->az)) * 180.0f / M_PI;
    float acc_roll  = mathacl_atan2f( in->ay, mathacl_sqrtf(in->ax * in->ax + in->az * in->az)) * 180.0f / M_PI;

    /* 退化模式检查 */
    if (self->degrade == FILTER_DEGRADE_HOLD_LAST) {
        out->pitch = p->pitch; out->roll = p->roll; out->yaw = p->yaw;
        out->q0 = 1.0f; out->q1 = out->q2 = out->q3 = 0.0f;
        return;
    }

    if (self->degrade == FILTER_DEGRADE_GYRO_ONLY) {
        /* 仅陀螺仪积分，跳过ACC修正 */
        p->pitch += in->gy * dt;
        p->roll  -= in->gx * dt;
        p->yaw   += in->gz * dt;
    } else if (self->degrade == FILTER_DEGRADE_ACC_ONLY) {
        /* 仅加速度计，跳过GYRO积分，yaw保持不变 */
        p->pitch = acc_pitch;
        p->roll  = acc_roll;
    } else {
        /* 正常模式 + STATIC_ONLY（需外部静止检测） */
        float rc = 1.0f / (2.0f * M_PI * p->cutoff_freq);
        float alpha = dt / (rc + dt);
        p->pitch = p->prev_pitch + alpha * (acc_pitch + in->gy * dt - p->prev_pitch);
        p->roll  = p->prev_roll  + alpha * (acc_roll  - in->gx * dt - p->prev_roll);
        p->yaw  += in->gz * dt;
    }
    /* Yaw wrap-around */
    while (p->yaw > 180.0f)  p->yaw -= 360.0f;
    while (p->yaw < -180.0f) p->yaw += 360.0f;

    /* 保存当前值 */
    p->prev_pitch = p->pitch;
    p->prev_roll  = p->roll;

    /* 输出 */
    out->pitch = p->pitch;
    out->roll  = p->roll;
    out->yaw   = p->yaw;
    out->q0 = 1.0f; out->q1 = out->q2 = out->q3 = 0.0f;
}

void lpf_reset(filter_t *self)
{
    lpf_priv_t *p = (lpf_priv_t *)self->priv;
    p->pitch = p->roll = p->yaw = 0.0f;
    p->prev_pitch = p->prev_roll = p->prev_yaw = 0.0f;
}

void lpf_set_param(filter_t *self, filter_param_t param, float value)
{
    lpf_priv_t *p = (lpf_priv_t *)self->priv;
    if (validate_filter_param(param, value) != 0) return;
    if (param == FILTER_PARAM_CUTOFF_FREQ) {
        p->cutoff_freq = value;
    }
}

static void lpf_destroy(filter_t *self)
{
    if (self->is_static) return;
    free(self->priv);
    free(self);
}

filter_t* filter_create_lpf(float cutoff_freq)
{
    filter_t *f = (filter_t *)malloc(sizeof(filter_t));
    if (!f) return NULL;

    lpf_priv_t *p = (lpf_priv_t *)malloc(sizeof(lpf_priv_t));
    if (!p) { free(f); return NULL; }

    p->cutoff_freq = cutoff_freq;
    p->alpha = 0.0f;
    p->pitch = p->roll = p->yaw = 0.0f;
    p->prev_pitch = p->prev_roll = p->prev_yaw = 0.0f;

    f->update    = lpf_update;
    f->reset     = lpf_reset;
    f->set_param = lpf_set_param;
    f->destroy   = lpf_destroy;
    f->type      = FILTER_TYPE_LPF;
    f->degrade   = FILTER_DEGRADE_NONE;
    f->priv      = p;
    f->is_static = 0;

    return f;
}
