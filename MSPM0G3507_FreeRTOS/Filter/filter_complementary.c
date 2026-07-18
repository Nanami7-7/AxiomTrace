/**
 * @file    filter_complementary.c
 * @brief   互补滤波器 (Complementary Filter) 实现
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
 * 1. 互补滤波器 (Complementary Filter)
 * ============================================================ */

void complementary_update(filter_t *self, const filter_input_t *in, filter_output_t *out)
{
    complementary_priv_t *p = (complementary_priv_t *)self->priv;
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
        out->q0 = out->q1 = out->q2 = out->q3 = 0.0f;
        return;
    }

    /* 退化模式检查 */
    if (self->degrade == FILTER_DEGRADE_HOLD_LAST) {
        /* HOLD_LAST: 直接返回上次输出，不更新状态 */
        out->pitch = p->pitch;
        out->roll  = p->roll;
        out->yaw   = p->yaw;
        out->q0 = out->q1 = out->q2 = out->q3 = 0.0f;
        return;
    }

    /* 计算 ACC 姿态角 */
    float sqrt_ay_az = mathacl_sqrtf(in->ay * in->ay + in->az * in->az);
    float sqrt_ax_az = mathacl_sqrtf(in->ax * in->ax + in->az * in->az);
    float acc_pitch = mathacl_atan2f(-in->ax, sqrt_ay_az) * 180.0f / M_PI;
    float acc_roll  = mathacl_atan2f( in->ay, sqrt_ax_az) * 180.0f / M_PI;

    /* DEBUG: 每200次输出一次中间值 */
#if FILTER_DEBUG_VERBOSE
    static uint32_t _dbg_cnt = 0;
    if ((_dbg_cnt++ % 200) == 0) {
        printf("[FILT] ax=%.3f ay=%.3f az=%.3f sq1=%.4f sq2=%.4f ap=%.2f ar=%.2f p=%.2f r=%.2f\r\n",
               (double)in->ax, (double)in->ay, (double)in->az,
               (double)sqrt_ay_az, (double)sqrt_ax_az,
               (double)acc_pitch, (double)acc_roll,
               (double)p->pitch, (double)p->roll);
    }
#endif

    /* 退化模式：陀螺仪仅积分（跳过ACC修正） */
    if (self->degrade == FILTER_DEGRADE_GYRO_ONLY) {
        p->pitch += in->gy * dt;
        p->roll  -= in->gx * dt;
        p->yaw   += in->gz * dt;
    } else if (self->degrade == FILTER_DEGRADE_ACC_ONLY) {
        /* 退化模式：仅加速度计（跳过GYRO积分） */
        p->pitch = acc_pitch;
        p->roll  = acc_roll;
    } else {
        /* 正常模式：互补滤波融合 */
        p->pitch = p->alpha * (p->pitch + in->gy * dt) + (1.0f - p->alpha) * acc_pitch;
        p->roll  = p->alpha * (p->roll  - in->gx * dt) + (1.0f - p->alpha) * acc_roll;
        p->yaw  += in->gz * dt;
    }

    /* Yaw wrap-around */
    while (p->yaw > 180.0f)  p->yaw -= 360.0f;
    while (p->yaw < -180.0f) p->yaw += 360.0f;

    /* 输出 */
    out->pitch = p->pitch;
    out->roll  = p->roll;
    out->yaw   = p->yaw;
    out->q0 = out->q1 = out->q2 = out->q3 = 0.0f;
}

void complementary_reset(filter_t *self)
{
    complementary_priv_t *p = (complementary_priv_t *)self->priv;
    p->pitch = p->roll = p->yaw = 0.0f;
}

void complementary_set_param(filter_t *self, filter_param_t param, float value)
{
    if (validate_filter_param(param, value) != 0) {
        return;  /* 参数无效，拒绝设置 */
    }

    complementary_priv_t *p = (complementary_priv_t *)self->priv;
    if (param == FILTER_PARAM_ALPHA) {
        p->alpha = value;
    }
}

static void complementary_destroy(filter_t *self)
{
    if (self->is_static) return;
    free(self->priv);
    free(self);
}

filter_t* filter_create_complementary(float alpha)
{
    filter_t *f = (filter_t *)malloc(sizeof(filter_t));
    if (!f) return NULL;

    complementary_priv_t *p = (complementary_priv_t *)malloc(sizeof(complementary_priv_t));
    if (!p) { free(f); return NULL; }

    p->alpha = alpha;
    p->pitch = p->roll = p->yaw = 0.0f;

    f->update    = complementary_update;
    f->reset     = complementary_reset;
    f->set_param = complementary_set_param;
    f->destroy   = complementary_destroy;
    f->type      = FILTER_TYPE_COMPLEMENTARY;
    f->degrade   = FILTER_DEGRADE_NONE;
    f->priv      = p;
    f->is_static = 0;

    return f;
}
