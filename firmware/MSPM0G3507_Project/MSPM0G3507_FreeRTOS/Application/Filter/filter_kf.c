/**
 * @file    filter_kf.c
 * @brief   纯卡尔曼滤波器 (KF) 实现
 *
 * 纯卡尔曼滤波器 - 3轴独立 2状态标量 KF
 *
 * 状态向量: 每轴 [angle, bias] (3轴独立)
 *   - axis 0: pitch  (gyro: +gy)
 *   - axis 1: roll   (gyro: -gx)
 *   - axis 2: yaw    (gyro: +gz, 无 ACC 测量)
 *
 * 特点:
 *   - 不需要计算雅可比矩阵 (线性模型)
 *   - 计算量极小 (~30 MAC/帧 vs EKF ~5000)
 *   - 内存极小 (~100 字节 vs EKF ~1.2KB)
 *
 * 与 LKF 的区别:
 *   - LKF 是 6 状态向量 KF (6×6 矩阵)
 *   - KF 是 3 个 2 状态标量 KF (3×2×2 矩阵)
 *   - KF 忽略轴间耦合，计算量更小
 *
 * 参考实现:
 *   [1] Kris Winer MPU-6050 标量 KF 实现
 *   [2] Starlino IMU 指南
 *
 * 适用场景:
 *   - 极低算力场景 (Cortex-M0+)
 *   - 教学用途 (最简 KF 实现)
 *   - 快速原型验证
 *   - 作为 EKF/LKF 的对照基准
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

#if FILTER_ENABLE_KF

/* KF_STATE_SIZE, KF_AXIS_COUNT, KF_HW_S_MIN 已在 filter_internal.h 中定义 */

/* 前向声明 (kf_update 内部调用 kf_reset) */
void kf_reset(filter_t *self);

/**
 * @brief 单轴 KF 预测步骤 (每轴独立)
 * @param p           KF 私有数据
 * @param axis        轴索引 (0=pitch, 1=roll, 2=yaw)
 * @param gyro_rate   该轴陀螺仪角速度 (dps)
 * @param dt          时间间隔 (秒)
 *
 * 预测步骤:
 *   angle' = angle + (gyro - bias) * dt
 *   bias'  = bias
 *   P' = F*P*F' + Q
 *
 * F = [1, -dt;   Q = [Q_angle,  0;
 *      0,   1]          0,       Q_bias]
 */
static inline void kf_predict(kf_priv_t *p, int axis, float gyro_rate, float dt)
{
    /* 状态预测 */
    float angle_pred = p->x[axis][0] + (gyro_rate - p->x[axis][1]) * dt;

    /* 协方差预测: P' = F*P*F' + Q (简化 2×2 展开) */
    float P00 = p->P[axis][0][0];
    float P01 = p->P[axis][0][1];
    float P11 = p->P[axis][1][1];

    float dt2 = dt * dt;
    /* Q 为功率谱密度 (单位 /s)，离散化需乘 dt，与 EKF 实现一致 (filter.c L1037-1040) */
    p->P[axis][0][0] = P00 - 2.0f * dt * P01 + dt2 * P11 + p->Q_angle * dt;
    p->P[axis][0][1] = P01 - dt * P11;
    p->P[axis][1][0] = p->P[axis][0][1];  /* 保持对称 */
    p->P[axis][1][1] = P11 + p->Q_bias * dt;

    /* 协方差正则化 (防止数值发散) */
    /* [Fix-1] 正定性检查: 2x2 矩阵正定 ⇔ P00>0 且 det=P00*P11-P01²>0
     *   P00 > |P01| 是更保守但更简单的不等式 */
    if (p->P[axis][0][0] < 1e-6f) p->P[axis][0][0] = 1e-6f;
    if (p->P[axis][1][1] < 1e-6f) p->P[axis][1][1] = 1e-6f;
    if (p->P[axis][0][0] <= fabsf(p->P[axis][0][1])) {
        /* 协方差退化: 重新膨胀对角线 */
        p->P[axis][0][0] *= 2.0f;
        p->P[axis][1][1] *= 2.0f;
    }
    /* [Fix-2] yaw 轴限制: 无 ACC 测量时, P[0][0] 仅靠 Q 膨胀, 防止指数增长到溢出 */
    if (axis == 2 && p->P[axis][0][0] > 100.0f) p->P[axis][0][0] = 100.0f;

    /* 应用预测状态 */
    p->x[axis][0] = angle_pred;
    /* bias 保持不变 */
}

/**
 * @brief 单轴 KF 更新步骤 (用 ACC 角度测量)
 * @param p           KF 私有数据
 * @param axis        轴索引 (0=pitch, 1=roll)
 * @param acc_angle   加速度计计算的角度 (度)
 *
 * 更新步骤:
 *   K = P*H'/(H*P*H' + R)
 *   x = x + K*(z - H*x)
 *   P = (I - K*H)*P
 *
 * H = [1, 0]  观测矩阵 (直接观测角度)
 */
static inline void kf_update_axis(kf_priv_t *p, int axis, float acc_angle)
{
    /* 创新协方差 S = H*P*H' + R = P[0][0] + R */
    float P00 = p->P[axis][0][0];
    float P01 = p->P[axis][0][1];
    float P10 = p->P[axis][1][0];
    float P11 = p->P[axis][1][1];

    float S = P00 + p->R_measure;
    if (S < KF_HW_S_MIN) S = KF_HW_S_MIN;  /* 防除零 */

    /* 卡尔曼增益 K = P*H'/S */
    float K0 = P00 / S;  /* angle 增益 */
    float K1 = P10 / S;  /* bias  增益 */

    /* 创新量 y = z - H*x = acc_angle - angle */
    float y = acc_angle - p->x[axis][0];

    /* 角度归一化到 [-180, 180], 防止跨边界时创新量突变 360° */
    while (y > 180.0f)  y -= 360.0f;
    while (y < -180.0f) y += 360.0f;

    /* 状态更新: x = x + K*y */
    p->x[axis][0] += K0 * y;
    p->x[axis][1] += K1 * y;

    /* 协方差更新: Joseph 形式 P = (I-KH)P(I-KH)' + K*R*K'
     * 数值稳定, 保证对称正定 (H = [1, 0])
     * (I-KH) = [1-K0, 0; -K1, 1], (I-KH)' = [1-K0, -K1; 0, 1] */
    float a = 1.0f - K0;  /* (1-K0) */
    float b = -K1;        /* -K1 */
    float R = p->R_measure;
    /* T = (I-KH) * P */
    float T00 = a * P00;
    float T01 = a * P01;
    float T10 = b * P00 + P10;
    float T11 = b * P01 + P11;
    /* P = T * (I-KH)' + K*R*K' */
    p->P[axis][0][0] = T00 * a + K0 * R * K0;
    p->P[axis][0][1] = T00 * b + T01 + K0 * R * K1;
    p->P[axis][1][1] = T10 * b + T11 + K1 * R * K1;

    /* 保持对称性 (Joseph 形式理论上对称，FP 舍入误差内强制一致) */
    p->P[axis][1][0] = p->P[axis][0][1];

    /* [Fix-3] Cauchy-Schwarz 约束: |P01| ≤ sqrt(P00 * P11)
     * 保证 2x2 协方差矩阵半正定 (det = P00*P11 - P01² ≥ 0) */
    {
        float cs_bound = sqrtf(fmaxf(0.0f, p->P[axis][0][0] * p->P[axis][1][1]));
        if (fabsf(p->P[axis][0][1]) > cs_bound) {
            p->P[axis][0][1] = (p->P[axis][0][1] > 0.0f ? cs_bound : -cs_bound);
            p->P[axis][1][0] = p->P[axis][0][1];
        }
    }
}

/**
 * @brief ZUPT 伪测量更新 (零速偏置观测)
 * @param p           KF 私有数据
 * @param axis        轴索引 (0=pitch, 1=roll, 2=yaw)
 * @param gyro_rate   该轴陀螺仪角速度 (dps)
 *
 * 静止时陀螺读数 ≈ 偏置 + 噪声, 构造伪测量 z = gyro_rate, H = [0, 1].
 * 创新量 y = gyro_rate - bias → 将偏置推向当前陀螺读数.
 * 协方差更新: P = (I - KH)*P (H = [0, 1] 形式).
 *
 * 轴门控: 创新量超过 KF_ZUPT_AXIS_GATE 时不更新，
 *         防止慢转被误判为静止时偏置跟踪真实角速度).
 * 默认 R_zupt=1e6 等效禁用 (增益趋近零).
 */
static inline void kf_zupt_update_axis(kf_priv_t *p, int axis, float gyro_rate)
{
    float P00 = p->P[axis][0][0];
    float P01 = p->P[axis][0][1];
    float P10 = p->P[axis][1][0];
    float P11 = p->P[axis][1][1];

    /* 创新量 y = z - H*x = gyro_rate - bias */
    float y = gyro_rate - p->x[axis][1];

    /* 轴门控: 创新量过大 → 并非真正静止, 跳过更新 (前置避免无效计算) */
    if (fabsf(y) > KF_ZUPT_AXIS_GATE) return;

    /* 创新协方差 S = H*P*H' + R = P[1][1] + R_zupt (H = [0, 1]) */
    float S = P11 + p->R_zupt;
    if (S < KF_HW_S_MIN) S = KF_HW_S_MIN;

    /* 卡尔曼增益 K = P*H'/S */
    float K0 = P01 / S;  /* angle 增益 (通过交叉协方差) */
    float K1 = P11 / S;  /* bias  增益 */

    /* 状态更新 */
    p->x[axis][0] += K0 * y;
    p->x[axis][1] += K1 * y;

    /* 协方差更新: Joseph 形式 P = (I-KH)P(I-KH)' + K*R*K' (H = [0, 1])
     * (I-KH) = [1, -K0; 0, 1-K1], (I-KH)' = [1, 0; -K0, 1-K1] */
    float c = -K0;
    float d = 1.0f - K1;
    float Rz = p->R_zupt;
    /* T = (I-KH) * P */
    float T00 = P00 + c * P10;
    float T01 = P01 + c * P11;
    float T11 = d * P11;
    /* P = T * (I-KH)' + K*R*K' */
    p->P[axis][0][0] = T00 + T01 * c + K0 * Rz * K0;
    p->P[axis][0][1] = T01 * d + K0 * Rz * K1;
    p->P[axis][1][1] = T11 * d + K1 * Rz * K1;

    /* 保持对称性 */
    p->P[axis][1][0] = p->P[axis][0][1];

    /* [Fix-3] Cauchy-Schwarz 约束 (ZUPT 路径同标准路径) */
    {
        float cs_bound = sqrtf(fmaxf(0.0f, p->P[axis][0][0] * p->P[axis][1][1]));
        if (fabsf(p->P[axis][0][1]) > cs_bound) {
            p->P[axis][0][1] = (p->P[axis][0][1] > 0.0f ? cs_bound : -cs_bound);
            p->P[axis][1][0] = p->P[axis][0][1];
        }
    }
}

#if defined(BSP_MATHACL_KF_HW)
/* ============================================================
 * KF 硬件加速辅助函数（Q24 定点 MAC / DIV）
 * ============================================================ */

/** @brief 协方差 Q24 范围上限，避免 float_to_q24 溢出 */
#define KF_HW_P_CLAMP_MAX   120.0f

/* KF_HW_S_MIN 已在 filter_internal.h 中定义 */

/**
 * @brief 将协方差值钳位到 Q24 安全范围
 */
static inline float kf_p_clamp_hw(float v)
{
    if (v > KF_HW_P_CLAMP_MAX) {
        return KF_HW_P_CLAMP_MAX;
    }
    if (v < -KF_HW_P_CLAMP_MAX) {
        return -KF_HW_P_CLAMP_MAX;
    }
    return v;
}

/**
 * @brief 使用 MATHACL DIV 计算 num/den（Q24 定点）
 *
 * @note 当输入超出 Q24 范围或分母不合法时，回退到软浮点除法。
 */
static inline float kf_div_hw(float num, float den)
{
    if (den <= 0.0f || isnan(num) || isinf(num) ||
        isnan(den) || isinf(den)) {
        return 0.0f;
    }
    if (fabsf(num) > KF_HW_P_CLAMP_MAX || fabsf(den) > KF_HW_P_CLAMP_MAX) {
        return num / den;
    }
    /* 检查输出是否会在 Q24 范围内，避免结果溢出 */
    if (fabsf(den) * KF_HW_P_CLAMP_MAX < fabsf(num)) {
        return num / den;
    }
    int32_t nq = float_to_q24(num);
    int32_t dq = float_to_q24(den);
    int32_t rq = mathacl_div_q24(nq, dq);
    return q24_to_float(rq);
}

/**
 * @brief 单轴 KF 预测步骤（MATHACL 硬件路径）
 *
 * 实测：2~4 项点积的 Q24 转换+MAC 启动开销高于 FPU 乘加，
 * 因此协方差预测保持纯浮点，仅在更新步骤使用硬件除法。
 */
static inline void kf_predict_hw(kf_priv_t *p, int axis,
                                 float gyro_rate, float dt)
{
    kf_predict(p, axis, gyro_rate, dt);
}

/**
 * @brief 单轴 KF 更新步骤（MATHACL 硬件路径）
 */
static inline void kf_update_axis_hw(kf_priv_t *p, int axis, float acc_angle)
{
    float P00 = p->P[axis][0][0];
    float P01 = p->P[axis][0][1];
    float P10 = p->P[axis][1][0];
    float P11 = p->P[axis][1][1];

    float S = P00 + p->R_measure;
    if (S < KF_HW_S_MIN) {
        S = KF_HW_S_MIN;
    }

    /* 卡尔曼增益使用硬件除法 */
    float K0 = kf_div_hw(P00, S);
    float K1 = kf_div_hw(P10, S);

    float y = acc_angle - p->x[axis][0];

    p->x[axis][0] += K0 * y;
    p->x[axis][1] += K1 * y;

    /* 协方差更新保持浮点（避免 Q24 截断累积） */
    p->P[axis][0][0] = (1.0f - K0) * P00;
    p->P[axis][0][1] = (1.0f - K0) * P01;
    p->P[axis][1][0] = P10 - K1 * P00;
    p->P[axis][1][1] = P11 - K1 * P01;

    p->P[axis][1][0] = p->P[axis][0][1];

    /* [Fix-3] Cauchy-Schwarz 约束 (HW 路径) */
    {
        float cs_bound = sqrtf(fmaxf(0.0f, p->P[axis][0][0] * p->P[axis][1][1]));
        if (fabsf(p->P[axis][0][1]) > cs_bound) {
            p->P[axis][0][1] = (p->P[axis][0][1] > 0.0f ? cs_bound : -cs_bound);
            p->P[axis][1][0] = p->P[axis][0][1];
        }
    }
}

#endif /* BSP_MATHACL_KF_HW */

/**
 * @brief 根据 use_hw 标志选择预测实现
 */
static inline void kf_predict_select(kf_priv_t *p, int axis,
                                      float gyro_rate, float dt)
{
#if defined(BSP_MATHACL_KF_HW)
    if (p->use_hw) {
        kf_predict_hw(p, axis, gyro_rate, dt);
        return;
    }
#endif
    kf_predict(p, axis, gyro_rate, dt);
}

/**
 * @brief 根据 use_hw 标志选择更新实现
 */
static inline void kf_update_axis_select(kf_priv_t *p, int axis,
                                          float acc_angle)
{
#if defined(BSP_MATHACL_KF_HW)
    if (p->use_hw) {
        kf_update_axis_hw(p, axis, acc_angle);
        return;
    }
#endif
    kf_update_axis(p, axis, acc_angle);
}

/**
 * @brief KF 主更新函数
 *
 * 状态转移（预测）:
 *   pitch' = pitch + (gy - bias_y) * dt
 *   roll'  = roll  - (gx - bias_x) * dt   (符号约定与 LKF 一致)
 *   yaw'   = yaw   + (gz - bias_z) * dt
 *
 * 测量更新:
 *   pitch: acc_pitch = atan2(-ax, sqrt(ay²+az²))
 *   roll:  acc_roll  = atan2( ay, sqrt(ax²+az²))
 *   yaw:   无测量，仅预测
 */
void kf_update(filter_t *self, const filter_input_t *in, filter_output_t *out)
{
    kf_priv_t *p = (kf_priv_t *)self->priv;
    float dt = in->dt;

    /* === 输入验证 === */
    /* dt 上限 0.1s 对应最低 10Hz 采样率，超过此值视为调度异常 */
    if (dt <= 0.0f || dt > 0.1f ||
        isnan(in->ax) || isinf(in->ax) ||
        isnan(in->ay) || isinf(in->ay) ||
        isnan(in->az) || isinf(in->az) ||
        isnan(in->gx) || isinf(in->gx) ||
        isnan(in->gy) || isinf(in->gy) ||
        isnan(in->gz) || isinf(in->gz)) {
        out->pitch = p->x[0][0];
        out->roll  = p->x[1][0];
        out->yaw   = p->x[2][0];
        out->q0 = out->q1 = out->q2 = out->q3 = 0.0f;
        return;
    }

    /* === 退化模式：HOLD_LAST === */
    if (self->degrade == FILTER_DEGRADE_HOLD_LAST) {
        out->pitch = p->x[0][0];
        out->roll  = p->x[1][0];
        out->yaw   = p->x[2][0];
        out->q0 = out->q1 = out->q2 = out->q3 = 0.0f;
        return;
    }

    /* === 预测步骤：三轴分别预测 === */
    /* pitch (axis 0): +gy */
    kf_predict_select(p, 0, in->gy, dt);
    /* roll  (axis 1): -gx (符号约定与 LKF/EKF 一致) */
    kf_predict_select(p, 1, -in->gx, dt);
    /* yaw   (axis 2): +gz */
    kf_predict_select(p, 2, in->gz, dt);

    /* === 更新步骤：检查退化模式 === */
    if (self->degrade == FILTER_DEGRADE_GYRO_ONLY) {
        /* GYRO_ONLY: 仅预测，不更新 (ACC 不可靠时) */
        /* 状态已更新，保持不变 */
    } else if (self->degrade == FILTER_DEGRADE_ACC_ONLY) {
        /* ACC_ONLY: 直接使用 ACC 角度，重置偏置 */
        float acc_pitch = mathacl_atan2f(-in->ax, mathacl_sqrtf(in->ay * in->ay + in->az * in->az)) * 180.0f / M_PI;
        float acc_roll  = mathacl_atan2f( in->ay, mathacl_sqrtf(in->ax * in->ax + in->az * in->az)) * 180.0f / M_PI;
        p->x[0][0] = acc_pitch;  p->x[0][1] = 0.0f;  /* 重置偏置 */
        p->x[1][0] = acc_roll;   p->x[1][1] = 0.0f;
        /* reset covariance for rapid re-convergence (完整清理非对角项 + yaw轴) */
        p->P[0][0][0] = 1.0f; p->P[0][0][1] = 0.0f; p->P[0][1][0] = 0.0f; p->P[0][1][1] = 1.0f;
        p->P[1][0][0] = 1.0f; p->P[1][0][1] = 0.0f; p->P[1][1][0] = 0.0f; p->P[1][1][1] = 1.0f;
        p->P[2][0][0] = 1.0f; p->P[2][0][1] = 0.0f; p->P[2][1][0] = 0.0f; p->P[2][1][1] = 1.0f;
    } else if (self->degrade == FILTER_DEGRADE_STATIC_ONLY) {
        /* STATIC_ONLY: 正常 ACC 更新 + ZUPT 偏置收敛 (不人工衰减 bias) */
        float acc_pitch = mathacl_atan2f(-in->ax, mathacl_sqrtf(in->ay * in->ay + in->az * in->az)) * 180.0f / M_PI;
        float acc_roll  = mathacl_atan2f( in->ay, mathacl_sqrtf(in->ax * in->ax + in->az * in->az)) * 180.0f / M_PI;
        kf_update_axis_select(p, 0, acc_pitch);
        kf_update_axis_select(p, 1, acc_roll);
        /* ZUPT: 静止时偏置观测 (R_zupt < 1e5 表示已启用)
         * bias 收敛由 ZUPT 自然驱动, 不再人工衰减以保持 KF 最优性 */
        if (p->R_zupt < 1e5f) {
            kf_zupt_update_axis(p, 0, in->gy);
            kf_zupt_update_axis(p, 1, -in->gx);
            kf_zupt_update_axis(p, 2, in->gz);
        }
    } else {
        /* 正常模式: 预测 + 更新 */
        /* 从加速度计计算角度 (仅用于 pitch 和 roll) */
        float ax = in->ax, ay = in->ay, az = in->az;
        float acc_norm = mathacl_sqrtf(ax*ax + ay*ay + az*az);

        if (acc_norm > DEGRADE_ACC_LOW && acc_norm < DEGRADE_ACC_HIGH) {
            /* ACC 幅值正常 → 可信任测量
             * 门限与 filter_check_acc_quality() 对齐 (filter.c L2395-2398)
             * [0.5, 2.0]g 排除自由落体(<0.5g)和碰撞/振动(>2.0g)场景 */
            float acc_pitch = mathacl_atan2f(-ax, mathacl_sqrtf(ay*ay + az*az)) * 180.0f / M_PI;
            float acc_roll  = mathacl_atan2f( ay, mathacl_sqrtf(ax*ax + az*az)) * 180.0f / M_PI;

            /* pitch 更新 */
            kf_update_axis_select(p, 0, acc_pitch);
            /* roll 更新 */
            kf_update_axis_select(p, 1, acc_roll);
        }

        /* ZUPT: 静止时偏置观测 (H=[0,1], 通过陀螺读数修正 bias)
         * R_zupt < 1e5 表示已启用 (1e6=禁用)
         * 静止条件: ACC幅值正常 + 接近1g + 陀螺低幅值 */
        if (p->R_zupt < 1e5f) {
            float gx = in->gx, gy = in->gy, gz = in->gz;
            float gyro_mag = mathacl_sqrtf(gx*gx + gy*gy + gz*gz);
            if (fabsf(acc_norm - 1.0f) < 0.05f && gyro_mag < 2.0f) {
                kf_zupt_update_axis(p, 0, in->gy);
                kf_zupt_update_axis(p, 1, -in->gx);
                kf_zupt_update_axis(p, 2, in->gz);
            }
        }

        /* YAW (axis 2): 无 ACC 测量，仅预测 */
    }

    /* === 角度限幅 === */
    if (p->x[0][0] > p->angle_max) p->x[0][0] = p->angle_max;
    if (p->x[0][0] < p->angle_min) p->x[0][0] = p->angle_min;
    if (p->x[1][0] > p->angle_max) p->x[1][0] = p->angle_max;
    if (p->x[1][0] < p->angle_min) p->x[1][0] = p->angle_min;

    /* YAW wrap-around (仅角度，不影响偏置) */
    while (p->x[2][0] > 180.0f)  p->x[2][0] -= 360.0f;
    while (p->x[2][0] < -180.0f) p->x[2][0] += 360.0f;

    /* 数值异常保护：若状态或协方差发散，重置滤波器 */
    bool kf_healthy = true;
    for (int axis = 0; axis < KF_AXIS_COUNT && kf_healthy; axis++) {
        for (int i = 0; i < KF_STATE_SIZE; i++) {
            if (isnan(p->x[axis][i]) || isinf(p->x[axis][i]) ||
                isnan(p->P[axis][i][0]) || isinf(p->P[axis][i][0]) ||
                isnan(p->P[axis][i][1]) || isinf(p->P[axis][i][1])) {
                kf_healthy = false;
                break;
            }
        }
    }
    if (!kf_healthy) {
        kf_reset(self);
    }

    /* === 输出 === */
    out->pitch = p->x[0][0];
    out->roll  = p->x[1][0];
    out->yaw   = p->x[2][0];
    out->q0 = 1.0f;  /* 单位四元数 (KF 不维护四元数, 设为单位元避免下游 validate 误报) */
    out->q1 = out->q2 = out->q3 = 0.0f;
}

void kf_reset(filter_t *self)
{
    kf_priv_t *p = (kf_priv_t *)self->priv;
    for (int axis = 0; axis < KF_AXIS_COUNT; axis++) {
        p->x[axis][0] = 0.0f;  /* angle = 0 */
        p->x[axis][1] = 0.0f;  /* bias = 0 */
        /* 初始化协方差 */
        p->P[axis][0][0] = 1.0f;  /* 角度不确定性 */
        p->P[axis][0][1] = 0.0f;
        p->P[axis][1][0] = 0.0f;
        p->P[axis][1][1] = 1.0f;  /* 偏置不确定性 */
    }
}

void kf_set_param(filter_t *self, filter_param_t param, float value)
{
    if (validate_filter_param(param, value) != 0) {
        return;  /* 参数无效，拒绝设置 */
    }

    kf_priv_t *p = (kf_priv_t *)self->priv;
    switch (param) {
        case FILTER_PARAM_KF_Q_ANGLE:   p->Q_angle   = value; break;
        case FILTER_PARAM_KF_Q_BIAS:    p->Q_bias    = value; break;
        case FILTER_PARAM_KF_R_MEASURE: p->R_measure = value; break;
        case FILTER_PARAM_KF_R_ZUPT:    p->R_zupt    = value; break;
        default: break;
    }
}

static void kf_destroy(filter_t *self)
{
    if (self->is_static) return;
    free(self->priv);
    free(self);
}

filter_t* filter_create_kf(float q_angle, float q_bias, float r_measure)
{
    filter_t *f = (filter_t *)malloc(sizeof(filter_t));
    if (!f) return NULL;

    kf_priv_t *p = (kf_priv_t *)malloc(sizeof(kf_priv_t));
    if (!p) { free(f); return NULL; }

    /* 初始化状态和协方差 */
    for (int axis = 0; axis < KF_AXIS_COUNT; axis++) {
        p->x[axis][0] = 0.0f;
        p->x[axis][1] = 0.0f;
        p->P[axis][0][0] = 1.0f;
        p->P[axis][0][1] = 0.0f;
        p->P[axis][1][0] = 0.0f;
        p->P[axis][1][1] = 1.0f;
    }
    p->Q_angle    = q_angle;
    p->Q_bias     = q_bias;
    p->R_measure  = r_measure;
    p->R_zupt     = KF_R_ZUPT_DEFAULT;  /* 默认禁用 ZUPT */
    p->angle_min  = -180.0f;
    p->angle_max  =  180.0f;
#if defined(BSP_MATHACL_KF_HW)
    p->use_hw     = 1;
#else
    p->use_hw     = 0;
#endif

    f->update    = kf_update;
    f->reset     = kf_reset;
    f->set_param = kf_set_param;
    f->destroy   = kf_destroy;
    f->type      = FILTER_TYPE_KF;
    f->degrade   = FILTER_DEGRADE_NONE;
    f->priv      = p;
    f->is_static = 0;

    return f;
}
#endif /* FILTER_ENABLE_KF */
