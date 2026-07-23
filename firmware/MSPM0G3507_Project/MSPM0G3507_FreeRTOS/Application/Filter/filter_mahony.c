/**
 * @file    filter_mahony.c
 * @brief   自适应非线性互补滤波器 (Adaptive Mahony)
 *
 * 基于物理约束的自适应增益策略:
 *   1. ACC 门控: |norm(acc)-1g| > acc_gate → 跳过修正, 纯陀螺积分
 *   2. 陀螺饱和: |gyro| > 0.85*FS → 降低 kp
 *   3. 自适应 kp: 静态时增大, 动态时缩小, 范围 [0.1, 1.5]*kp_base
 *   4. 自适应 ki: 静态时激活偏置估计, 动态时指数衰减
 *   5. Yaw 解耦: ACC 仅修正 pitch/roll 误差分量
 *   6. 偏置限幅: |bias| <= 20dps (LSM6DSR 规格)
 *   7. 积分抗饱和: clamp ±MAHONY_INTEGRAL_LIMIT rad/s
 *
 * 参考: Mahony et al., 2008, "Nonlinear complementary filters"
 */

#include "filter_internal.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

#if FILTER_ENABLE_MAHONY

/* ---- 本地常量 (复用 filter_config.h 宏) ---- */

void mahony_update(filter_t *self, const filter_input_t *in, filter_output_t *out)
{
    mahony_priv_t *p = (mahony_priv_t *)self->priv;
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

    /* === 2. 退化模式检查 === */
    if (self->degrade == FILTER_DEGRADE_HOLD_LAST) {
        filter_quat_to_euler(p->q0, p->q1, p->q2, p->q3, out);
        return;
    }

    if (self->degrade == FILTER_DEGRADE_GYRO_ONLY) {
        float gx_r = in->gx * M_PI / 180.0f + p->ix;
        float gy_r = in->gy * M_PI / 180.0f + p->iy;
        float gz_r = in->gz * M_PI / 180.0f + p->iz;
        filter_quat_integrate_gyro(&p->q0, &p->q1, &p->q2, &p->q3,
                                   gx_r + p->ix, gy_r + p->iy,
                                   gz_r + p->iz, dt);
        filter_quat_to_euler(p->q0, p->q1, p->q2, p->q3, out);
        return;
    }

    if (self->degrade == FILTER_DEGRADE_ACC_ONLY) {
        filter_acc_to_quat(in->ax, in->ay, in->az,
                           &p->q0, &p->q1, &p->q2, &p->q3);
        filter_quat_to_euler(p->q0, p->q1, p->q2, p->q3, out);
        return;
    }

    /* === 3. 传感器质量评估 === */
    float ax = in->ax, ay = in->ay, az = in->az;
    float gx_r = in->gx * M_PI / 180.0f;
    float gy_r = in->gy * M_PI / 180.0f;
    float gz_r = in->gz * M_PI / 180.0f;

    float acc_norm = sqrtf(ax * ax + ay * ay + az * az);
    float acc_dev  = fabsf(acc_norm - 1.0f);
    float gyro_mag_dps = sqrtf(in->gx*in->gx + in->gy*in->gy + in->gz*in->gz);

    /* ACC 质量: 0=完全不可信, 1=完美重力参考 */
    float acc_quality;
    if (acc_dev < p->static_acc_th)
        acc_quality = 1.0f;  /* 静态，完美信任 */
    else if (acc_dev < 0.1f)
        acc_quality = 1.0f - (acc_dev - p->static_acc_th) / (0.1f - p->static_acc_th);
    else
        acc_quality = 0.0f;  /* 强机动，不信任 */

    /* GYRO 饱和因子: 1=正常, 0=饱和 */
    float gyro_sat = 1.0f;
    if (gyro_mag_dps > p->gyro_sat_thresh)
        gyro_sat = (gyro_mag_dps < 250.0f) ? (250.0f - gyro_mag_dps) / (250.0f - p->gyro_sat_thresh) : 0.0f;
    if (gyro_sat < 0.1f) gyro_sat = 0.1f;  /* 最低限度信任 */

    /* === 4. 自适应增益计算 === */

    /* 4a. 检测静态 */
    int is_static = (gyro_mag_dps < p->static_gyro_th && acc_dev < p->static_acc_th);

    /* 4b. kp 自适应 */
    if (is_static)
        p->kp_eff = p->kp_base * MAHONY_KP_STATIC_BOOST;
    else
        p->kp_eff = p->kp_base * MAHONY_KP_DYN_ACC_FACTOR * fmaxf(acc_quality, 0.2f) * gyro_sat;

    /* 钳位到安全范围 */
    if (p->kp_eff > p->kp_base * MAHONY_KP_STATIC_BOOST) p->kp_eff = p->kp_base * MAHONY_KP_STATIC_BOOST;
    if (p->kp_eff < p->kp_base * 0.1f) p->kp_eff = p->kp_base * 0.1f;

    /* 4c. ki 自适应 */
    float ki_eff;
    if (is_static)
        ki_eff = p->ki_base;  /* 静态: 全速偏置估计 */
    else if (gyro_mag_dps < 5.0f)
        ki_eff = p->ki_base * 0.5f;  /* 准静态: 减速 */
    else if (gyro_mag_dps < 20.0f)
        ki_eff = p->ki_base * MAHONY_KI_DYN1_FACTOR;
    else
        ki_eff = p->ki_base * MAHONY_KI_DYN2_FACTOR;

    /* ACC 质量差时额外减速 */
    if (acc_quality < 0.3f) ki_eff *= MAHONY_KI_ACC_POOR;

    p->ki_eff = ki_eff;

    /* === 5. ACC 门控: 判定是否使用加速度计修正 === */
    int use_acc = (acc_norm > p->acc_gate_low && acc_norm < p->acc_gate_high);
    if (!use_acc) {
        /* ACC 不可用, 纯陀螺积分 */
        filter_quat_integrate_gyro(&p->q0, &p->q1, &p->q2, &p->q3,
                                   gx_r, gy_r, gz_r, dt);
        filter_quat_to_euler(p->q0, p->q1, p->q2, p->q3, out);
        return;
    }

    /* === 6. Mahony 标准流程 === */
    ax /= acc_norm; ay /= acc_norm; az /= acc_norm;

    /* 估计重力方向 */
    float vx = 2.0f * (p->q1 * p->q3 - p->q0 * p->q2);
    float vy = 2.0f * (p->q0 * p->q1 + p->q2 * p->q3);
    float vz = 1.0f - 2.0f * (p->q1 * p->q1 + p->q2 * p->q2);

    /* 误差（叉积） */
    float ex = (ay * vz - az * vy);
    float ey = (az * vx - ax * vz);
    float ez = (ax * vy - ay * vx);

    /* === 6a. Yaw 解耦: 将误差投影到 pitch/roll 平面 ===
     * 无磁力计时, ACC 无法观测 yaw 误差.
     * 将总误差向量分解为"可观测(pitch/roll)"和"不可观测(yaw)"分量,
     * 仅用可观测分量修正. */
    {
        /* 世界坐标系的 Z 轴(重力方向) 在 IMU 坐标系中的表示 = v */
        /* yaw 旋转轴 = v (绕重力方向的旋转不能被 ACC 观测) */
        float v_norm = sqrtf(vx*vx + vy*vy + vz*vz);
        if (v_norm > 0.01f) {
            float ux = vx / v_norm, uy = vy / v_norm, uz = vz / v_norm;
            /* 误差在重力轴上的投影分量 = yaw 不可观测部分 */
            float proj = ex*ux + ey*uy + ez*uz;
            /* 减去投影分量, 保留 pitch/roll 可观测部分 */
            ex -= proj * ux;
            ey -= proj * uy;
            ez -= proj * uz;
        }
    }

    /* === 6b. PI 控制器 (使用有效增益) === */
    if (ki_eff > 0.0f) {
        p->ix += ki_eff * ex * dt;
        p->iy += ki_eff * ey * dt;
        p->iz += ki_eff * ez * dt;

        /* 积分抗饱和限幅 */
        float il = MAHONY_INTEGRAL_LIMIT;
        if (p->ix >  il) p->ix =  il; else if (p->ix < -il) p->ix = -il;
        if (p->iy >  il) p->iy =  il; else if (p->iy < -il) p->iy = -il;
        if (p->iz >  il) p->iz =  il; else if (p->iz < -il) p->iz = -il;

        /* PI 积分量是对陀螺的修正量，物理偏置与其符号相反。 */
        p->bias_x = -p->ix * 180.0f / M_PI;
        p->bias_y = -p->iy * 180.0f / M_PI;
        p->bias_z = -p->iz * 180.0f / M_PI;

        /* 物理偏置限幅 */
        float bl = p->bias_limit;
        if (p->bias_x >  bl) { p->bias_x =  bl; p->ix = -bl * M_PI / 180.0f; }
        else if (p->bias_x < -bl) { p->bias_x = -bl; p->ix = bl * M_PI / 180.0f; }
        if (p->bias_y >  bl) { p->bias_y =  bl; p->iy = -bl * M_PI / 180.0f; }
        else if (p->bias_y < -bl) { p->bias_y = -bl; p->iy = bl * M_PI / 180.0f; }
        if (p->bias_z >  bl) { p->bias_z =  bl; p->iz = -bl * M_PI / 180.0f; }
        else if (p->bias_z < -bl) { p->bias_z = -bl; p->iz = bl * M_PI / 180.0f; }
    }

    /* 已学习的偏置修正在 ki 运行时置零后仍应保持生效。 */
    gx_r += p->ix;
    gy_r += p->iy;
    gz_r += p->iz;

    gx_r += p->kp_eff * ex;
    gy_r += p->kp_eff * ey;
    gz_r += p->kp_eff * ez;

    /* === 7. 四元数积分 === */
    filter_quat_integrate_gyro(&p->q0, &p->q1, &p->q2, &p->q3,
                               gx_r, gy_r, gz_r, dt);

    /* === 8. 输出 === */
    filter_quat_to_euler(p->q0, p->q1, p->q2, p->q3, out);
}

void mahony_reset(filter_t *self)
{
    mahony_priv_t *p = (mahony_priv_t *)self->priv;
    p->q0 = 1.0f; p->q1 = p->q2 = p->q3 = 0.0f;
    p->ix = p->iy = p->iz = 0.0f;
    p->bias_x = p->bias_y = p->bias_z = 0.0f;
}

void mahony_set_param(filter_t *self, filter_param_t param, float value)
{
    if (validate_filter_param(param, value) != 0) return;
    mahony_priv_t *p = (mahony_priv_t *)self->priv;
    if (param == FILTER_PARAM_KP) { p->kp_base = value; p->kp_eff = value; }
    if (param == FILTER_PARAM_KI) { p->ki_base = value; p->ki_eff = value; }
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

    p->kp_base = kp; p->kp_eff = kp;
    p->ki_base = ki; p->ki_eff = ki;
    p->q0 = 1.0f; p->q1 = p->q2 = p->q3 = 0.0f;
    p->ix = p->iy = p->iz = 0.0f;
    p->bias_x = p->bias_y = p->bias_z = 0.0f;
    p->acc_gate_low  = MAHONY_ACC_GATE_LOW;
    p->acc_gate_high = MAHONY_ACC_GATE_HIGH;
    p->gyro_sat_thresh = MAHONY_GYRO_SAT_TH;
    p->bias_limit = MAHONY_BIAS_LIMIT;
    p->static_gyro_th = MAHONY_STATIC_GYRO_TH;
    p->static_acc_th  = MAHONY_STATIC_ACC_TH;

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
#endif /* FILTER_ENABLE_MAHONY */
