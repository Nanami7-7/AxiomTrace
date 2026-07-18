/**
 * @file    filter_mahony.c
 * @brief   Mahony 滤波器实现
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
 * 4. Mahony 滤波器
 * ============================================================ */

void mahony_update(filter_t *self, const filter_input_t *in, filter_output_t *out)
{
    mahony_priv_t *p = (mahony_priv_t *)self->priv;
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
        /* 仅陀螺仪：跳过ACC修正，只做四元数积分 */
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
    float gx = in->gx * M_PI / 180.0f;  /* 转换为 rad/s */
    float gy = in->gy * M_PI / 180.0f;
    float gz = in->gz * M_PI / 180.0f;

    /* 归一化加速度 */
    float norm = mathacl_sqrtf(ax * ax + ay * ay + az * az);
    if (norm > 0.0f) {
        ax /= norm; ay /= norm; az /= norm;
    }

    /* 估计重力方向 */
    float vx = 2.0f * (p->q1 * p->q3 - p->q0 * p->q2);
    float vy = 2.0f * (p->q0 * p->q1 + p->q2 * p->q3);
    float vz = 1.0f - 2.0f * (p->q1 * p->q1 + p->q2 * p->q2);

    /* 误差（叉积） */
    float ex = (ay * vz - az * vy);
    float ey = (az * vx - ax * vz);
    float ez = (ax * vy - ay * vx);

    /* PI 控制器 */
    if (p->ki > 0.0f) {
        p->ix += p->ki * ex * dt;
        p->iy += p->ki * ey * dt;
        p->iz += p->ki * ez * dt;
        /* 积分抗饱和限幅 */
        #define INTEGRAL_LIMIT 0.5f  /* 约 28.6°/s */
        p->ix = fmaxf(-INTEGRAL_LIMIT, fminf(INTEGRAL_LIMIT, p->ix));
        p->iy = fmaxf(-INTEGRAL_LIMIT, fminf(INTEGRAL_LIMIT, p->iy));
        p->iz = fmaxf(-INTEGRAL_LIMIT, fminf(INTEGRAL_LIMIT, p->iz));
        #undef INTEGRAL_LIMIT
        gx += p->ix;
        gy += p->iy;
        gz += p->iz;
    }

    gx += p->kp * ex;
    gy += p->kp * ey;
    gz += p->kp * ez;

    /* 四元数积分 */
    filter_quat_integrate_gyro(&p->q0, &p->q1, &p->q2, &p->q3, gx, gy, gz, dt);

    /* 输出 */
    filter_quat_to_euler(p->q0, p->q1, p->q2, p->q3, out);
}

void mahony_reset(filter_t *self)
{
    mahony_priv_t *p = (mahony_priv_t *)self->priv;
    p->q0 = 1.0f; p->q1 = p->q2 = p->q3 = 0.0f;
    p->ix = p->iy = p->iz = 0.0f;
}

void mahony_set_param(filter_t *self, filter_param_t param, float value)
{
    if (validate_filter_param(param, value) != 0) {
        return;  /* 参数无效，拒绝设置 */
    }

    mahony_priv_t *p = (mahony_priv_t *)self->priv;
    if (param == FILTER_PARAM_KP) p->kp = value;
    if (param == FILTER_PARAM_KI) p->ki = value;
}

static void mahony_destroy(filter_t *self)
{
    if (self->is_static) return;
    free(self->priv);
    free(self);
}

filter_t* filter_create_mahony(float kp, float ki)
{
    filter_t *f = (filter_t *)malloc(sizeof(filter_t));
    if (!f) return NULL;

    mahony_priv_t *p = (mahony_priv_t *)malloc(sizeof(mahony_priv_t));
    if (!p) { free(f); return NULL; }

    p->kp = kp;
    p->ki = ki;
    p->q0 = 1.0f; p->q1 = p->q2 = p->q3 = 0.0f;
    p->ix = p->iy = p->iz = 0.0f;

    f->update    = mahony_update;
    f->reset     = mahony_reset;
    f->set_param = mahony_set_param;
    f->destroy   = mahony_destroy;
    f->type      = FILTER_TYPE_MAHONY;
    f->degrade   = FILTER_DEGRADE_NONE;
    f->priv      = p;
    f->is_static = 0;

    return f;
}
