/**
 * @file    filter_ekf.c
 * @brief   扩展卡尔曼滤波器 (EKF) 实现
 *
 * 状态向量: [q0, q1, q2, q3, bias_x, bias_y, bias_z]
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

/* EKF 数值安全阈值 */
#define EKF_COV_MAX_LIMIT       1e6f
#define EKF_COV_MIN_LIMIT       1e-10f
#define EKF_INV_DET_EPS         1e-12f
/* EKF_NORM_EPS 已在 filter_internal.h 中定义 */

/* EKF偏置限制 (dps) - 使用 filter_config.h 中的 EKF_BIAS_LIMIT_DEFAULT */

/* 矩阵操作辅助函数 */

void ekf_reset(filter_t *self);

/**
 * @brief 检查数组中是否存在 NaN 或 Inf
 */
static inline bool ekf_has_nan_inf(const float *v, int n)
{
    for (int i = 0; i < n; i++) {
        if (isnan(v[i]) || isinf(v[i])) {
            return true;
        }
    }
    return false;
}

/**
 * @brief 检查 7x7 协方差矩阵是否存在 NaN/Inf 或严重发散
 */
static inline bool ekf_covariance_healthy(const float P[7][7])
{
    for (int i = 0; i < 7; i++) {
        for (int j = 0; j < 7; j++) {
            if (isnan(P[i][j]) || isinf(P[i][j])) {
                return false;
            }
        }
        if (P[i][i] < -EKF_COV_MAX_LIMIT || P[i][i] > EKF_COV_MAX_LIMIT) {
            return false;
        }
    }
    return true;
}

/**
 * @brief 将四元数归一化并检查有效性
 */
static inline void ekf_normalize_quaternion(float *q0, float *q1, float *q2, float *q3)
{
    float norm = mathacl_sqrtf((*q0) * (*q0) + (*q1) * (*q1) + (*q2) * (*q2) + (*q3) * (*q3));
    if (norm > EKF_NORM_EPS) {
        *q0 /= norm; *q1 /= norm; *q2 /= norm; *q3 /= norm;
    } else {
        *q0 = 1.0f; *q1 = 0.0f; *q2 = 0.0f; *q3 = 0.0f;
    }
}

/**
 * @brief 从 EKF 状态计算欧拉角输出
 */
static inline void ekf_state_to_output(const float *state, filter_output_t *out)
{
    filter_quat_to_euler(state[0], state[1], state[2], state[3], out);
}

#if defined(BSP_MATHACL_EKF_HW)
/* ============================================================
 * EKF 硬件加速辅助函数（Q24 定点除法）
 * ============================================================ */

/** @brief EKF 除法 Q24 范围上限，避免 float_to_q24 溢出 */
#define EKF_HW_DIV_CLAMP_MAX   120.0f

/**
 * @brief 使用 MATHACL DIV 计算 num/den（Q24 定点）
 *
 * @note 当输入超出 Q24 范围或分母不合法时，回退到软浮点除法。
 */
static inline float ekf_div_hw(float num, float den)
{
    if (den <= 0.0f || isnan(num) || isinf(num) ||
        isnan(den) || isinf(den)) {
        return 0.0f;
    }
    if (fabsf(num) > EKF_HW_DIV_CLAMP_MAX || fabsf(den) > EKF_HW_DIV_CLAMP_MAX) {
        return num / den;
    }
    /* 检查输出是否会在 Q24 范围内，避免结果溢出 */
    if (fabsf(den) * EKF_HW_DIV_CLAMP_MAX < fabsf(num)) {
        return num / den;
    }
    int32_t nq = float_to_q24(num);
    int32_t dq = float_to_q24(den);
    int32_t rq = mathacl_div_q24(nq, dq);
    return q24_to_float(rq);
}

#else

/**
 * @brief 未启用 MATHACL 时的软浮点占位函数（保持代码对称）
 */
static inline float ekf_div_hw(float num, float den)
{
    (void)num;
    (void)den;
    return 0.0f;
}

#endif /* BSP_MATHACL_EKF_HW */


/* ============================================================
 * EKF 实例说明
 * ============================================================
 * @note ekf_update 使用 ekf_priv_t 内嵌的工作缓冲区，
 *       每个实例独立，支持多任务并发调用（只要实例不同）。
 */

void ekf_update(filter_t *self, const filter_input_t *in, filter_output_t *out)
{
    ekf_priv_t *p = (ekf_priv_t *)self->priv;
    float dt = in->dt;

    /* 输入验证：检查所有 6 轴传感器数据以及 dt 有效性 */
    if (dt <= 0.0f || dt > 1.0f ||
        isnan(in->ax) || isinf(in->ax) ||
        isnan(in->ay) || isinf(in->ay) ||
        isnan(in->az) || isinf(in->az) ||
        isnan(in->gx) || isinf(in->gx) ||
        isnan(in->gy) || isinf(in->gy) ||
        isnan(in->gz) || isinf(in->gz)) {
        /* 输出上次状态 */
        ekf_state_to_output(p->state, out);
        return;
    }

    /* 退化模式检查 */
    if (self->degrade == FILTER_DEGRADE_HOLD_LAST) {
        ekf_state_to_output(p->state, out);
        return;
    }

    if (self->degrade == FILTER_DEGRADE_ACC_ONLY) {
        /* 仅加速度计：从ACC计算姿态角并重置四元数 */
        filter_acc_to_quat(in->ax, in->ay, in->az,
                           &p->state[0], &p->state[1], &p->state[2], &p->state[3]);
        /* 偏置保持不变 */
        ekf_state_to_output(p->state, out);
        return;
    }

    /* 提取状态 */
    float q0 = p->state[0], q1 = p->state[1], q2 = p->state[2], q3 = p->state[3];
    float bx = p->state[4], by = p->state[5], bz = p->state[6];

    /* 偏置补偿并转换为 rad/s */
    float gx = (in->gx - bx) * M_PI / 180.0f;
    float gy = (in->gy - by) * M_PI / 180.0f;
    float gz = (in->gz - bz) * M_PI / 180.0f;

#if EKF_GYRO_RESIDUAL_ENABLE
    /* ===== 陀螺仪残留抑制 v2 (maneuver_level 状态转换触发) =====
     * 根因: LSM6DSR 高速转动后突然停止, 陀螺仪机械振荡残留持续输出
     *       非零角速度 (5-10 dps, 1-3 秒), 但加速度计已表明静止.
     *       EKF 盲信陀螺仪继续积分 yaw, 导致 yaw 反向漂移.
     *
     * [v1 缺陷] 原 "acc静止 + gyro非零" 双检测无法区分:
     *   - 陀螺仪残留 (停转后): acc≈1g, gyro=5-10dps
     *   - 原地旋转 (真实转动): acc≈1g (无线性加速度!), gyro=12-38dps
     *   → v1 误杀原地旋转, yaw 仅增长 -1.443° (应 ~90°)
     *
     * [v2 修复] 改用 maneuver_level 状态转换触发:
     *   - 真实转动: level 持续 ≥2 (缓动态/高动态), 不触发抑制
     *   - 停转残留: level 从 ≥2 降到 ≤1, 且 gyro 仍非零 → 触发窗口
     *
     * 时序说明:
     *   - 此处 p->maneuver_level 为上一帧值 (L1049 在状态预测后设置)
     *   - 本地计算 curr_level 供转换检测使用 */
    {
        float acc_norm_local = mathacl_sqrtf(in->ax * in->ax +
                                            in->ay * in->ay +
                                            in->az * in->az);
        float gyro_mag_raw = mathacl_sqrtf(in->gx * in->gx +
                                          in->gy * in->gy +
                                          in->gz * in->gz);
        float acc_err_local = fabsf(acc_norm_local - 1.0f);

        /* 本地计算当前帧 maneuver_level (与 L1049 检测逻辑一致)
         * p->maneuver_level 此时为上一帧值, 用于滞回判断 */
        uint8_t curr_level;
        if (gyro_mag_raw < EKF_MANEUVER_GYRO_STATIC && acc_err_local < EKF_MANEUVER_ACC_STATIC) {
            curr_level = 0;  /* 静止 */
        } else if (p->maneuver_level == 0 &&
                   gyro_mag_raw < EKF_MANEUVER_GYRO_STATIC_EXIT &&
                   acc_err_local < EKF_MANEUVER_ACC_STATIC) {
            curr_level = 0;  /* 滞回保持静止 */
        } else if (gyro_mag_raw < EKF_MANEUVER_GYRO_QUASI && acc_err_local < EKF_MANEUVER_ACC_QUASI) {
            curr_level = 1;  /* 准静态 */
        } else if (gyro_mag_raw < EKF_MANEUVER_GYRO_SLOW && acc_err_local < EKF_MANEUVER_ACC_SLOW) {
            curr_level = 2;  /* 缓动态 */
        } else {
            curr_level = 3;  /* 高动态 */
        }

        /* 检测 "急停" 转换: 上一帧高动态(≥3, >50dps) → 当前帧低动态(≤1) + 陀螺残留
         * 慢转(~10dps)不触发, 避免 level 1↔2 振荡导致 yaw 跟随卡顿 */
        int transition_detected = (p->maneuver_level >= 3 && curr_level <= 1 &&
                                    gyro_mag_raw > EKF_GYRO_RESIDUAL_GYRO_TH);

        if (transition_detected) {
            /* 启动残留抑制窗口 (第一帧不衰减, 从第二帧开始衰减) */
            p->residual_active_frames = 1;
            p->residual_decay_factor = 1.0f;
        }

        if (p->residual_active_frames > 0) {
            if (gyro_mag_raw > EKF_GYRO_RESIDUAL_GYRO_TH &&
                p->residual_active_frames <= EKF_GYRO_RESIDUAL_WINDOW) {
                /* 残留仍存在且窗口未超时, 施加指数衰减 */
                float decay = expf(-(float)p->residual_active_frames / EKF_GYRO_RESIDUAL_DECAY_TAU);
                p->residual_decay_factor = decay;
                gx *= decay;
                gy *= decay;
                gz *= decay;
                p->residual_active_frames++;
            } else {
                /* 残留消散 (gyro<阈值) 或窗口超时, 结束抑制 */
                p->residual_active_frames = 0;
                p->residual_decay_factor = 1.0f;
            }
        }
    }
#endif /* EKF_GYRO_RESIDUAL_ENABLE */

    /* ===== 1. 状态预测 (一阶 Euler 积分) =====
     * q_new = q_old + q_dot * dt
     * q_dot = 0.5 * q ⊗ ω
     * 截断误差 O(ω²·dt²), 与协方差预测 F 矩阵(一阶雅可比)数学一致
     *
     * [回滚原因] 中点法(二阶)虽精度更高, 但 F 矩阵仍为一阶 Euler 雅可比,
     * 二者数学不一致导致 P 矩阵不能正确反映预测不确定性, 引发数值不稳定
     * (max_jump=1.6167°, std=1.7758°)。回滚保证状态预测与协方差预测一致性。 */
    float q0_dot = 0.5f * (-q1 * gx - q2 * gy - q3 * gz);
    float q1_dot = 0.5f * ( q0 * gx + q2 * gz - q3 * gy);
    float q2_dot = 0.5f * ( q0 * gy - q1 * gz + q3 * gx);
    float q3_dot = 0.5f * ( q0 * gz + q1 * gy - q2 * gx);

    p->state[0] += q0_dot * dt;
    p->state[1] += q1_dot * dt;
    p->state[2] += q2_dot * dt;
    p->state[3] += q3_dot * dt;

    /* 四元数归一化
     * [方案 H2] 条件归一化: 仅在范数偏离1.0超过阈值时归一化
     * 多数帧范数接近1, 可省~80%归一化计算
     * 每 EKF_NORM_FORCE_INTERVAL 帧强制归一化一次, 防止长期漂移 */
#if EKF_COND_NORMALIZE
    {
        float norm_sq = p->state[0]*p->state[0] + p->state[1]*p->state[1]
                      + p->state[2]*p->state[2] + p->state[3]*p->state[3];
        if (fabsf(norm_sq - 1.0f) > EKF_NORM_TOL
            || ++p->norm_skip_count >= EKF_NORM_FORCE_INTERVAL) {
            ekf_normalize_quaternion(&p->state[0], &p->state[1],
                                     &p->state[2], &p->state[3]);
            p->norm_skip_count = 0;
        }
    }
#else
    ekf_normalize_quaternion(&p->state[0], &p->state[1], &p->state[2], &p->state[3]);
#endif

    /* ===== 2. 协方差预测 P = F * P * F^T + Q ===== */
    /* F 矩阵 (7x7)：四元数部分有非对角元素 */
    /* F_qq = I + 0.5*dt*Omega(gx,gy,gz) */
    /* F_qb = -0.5*dt*Xi(q) */
    /* F_bq = 0, F_bb = I */

    /* 临时存储 P 的副本用于计算 F*P*F^T */
    float (*P_new)[7] = p->P_new;
    memset(P_new, 0, sizeof(p->P_new));

    /* F 矩阵元素（预计算） */
    float dt_half = 0.5f * dt;
    /* F_qq = I + 0.5*dt*Omega */
    /* Omega = [0, -gx, -gy, -gz; gx, 0, gz, -gy; gy, -gz, 0, gx; gz, gy, -gx, 0] */
    float F00 = 1.0f, F01 = -dt_half*gx, F02 = -dt_half*gy, F03 = -dt_half*gz;
    float F10 = dt_half*gx, F11 = 1.0f, F12 = dt_half*gz, F13 = -dt_half*gy;
    float F20 = dt_half*gy, F21 = -dt_half*gz, F22 = 1.0f, F23 = dt_half*gx;
    float F30 = dt_half*gz, F31 = dt_half*gy, F32 = -dt_half*gx, F33 = 1.0f;

    /* F_qb = -0.5*dt*Xi(q)
     * Xi(q) = [[-q1, -q2, -q3],
     *          [ q0, -q3,  q2],
     *          [ q3,  q0, -q1],
     *          [-q2,  q1,  q0]]
     * 该耦合项反映陀螺仪偏置误差对四元数预测的影响，数学上不可忽略。
     */
    float F_qb[4][3];
    F_qb[0][0] = -dt_half * (-q1); F_qb[0][1] = -dt_half * (-q2); F_qb[0][2] = -dt_half * (-q3);
    F_qb[1][0] = -dt_half * ( q0); F_qb[1][1] = -dt_half * (-q3); F_qb[1][2] = -dt_half * ( q2);
    F_qb[2][0] = -dt_half * ( q3); F_qb[2][1] = -dt_half * ( q0); F_qb[2][2] = -dt_half * (-q1);
    F_qb[3][0] = -dt_half * (-q2); F_qb[3][1] = -dt_half * ( q1); F_qb[3][2] = -dt_half * ( q0);

    /* 计算 F*P 的四元数部分 (4x7)
     * FP = F_qq * P[0:4][:] + F_qb * P[4:7][:]
     */
    float (*FP)[7] = p->FP;
    for (int j = 0; j < 7; j++) {
        FP[0][j] = F00*p->P[0][j] + F01*p->P[1][j] + F02*p->P[2][j] + F03*p->P[3][j]
                 + F_qb[0][0]*p->P[4][j] + F_qb[0][1]*p->P[5][j] + F_qb[0][2]*p->P[6][j];
        FP[1][j] = F10*p->P[0][j] + F11*p->P[1][j] + F12*p->P[2][j] + F13*p->P[3][j]
                 + F_qb[1][0]*p->P[4][j] + F_qb[1][1]*p->P[5][j] + F_qb[1][2]*p->P[6][j];
        FP[2][j] = F20*p->P[0][j] + F21*p->P[1][j] + F22*p->P[2][j] + F23*p->P[3][j]
                 + F_qb[2][0]*p->P[4][j] + F_qb[2][1]*p->P[5][j] + F_qb[2][2]*p->P[6][j];
        FP[3][j] = F30*p->P[0][j] + F31*p->P[1][j] + F32*p->P[2][j] + F33*p->P[3][j]
                 + F_qb[3][0]*p->P[4][j] + F_qb[3][1]*p->P[5][j] + F_qb[3][2]*p->P[6][j];
    }
    /* 偏置部分：P[4:7][:] 不变（F_bb = I, F_bq = 0） */

    /* 计算 (F*P)*F^T 的四元数部分 (4x4)
     * P_new[0:4][0:4] = FP[0:4][0:4] * F_qq^T + FP[0:4][4:7] * F_qb^T
     */
    for (int i = 0; i < 4; i++) {
        P_new[i][0] = FP[i][0]*F00 + FP[i][1]*F01 + FP[i][2]*F02 + FP[i][3]*F03
                    + FP[i][4]*F_qb[0][0] + FP[i][5]*F_qb[0][1] + FP[i][6]*F_qb[0][2];
        P_new[i][1] = FP[i][0]*F10 + FP[i][1]*F11 + FP[i][2]*F12 + FP[i][3]*F13
                    + FP[i][4]*F_qb[1][0] + FP[i][5]*F_qb[1][1] + FP[i][6]*F_qb[1][2];
        P_new[i][2] = FP[i][0]*F20 + FP[i][1]*F21 + FP[i][2]*F22 + FP[i][3]*F23
                    + FP[i][4]*F_qb[2][0] + FP[i][5]*F_qb[2][1] + FP[i][6]*F_qb[2][2];
        P_new[i][3] = FP[i][0]*F30 + FP[i][1]*F31 + FP[i][2]*F32 + FP[i][3]*F33
                    + FP[i][4]*F_qb[3][0] + FP[i][5]*F_qb[3][1] + FP[i][6]*F_qb[3][2];
    }
    /* 偏置交叉项：P_new[0:4][4:7] = FP[0:4][4:7] */
    for (int i = 0; i < 4; i++) {
        for (int j = 4; j < 7; j++) {
            P_new[i][j] = FP[i][j];
        }
    }
    /* 偏置自项：P_new[4:7][4:7] = P[4:7][4:7] */
    for (int i = 4; i < 7; i++) {
        for (int j = 4; j < 7; j++) {
            P_new[i][j] = p->P[i][j];
        }
    }
    /* 偏置-四元数交叉项：P_new[4:7][0:4] = P[4:7][0:4] * F_qq^T + P[4:7][4:7] * F_qb^T
     * 由于 F_bq = 0, F_bb = I, 实际为 P[4:7][0:4] * F_qq^T + P_bb * F_qb^T
     */
    for (int i = 4; i < 7; i++) {
        P_new[i][0] = p->P[i][0]*F00 + p->P[i][1]*F01 + p->P[i][2]*F02 + p->P[i][3]*F03
                    + p->P[i][4]*F_qb[0][0] + p->P[i][5]*F_qb[0][1] + p->P[i][6]*F_qb[0][2];
        P_new[i][1] = p->P[i][0]*F10 + p->P[i][1]*F11 + p->P[i][2]*F12 + p->P[i][3]*F13
                    + p->P[i][4]*F_qb[1][0] + p->P[i][5]*F_qb[1][1] + p->P[i][6]*F_qb[1][2];
        P_new[i][2] = p->P[i][0]*F20 + p->P[i][1]*F21 + p->P[i][2]*F22 + p->P[i][3]*F23
                    + p->P[i][4]*F_qb[2][0] + p->P[i][5]*F_qb[2][1] + p->P[i][6]*F_qb[2][2];
        P_new[i][3] = p->P[i][0]*F30 + p->P[i][1]*F31 + p->P[i][2]*F32 + p->P[i][3]*F33
                    + p->P[i][4]*F_qb[3][0] + p->P[i][5]*F_qb[3][1] + p->P[i][6]*F_qb[3][2];
    }

    /* 加过程噪声 Q */
    /* ===== 2c. 过程噪声叠加 Q ===== */
    /* [方案 E3] 分级 Q 缩放: 基于机动等级调整过程噪声
     * 静态时降低 Q → 输出更平滑; 高动态时增大 Q → 加快跟踪
     * 默认关闭(EKF_MANEUVER_QR_ADAPT=0), Q_angle/Q_bias 不变 */
#if EKF_MANEUVER_QR_ADAPT
    float q_angle_eff = p->Q_angle;
    float q_bias_eff  = p->Q_bias;
    switch (p->maneuver_level) {
        case 0: q_angle_eff *= EKF_MANEUVER_Q_SCALE_0; q_bias_eff *= EKF_MANEUVER_Q_SCALE_0; break;
        case 1: q_angle_eff *= EKF_MANEUVER_Q_SCALE_1; q_bias_eff *= EKF_MANEUVER_Q_SCALE_1; break;
        case 2: q_angle_eff *= EKF_MANEUVER_Q_SCALE_2; q_bias_eff *= EKF_MANEUVER_Q_SCALE_2; break;
        default:q_angle_eff *= EKF_MANEUVER_Q_SCALE_3; q_bias_eff *= EKF_MANEUVER_Q_SCALE_3; break;
    }
    for (int i = 0; i < 4; i++) {
        P_new[i][i] += q_angle_eff * dt;
    }
    for (int i = 4; i < 7; i++) {
        P_new[i][i] += q_bias_eff * dt;
    }
#else
    for (int i = 0; i < 4; i++) {
        P_new[i][i] += p->Q_angle * dt;
    }
    for (int i = 4; i < 7; i++) {
        P_new[i][i] += p->Q_bias * dt;
    }
#endif

    /* 复制回 P */
    memcpy(p->P, P_new, sizeof(p->P_new));

    /* 退化模式：GYRO_ONLY/STATIC_ONLY - 跳过测量更新 */
    if (self->degrade == FILTER_DEGRADE_GYRO_ONLY ||
        self->degrade == FILTER_DEGRADE_STATIC_ONLY) {
        ekf_state_to_output(p->state, out);
        return;
    }
    /* ===== 3. 测量更新 ===== */
    /* 加速度计归一化 */
    float ax = in->ax, ay = in->ay, az = in->az;
    float acc_norm = mathacl_sqrtf(ax*ax + ay*ay + az*az);
    /* 跳过异常读数：门限收紧至 [0.7, 1.3]g
     * 原门限 [0.01, 20] 过宽，1~2g 线性加速度会通过并污染姿态估计。
     * 0.7~1.3g 覆盖正常静态/缓动态场景（含安装倾角导致的重力分量变化），
     * 机动时的线性加速度（典型 >0.3g 偏差）将被拒绝，仅做陀螺积分预测。
     * [A1] 零风险优化：仅收紧判定门限，跳过逻辑不变
     *
     * [方案 D1] 自适应加速度门限: 基于陀螺角速度动态扩展门限
     * 转动越快, 允许越大的离心加速度偏差
     * gate_expand = min(gyro_mag * GYRO_FACTOR, MAX_EXPAND) */
#if EKF_ACC_GATE_ADAPTIVE
    float gyro_mag_dps = mathacl_sqrtf(in->gx * in->gx + in->gy * in->gy + in->gz * in->gz);
    float gate_expand = gyro_mag_dps * EKF_ACC_GATE_GYRO_FACTOR;
    if (gate_expand > EKF_ACC_GATE_MAX_EXPAND) gate_expand = EKF_ACC_GATE_MAX_EXPAND;
    float acc_gate_low  = EKF_ACC_GATE_LOW  - gate_expand;
    float acc_gate_high = EKF_ACC_GATE_HIGH + gate_expand;
#else
    float acc_gate_low  = EKF_ACC_GATE_LOW;
    float acc_gate_high = EKF_ACC_GATE_HIGH;
#endif

#if EKF_MANEUVER_DETECT
    /* [方案 E3] 机动等级分类: 基于陀螺角速度 + 加速度偏差
     * 纯诊断, 不改变滤波器行为 (除非 EKF_MANEUVER_QR_ADAPT=1)
     * 复用 D1 的 gyro_mag_dps (若已计算), 避免重复 sqrtf */
  #if !EKF_ACC_GATE_ADAPTIVE
    float gyro_mag_dps = mathacl_sqrtf(in->gx * in->gx + in->gy * in->gy + in->gz * in->gz);
  #endif
    float acc_err = fabsf(acc_norm - 1.0f);
    p->gyro_mag_dps = gyro_mag_dps;
    p->acc_norm_err = acc_err;

    if (gyro_mag_dps < EKF_MANEUVER_GYRO_STATIC && acc_err < EKF_MANEUVER_ACC_STATIC) {
        p->maneuver_level = 0;  /* 静止 */
    } else if (p->maneuver_level == 0 &&
               gyro_mag_dps < EKF_MANEUVER_GYRO_STATIC_EXIT &&
               acc_err < EKF_MANEUVER_ACC_STATIC) {
        /* 滞回: 已静止时用宽松阈值保持, 避免边界抖动导致 ZUPT 间歇触发 */
        p->maneuver_level = 0;
    } else if (gyro_mag_dps < EKF_MANEUVER_GYRO_QUASI && acc_err < EKF_MANEUVER_ACC_QUASI) {
        p->maneuver_level = 1;  /* 准静态 */
    } else if (gyro_mag_dps < EKF_MANEUVER_GYRO_SLOW && acc_err < EKF_MANEUVER_ACC_SLOW) {
        p->maneuver_level = 2;  /* 缓动态 */
    } else {
        p->maneuver_level = 3;  /* 高动态 */
    }
#endif /* EKF_MANEUVER_DETECT */

    if (acc_norm < acc_gate_low || acc_norm > acc_gate_high) {
        /* 只做预测，不做测量更新 */
        ekf_state_to_output(p->state, out);
        return;
    }
    ax /= acc_norm; ay /= acc_norm; az /= acc_norm;

    /* 预测的重力方向 h(q) = R^T * [0, 0, 1] */
    /* R^T * [0,0,1] = [2(q1q3-q0q2), 2(q2q3+q0q1), 1-2(q1²+q2²)] */
    float hx = 2.0f * (p->state[1]*p->state[3] - p->state[0]*p->state[2]);
    float hy = 2.0f * (p->state[0]*p->state[1] + p->state[2]*p->state[3]);
    float hz = 1.0f - 2.0f * (p->state[1]*p->state[1] + p->state[2]*p->state[2]);

    /* 测量残差 y = z - h(x) */
    float y[3] = { ax - hx, ay - hy, az - hz };

    /* ===== 动态 R 适配：根据加速度计的质量动态调整 R_measure ===== */
    /* [B2] 改进为分段式：静止时保持标准R，轻微机动缓增，剧烈机动大幅降权。
     * 原逻辑为单调递增(1+10*err)，在小误差时即过度增大R导致收敛变慢。
     * 默认 r_adapt_enable=0 时本块不执行，行为与原版完全一致。
     * 启用后：err<0.05g 保持R(静止)；0.05~0.2g 缓增；>0.2g 快速增至上限。 */
    if (p->r_adapt_enable) {
        float acc_mag_error = fabsf(acc_norm - 1.0f);
        float r_factor;
        if (acc_mag_error < 0.05f) {
            /* 静止/准静态：完全信任加速度计 */
            r_factor = 1.0f;
        } else if (acc_mag_error < 0.2f) {
            /* 轻微机动：线性过渡，因子 1→4 */
            r_factor = 1.0f + (acc_mag_error - 0.05f) * 20.0f;
        } else {
            /* 剧烈机动：大幅降低加速度计权重 */
            r_factor = EKF_R_ADAPT_FACTOR_MAX;
        }
        if (r_factor > EKF_R_ADAPT_FACTOR_MAX) r_factor = EKF_R_ADAPT_FACTOR_MAX;
        if (r_factor < EKF_R_ADAPT_FACTOR_MIN) r_factor = EKF_R_ADAPT_FACTOR_MIN;
        p->R_adapt_factor = r_factor;
    } else {
        p->R_adapt_factor = 1.0f;
    }

    /* ===== 4. 计算 H 矩阵 (3x7) =====
     * h(q) = [2(q1q3-q0q2), 2(q2q3+q0q1), 1-2(q1²+q2²)]
     * dh/dq = [[-2q2, 2q3, -2q0, 2q1],
     *          [ 2q1, 2q0,  2q3, 2q2],
     *          [   0, -4q1, -4q2,   0]]
     * dh/db = 0 (3x3)
     *
     * [YAW_DECOUPLE] 6轴IMU无磁力计时, 加速度计只能观测重力向量,
     * 无法观测 yaw。但 H 矩阵第4列(dh/dq3) = [2q1, 2q2, 0] 在 pitch/roll
     * 非零时为弱非零值, 会导致加速度计噪声通过 K[3][:] 耦合到 q3,
     * 引起 yaw 震荡和漂移。
     *
     * 修复: 强制 H 的第4列(q3 列)为 0, 数学上等价于 Madgwick 梯度在
     * yaw 方向天然为零的特性, 从根本上禁止加速度计修正 q3。
     * yaw 完全依赖陀螺仪积分, 由 BSP 层 ZUPT 补偿偏置。
     *
     * 参考: PX4 EKF2 在无磁力计时也跳过 yaw 更新;
     *       Madgwick 论文目标函数对 q3 偏导为零。
     *
     * 注: P[3][3] 仍通过 Q_angle 增长(建模不确定性), 为后续磁力计/
     * GPS 融合留接口; 但 K[3][:]=0 保证当前无观测时不会虚假修正。 */
    q0 = p->state[0]; q1 = p->state[1]; q2 = p->state[2]; q3 = p->state[3];
    float (*H)[7] = p->H;
    /* [修复] 恢复正确雅可比矩阵, 不再置零 q3 列
     * 原因: YAW_DECOUPLE 置零 H 的 q3 列会歪曲观测模型, 导致加速度计噪声
     *       通过 q0/q1/q2 的修正间接扭曲 yaw=atan2(2(q0q3+q1q2),...)
     * 仿真验证: 置零 H q3列 + R=0.05 → 慢转误差+59%; 不置零 + R=0.5 → 误差<2%
     * 替代方案: 保留 K[3][:]=0 作为 yaw 解耦, 既保持观测模型正确性,
     *           又阻止加速度计直接修正 q3 */
    H[0][0] = -2.0f*q2;  H[0][1] = 2.0f*q3;   H[0][2] = -2.0f*q0;  H[0][3] = 2.0f*q1;  /* 恢复正确值 */
    H[0][4] = 0.0f;      H[0][5] = 0.0f;      H[0][6] = 0.0f;
    H[1][0] = 2.0f*q1;   H[1][1] = 2.0f*q0;   H[1][2] = 2.0f*q3;   H[1][3] = 2.0f*q2;  /* 恢复正确值 */
    H[1][4] = 0.0f;      H[1][5] = 0.0f;      H[1][6] = 0.0f;
    H[2][0] = 0.0f;      H[2][1] = -4.0f*q1;  H[2][2] = -4.0f*q2;  H[2][3] = 0.0f;  /* 原本就为0 */
    H[2][4] = 0.0f;      H[2][5] = 0.0f;      H[2][6] = 0.0f;

    /* ===== 5. 计算 S = H * P * H^T + R (3x3) ===== */
    /* [A2] 利用 H 矩阵稀疏性：H 的 k=4,5,6 列（bias 部分）全为 0，
     * P*H^T 和 H*P*H^T 中 k=4~6 项贡献为 0，循环上限改为 4。
     * 数学完全等价，省去 3/7 的乘加运算（约 -43%）。 */
    /* 先计算 P * H^T (7x3)，仅累加 k=0~3 */
    float (*PHt)[3] = p->PHt;
    for (int i = 0; i < 7; i++) {
        for (int j = 0; j < 3; j++) {
            PHt[i][j] = p->P[i][0] * H[j][0]
                      + p->P[i][1] * H[j][1]
                      + p->P[i][2] * H[j][2]
                      + p->P[i][3] * H[j][3];
        }
    }

    /* S = H * (P * H^T) + R，仅累加 k=0~3 */
    float S[3][3];
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            S[i][j] = H[i][0] * PHt[0][j]
                    + H[i][1] * PHt[1][j]
                    + H[i][2] * PHt[2][j]
                    + H[i][3] * PHt[3][j];
            if (i == j) {
                float r_eff = p->R_measure * p->R_adapt_factor;
            #if EKF_R_ADAPTIVE
                r_eff *= p->r_nis_factor;
            #endif
            #if EKF_MANEUVER_QR_ADAPT
                /* [方案 E3] 分级 R 缩放: 机动等级越高, R 越大(降ACC权重) */
                switch (p->maneuver_level) {
                    case 0: r_eff *= EKF_MANEUVER_R_SCALE_0; break;
                    case 1: r_eff *= EKF_MANEUVER_R_SCALE_1; break;
                    case 2: r_eff *= EKF_MANEUVER_R_SCALE_2; break;
                    default:r_eff *= EKF_MANEUVER_R_SCALE_3; break;
                }
            #endif
                S[i][j] += r_eff;
            }
        }
    }
    /* 强制 S 对称，消除 FP 舍入导致的不对称 */
    for (int i = 0; i < 3; i++) {
        for (int j = i + 1; j < 3; j++) {
            float avg = 0.5f * (S[i][j] + S[j][i]);
            S[i][j] = avg;
            S[j][i] = avg;
        }
    }

    /* ===== 6. 计算 S 的逆 (3x3 解析公式) ===== */
    float a = S[0][0], b = S[0][1], c = S[0][2];
    float d = S[1][0], e = S[1][1], f = S[1][2];
    float g = S[2][0], h = S[2][1], k = S[2][2];
    float det = a*(e*k - f*h) - b*(d*k - f*g) + c*(d*h - e*g);

    /* 条件数粗估：用最大绝对元素作为缩放基准 */
    float max_s = 0.0f;
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            float v = fabsf(S[i][j]);
            if (v > max_s) max_s = v;
        }
    }
    if (max_s < EKF_COV_MIN_LIMIT) {
        max_s = EKF_COV_MIN_LIMIT;
    }
    if (fabsf(det) < EKF_INV_DET_EPS * max_s) {
        /* S 接近奇异，跳过测量更新，仅做预测 */
        ekf_state_to_output(p->state, out);
        return;
    }
    float inv_det;
    if (p->use_hw) {
        inv_det = ekf_div_hw(1.0f, det);
    } else {
        inv_det = 1.0f / det;
    }
    float S_inv[3][3];
    S_inv[0][0] = (e*k - f*h) * inv_det;
    S_inv[0][1] = (c*h - b*k) * inv_det;
    S_inv[0][2] = (b*f - c*e) * inv_det;
    S_inv[1][0] = (f*g - d*k) * inv_det;
    S_inv[1][1] = (a*k - c*g) * inv_det;
    S_inv[1][2] = (c*d - a*f) * inv_det;
    S_inv[2][0] = (d*h - e*g) * inv_det;
    S_inv[2][1] = (b*g - a*h) * inv_det;
    S_inv[2][2] = (a*e - b*d) * inv_det;

    /* ===== 6b. Chi-squared 创新门控 (卡方检验，自由度=3) ===== */
    /* 计算卡方统计量: chi2 = y^T * S^-1 * y */
    float chi2 = y[0] * (S_inv[0][0]*y[0] + S_inv[0][1]*y[1] + S_inv[0][2]*y[2])
               + y[1] * (S_inv[1][0]*y[0] + S_inv[1][1]*y[1] + S_inv[1][2]*y[2])
               + y[2] * (S_inv[2][0]*y[0] + S_inv[2][1]*y[1] + S_inv[2][2]*y[2]);
    p->last_chi2 = chi2;  /* 缓存供诊断接口读取 */

    /* [方案 N] NIS 监测: 跟踪归一化创新平方 (chi2 即 NIS), 验证调参正确性
     * 纯监测不改变滤波行为; R 自适应为可选扩展, 仅在有界范围内慢速调整 */
#if EKF_NIS_MONITOR
    {
        p->nis_window[p->nis_idx] = chi2;
        p->nis_idx = (p->nis_idx + 1) % EKF_NIS_WINDOW_SIZE;
        if (p->nis_count < EKF_NIS_WINDOW_SIZE) p->nis_count++;

        if (p->nis_count >= EKF_NIS_WINDOW_SIZE) {
            float sum = 0.0f;
            for (int i = 0; i < EKF_NIS_WINDOW_SIZE; i++) {
                sum += p->nis_window[i];
            }
            p->nis_avg = sum / EKF_NIS_WINDOW_SIZE;

        #if EKF_R_ADAPTIVE
            /* [方案 R] R 自适应: 基于 NIS 平均值慢速有界调整 r_nis_factor
             * NIS > HIGH → 滤波器过于自信 → 增大 R
             * NIS < LOW  → 滤波器不够自信 → 减小 R */
            if (p->nis_avg > EKF_NIS_CHI2_HIGH) {
                p->r_nis_factor += EKF_R_ADAPT_STEP;
            } else if (p->nis_avg < EKF_NIS_CHI2_LOW) {
                p->r_nis_factor -= EKF_R_ADAPT_STEP;
            }
            if (p->r_nis_factor > EKF_R_NIS_FACTOR_MAX)
                p->r_nis_factor = EKF_R_NIS_FACTOR_MAX;
            if (p->r_nis_factor < EKF_R_NIS_FACTOR_MIN)
                p->r_nis_factor = EKF_R_NIS_FACTOR_MIN;
        #endif /* EKF_R_ADAPTIVE */
        }
    }
#endif /* EKF_NIS_MONITOR */

    if (chi2 > p->chi2_threshold) {
        /* 创新过大，视为离群值，跳过测量更新，仅做预测 */
        ekf_state_to_output(p->state, out);
        return;
    }

    /* [方案 D2] Huber 鲁棒估计: 减大创新量权重, 抑制离群值
     * 创新量范数超过阈值时, 按阈值/范数比例缩放
     * 阈值取卡方门限 chi2_threshold 的平方根 (sqrtf(p->chi2_threshold))
     * 注: chi2_threshold 由上层调用方在 task_imu.c 中配置, 本处不引用具体数值 */
#if EKF_HUBER_WEIGHT
    {
        float innov_norm = mathacl_sqrtf(y[0]*y[0] + y[1]*y[1] + y[2]*y[2]);
        float huber_thresh = mathacl_sqrtf(p->chi2_threshold);
        if (innov_norm > huber_thresh) {
            float weight = huber_thresh / innov_norm;
            y[0] *= weight;
            y[1] *= weight;
            y[2] *= weight;
        }
    }
#endif

    /* ===== 7. 卡尔曼增益 K = P * H^T * S^-1 (7x3) ===== */
    float (*K)[3] = p->K;
    for (int i = 0; i < 7; i++) {
        for (int j = 0; j < 3; j++) {
            K[i][j] = 0.0f;
            for (int kk = 0; kk < 3; kk++) {
                K[i][j] += PHt[i][kk] * S_inv[kk][j];
            }
        }
    }

    /* [YAW_DECOUPLE] 保留 K[3][:]=0 禁止加速度计直接修正 q3(yaw)
     * 注意: H 矩阵已恢复正确雅可比(不置零 q3 列), 此处 K[3] 置零是
     *       唯一的 yaw 解耦手段, 确保加速度计只修正 roll/pitch 不修正 yaw.
     * 仿真验证: H 正确 + K[3]=0 + R=0.5 → 慢转误差<2%, 快转<0.1% */
    K[3][0] = 0.0f;
    K[3][1] = 0.0f;
    K[3][2] = 0.0f;

    /* ===== 8. 状态更新 x = x + K * y ===== */
    for (int i = 0; i < 7; i++) {
        for (int j = 0; j < 3; j++) {
            p->state[i] += K[i][j] * y[j];
        }
    }

    /* 偏置幅值限制 (防止偏置发散) */
    float bias_limit_dps = p->bias_limit_dps;
    for (int i = 4; i < 7; i++) {
        if (p->state[i] > bias_limit_dps) p->state[i] = bias_limit_dps;
        else if (p->state[i] < -bias_limit_dps) p->state[i] = -bias_limit_dps;
    }

    /* 四元数归一化
     * [修复] 测量更新后状态已被 K*y 修改, 无条件归一化
     * 原: 与预测后共用 norm_skip_count 条件归一化, 导致双重递增
     *     (预测后 ++ 一次, 测量更新后又 ++ 一次, 强制归一化频率翻倍)
     * 新: 测量更新后直接归一化, norm_skip_count 仅由预测后管理 */
    ekf_normalize_quaternion(&p->state[0], &p->state[1], &p->state[2], &p->state[3]);

    /* ===== 9. 协方差更新 P = (I - K*H) * P ===== */
    /* 计算 K*H (7x7) */
    /* [A3] 利用 H 稀疏性：H 的 j=4,5,6 列全为 0，K*H 对应列也为 0。
     * 仅计算 j=0~3 列，j=4~6 列直接置 0。省 3/7 运算量。 */
    float (*KH)[7] = p->KH;
    for (int i = 0; i < 7; i++) {
        for (int j = 0; j < 4; j++) {
            KH[i][j] = K[i][0] * H[0][j]
                     + K[i][1] * H[1][j]
                     + K[i][2] * H[2][j];
        }
        /* j=4~6 列：H[kk][j]=0，故 KH[i][j]=0 */
        KH[i][4] = 0.0f;  KH[i][5] = 0.0f;  KH[i][6] = 0.0f;
    }

    /* P = (I - K*H) * P 协方差更新 */
    /* [B3] 编译开关选择 Joseph 形式或标准形式。
     * EKF_USE_JOSEPH_FORM=1: Joseph（数值稳定，+33%运算量）
     * EKF_USE_JOSEPH_FORM=0: 标准形式（默认，已有对称化+对角保护兜底） */
    float (*I_KH)[7] = p->I_KH;
    for (int i = 0; i < 7; i++) {
        for (int j = 0; j < 7; j++) {
            I_KH[i][j] = (i == j ? 1.0f : 0.0f) - KH[i][j];
        }
    }
#if EKF_USE_JOSEPH_FORM
    /* Joseph 形式：temp = I_KH * P，P_new = temp * I_KH^T
     * [方案 H1] double 累加: 与标准形式保持一致, 减少浮点累加精度损失 */
    float (*temp)[7] = p->temp;
#if EKF_DOUBLE_ACCUM
    for (int i = 0; i < 7; i++) {
        for (int j = 0; j < 7; j++) {
            double sum = 0.0;
            for (int k = 0; k < 7; k++) {
                sum += (double)I_KH[i][k] * (double)p->P[k][j];
            }
            temp[i][j] = (float)sum;
        }
    }
    for (int i = 0; i < 7; i++) {
        for (int j = 0; j < 7; j++) {
            double sum = 0.0;
            for (int k = 0; k < 7; k++) {
                sum += (double)temp[i][k] * (double)I_KH[j][k];
            }
            P_new[i][j] = (float)sum;
        }
    }
#else
    for (int i = 0; i < 7; i++) {
        for (int j = 0; j < 7; j++) {
            temp[i][j] = 0.0f;
            for (int k = 0; k < 7; k++) {
                temp[i][j] += I_KH[i][k] * p->P[k][j];
            }
        }
    }
    for (int i = 0; i < 7; i++) {
        for (int j = 0; j < 7; j++) {
            P_new[i][j] = 0.0f;
            for (int k = 0; k < 7; k++) {
                P_new[i][j] += temp[i][k] * I_KH[j][k];
            }
        }
    }
#endif
    /* 加 K*R*K^T：R_eff 为标量，直接计算外积 */
    float R_eff = p->R_measure * p->R_adapt_factor;
#if EKF_R_ADAPTIVE
    R_eff *= p->r_nis_factor;
#endif
#if EKF_MANEUVER_QR_ADAPT
    /* [方案 E3] Joseph 形式 R 缩放, 与 S 矩阵保持一致 */
    switch (p->maneuver_level) {
        case 0: R_eff *= EKF_MANEUVER_R_SCALE_0; break;
        case 1: R_eff *= EKF_MANEUVER_R_SCALE_1; break;
        case 2: R_eff *= EKF_MANEUVER_R_SCALE_2; break;
        default:R_eff *= EKF_MANEUVER_R_SCALE_3; break;
    }
#endif
    for (int i = 0; i < 7; i++) {
        for (int j = 0; j < 7; j++) {
            for (int kk = 0; kk < 3; kk++) {
                P_new[i][j] += K[i][kk] * R_eff * K[j][kk];
            }
        }
    }
#else
    /* 标准形式：P_new = (I-KH) * P，仅一次矩阵乘法
     * [方案 H1] double 累加: 减少浮点累加精度损失 */
#if EKF_DOUBLE_ACCUM
    for (int i = 0; i < 7; i++) {
        for (int j = 0; j < 7; j++) {
            double sum = 0.0;
            for (int k = 0; k < 7; k++) {
                sum += (double)I_KH[i][k] * (double)p->P[k][j];
            }
            P_new[i][j] = (float)sum;
        }
    }
#else
    for (int i = 0; i < 7; i++) {
        for (int j = 0; j < 7; j++) {
            P_new[i][j] = 0.0f;
            for (int k = 0; k < 7; k++) {
                P_new[i][j] += I_KH[i][k] * p->P[k][j];
            }
        }
    }
#endif
#endif

    /* 确保协方差矩阵对称且正定 */
    for (int i = 0; i < 7; i++) {
        for (int j = i; j < 7; j++) {
            p->P[i][j] = 0.5f * (P_new[i][j] + P_new[j][i]);
            p->P[j][i] = p->P[i][j];
        }
    }
    /* 对角线强制为正 */
    for (int i = 0; i < 7; i++) {
        if (p->P[i][i] < EKF_COV_MIN_LIMIT) p->P[i][i] = EKF_COV_MIN_LIMIT;
    }

    /* ===== 10b. 协方差矩阵定期正则化 (每 100 次更新) ===== */
    p->update_count++;
    if (p->update_count >= 100) {
        p->update_count = 0;
        /* 强制对称化 */
        for (int i = 0; i < 7; i++) {
            for (int j = i + 1; j < 7; j++) {
                float avg = 0.5f * (p->P[i][j] + p->P[j][i]);
                p->P[i][j] = avg;
                p->P[j][i] = avg;
            }
        }
        /* 对角线下界保护 */
        for (int i = 0; i < 7; i++) {
            if (p->P[i][i] < EKF_COV_MIN_LIMIT) p->P[i][i] = EKF_COV_MIN_LIMIT;
        }
        /* 最大协方差限制，防止发散 */
        for (int i = 0; i < 7; i++) {
            if (p->P[i][i] > EKF_COV_MAX_LIMIT) p->P[i][i] = EKF_COV_MAX_LIMIT;
        }
    }

    /* ===== 10a. 数值异常保护：若状态或协方差发散，重置滤波器 ===== */
    if (ekf_has_nan_inf(p->state, EKF_STATE_SIZE) ||
        !ekf_covariance_healthy(p->P)) {
        ekf_reset(self);
    }

    /* ===== 10b. ZUPT (Zero-velocity Update) - 零速更新 =====
     * 静止时陀螺读数=零偏, 直接注入偏置观测
     * 解决: yaw在无磁力计时不可观测, EKF无法估计bz
     *       ZUPT提供独立的偏置观测源, 不依赖加速度计
     *
     * 最终策略 (D1实验结论):
     *   level=0(静止):    窗口均值强观测 (降噪√N)
     *   level=1/2/3(运动): 不更新偏置, 清空窗口
     *
     * 实验记录:
     *   - level=1 门控EMA: 60s drift 14° (噪声尖峰触发level抖动, 系统性偏移)
     *   - level=2 门控EMA: 60s drift 5° (偶发振动触发, 系统性偏移)
     *   - 不更新+不清空窗口: 60s drift 2.4° (level抖动时窗口永不满, ZUPT失效)
     *   - 不更新+清空窗口: 60s drift 0.2° (P1+P2+P3基准, 物理极限)
     *
     * 根本原因: 运动时陀螺读数=真实角速度+偏置+噪声, 三者无法分离
     *           任何 level>0 的偏置更新都会被真实角速度系统性拉偏
     *           门控阈值无法区分"真实角速度接近偏置"与"纯偏置"场景
     *
     * 清空窗口理由: level 频繁抖动时保留窗口会导致 count 徘徊不满,
     *              ZUPT 更新冻结; 清空窗口保证连续 8 帧静止才更新, 稳定可靠 */
#if EKF_ZUPT_ENABLE
    if (p->maneuver_level == 0) {
        /* ===== 静止: 窗口均值强观测 (逻辑不变) ===== */
        uint8_t idx = p->zupt_win_idx;
        p->zupt_win_gx[idx] = in->gx;
        p->zupt_win_gy[idx] = in->gy;
        p->zupt_win_gz[idx] = in->gz;
        p->zupt_win_idx = (idx + 1) & (EKF_ZUPT_WINDOW_SIZE - 1);
        if (p->zupt_win_count < EKF_ZUPT_WINDOW_SIZE) {
            p->zupt_win_count++;
        }

        if (p->zupt_win_count >= EKF_ZUPT_WINDOW_SIZE) {
            float sum_x = 0.0f, sum_y = 0.0f, sum_z = 0.0f;
            for (int i = 0; i < EKF_ZUPT_WINDOW_SIZE; i++) {
                sum_x += p->zupt_win_gx[i];
                sum_y += p->zupt_win_gy[i];
                sum_z += p->zupt_win_gz[i];
            }
            float mean_gx = sum_x / EKF_ZUPT_WINDOW_SIZE;
            float mean_gy = sum_y / EKF_ZUPT_WINDOW_SIZE;
            float mean_gz = sum_z / EKF_ZUPT_WINDOW_SIZE;

            /* 单轴门控: 窗口均值代表该轴真实角速度, 若非零则不更新该轴偏置
             * 防止慢转(2-3dps)滞回误判为 level=0 时, 用真实角速度污染偏置
             * 3 轴独立判断, 仅静止轴更新偏置, 转动轴保持 */
            if (fabsf(mean_gx) < EKF_ZUPT_AXIS_GATE) {
                float dbx = EKF_ZUPT_RATE * (mean_gx - p->state[4]);
                if (dbx >  EKF_ZUPT_MAX_DELTA)  dbx =  EKF_ZUPT_MAX_DELTA;
                if (dbx < -EKF_ZUPT_MAX_DELTA)  dbx = -EKF_ZUPT_MAX_DELTA;
                p->state[4] += dbx;
            }
            if (fabsf(mean_gy) < EKF_ZUPT_AXIS_GATE) {
                float dby = EKF_ZUPT_RATE * (mean_gy - p->state[5]);
                if (dby >  EKF_ZUPT_MAX_DELTA)  dby =  EKF_ZUPT_MAX_DELTA;
                if (dby < -EKF_ZUPT_MAX_DELTA)  dby = -EKF_ZUPT_MAX_DELTA;
                p->state[5] += dby;
            }
            if (fabsf(mean_gz) < EKF_ZUPT_AXIS_GATE) {
                float dbz = EKF_ZUPT_RATE * (mean_gz - p->state[6]);
                if (dbz >  EKF_ZUPT_MAX_DELTA)  dbz =  EKF_ZUPT_MAX_DELTA;
                if (dbz < -EKF_ZUPT_MAX_DELTA)  dbz = -EKF_ZUPT_MAX_DELTA;
                p->state[6] += dbz;
            }
        }
    } else {
        /* level=1/2/3(运动): 不更新偏置, 清空窗口
         * 根因: 运动时陀螺读数=真实角速度+偏置+噪声, 无法分离
         *       任何 level>0 的偏置更新都会被真实角速度系统性拉偏
         *       (实测 level=1: 60s drift 14°, level=2: 60s drift 5°)
         * 清空窗口: 保证回到 level=0 后连续 8 帧静止才更新,
         *          避免 level 抖动时旧窗口数据污染或窗口永不满 */
        p->zupt_win_count = 0;
        p->zupt_win_idx   = 0;
    }
#endif

    /* ===== 10. 输出 ===== */
    ekf_state_to_output(p->state, out);
}

void ekf_reset(filter_t *self)
{
    ekf_priv_t *p = (ekf_priv_t *)self->priv;
    memset(p->state, 0, sizeof(p->state));
    memset(p->P, 0, sizeof(p->P));
    p->state[0] = 1.0f;  /* q0 = 1 */
    for (int i = 0; i < EKF_STATE_SIZE; i++) {
        p->P[i][i] = 1.0f;
    }
#if EKF_NIS_MONITOR
    memset(p->nis_window, 0, sizeof(p->nis_window));
    p->nis_idx = 0;
    p->nis_count = 0;
    p->nis_avg = 0.0f;
#endif
#if EKF_R_ADAPTIVE
    p->r_nis_factor = 1.0f;
#endif
#if EKF_MANEUVER_DETECT
    p->maneuver_level = 0;  /* 初始假设静止 */
    p->gyro_mag_dps = 0.0f;
    p->acc_norm_err = 0.0f;
#endif
#if EKF_ZUPT_ENABLE
    /* ZUPT 滑动窗口初始化 */
    memset(p->zupt_win_gx, 0, sizeof(p->zupt_win_gx));
    memset(p->zupt_win_gy, 0, sizeof(p->zupt_win_gy));
    memset(p->zupt_win_gz, 0, sizeof(p->zupt_win_gz));
    p->zupt_win_idx = 0;
    p->zupt_win_count = 0;
#endif
#if EKF_GYRO_RESIDUAL_ENABLE
    p->residual_active_frames = 0;
    p->residual_decay_factor = 1.0f;
#endif
    /* [修复] 重置计数器, 避免 reset 后正则化/归一化周期错乱 */
    p->norm_skip_count = 0;
    p->update_count = 0;
}

void ekf_set_param(filter_t *self, filter_param_t param, float value)
{
    if (validate_filter_param(param, value) != 0) {
        return;  /* 参数无效，拒绝设置 */
    }

    ekf_priv_t *p = (ekf_priv_t *)self->priv;
    switch (param) {
        case FILTER_PARAM_Q_ANGLE:  p->Q_angle  = value; break;
        case FILTER_PARAM_Q_BIAS:   p->Q_bias   = value; break;
        case FILTER_PARAM_R_MEASURE: p->R_measure = value; break;
        case FILTER_PARAM_BIAS_LIMIT_DPS: p->bias_limit_dps = value; break;
        case FILTER_PARAM_CHI2_THRESHOLD: p->chi2_threshold = value; break;
        case FILTER_PARAM_R_ADAPT_ENABLE: p->r_adapt_enable = (int)value; break;
        case FILTER_PARAM_R_ADAPT_FACTOR: p->R_adapt_factor = value; break;
        default: break;
    }
}

static void ekf_destroy(filter_t *self)
{
    if (self->is_static) return;
    free(self->priv);
    free(self);
}

filter_t* filter_create_ekf(float q_angle, float q_bias, float r_measure)
{
    filter_t *f = (filter_t *)malloc(sizeof(filter_t));
    if (!f) return NULL;

    ekf_priv_t *p = (ekf_priv_t *)malloc(sizeof(ekf_priv_t));
    if (!p) { free(f); return NULL; }

    p->Q_angle = q_angle;
    p->Q_bias = q_bias;
    p->R_measure = r_measure;

    /* 初始化状态 */
    memset(p->state, 0, sizeof(p->state));
    p->state[0] = 1.0f;  /* q0 = 1 */

    /* 初始化协方差矩阵 */
    memset(p->P, 0, sizeof(p->P));
    for (int i = 0; i < EKF_STATE_SIZE; i++) {
        p->P[i][i] = 1.0f;
    }

    /* 初始化新增字段 */
    p->bias_limit_dps = EKF_BIAS_LIMIT_DEFAULT;
    p->chi2_threshold = EKF_CHI2_THRESHOLD_DEFAULT;
    p->R_adapt_factor = 1.0f;
    p->r_adapt_enable = EKF_R_ADAPT_ENABLE_DEFAULT;
    p->update_count = 0;
#if defined(BSP_MATHACL_EKF_HW)
    p->use_hw = 1;
#else
    p->use_hw = 0;
#endif
#if EKF_NIS_MONITOR
    memset(p->nis_window, 0, sizeof(p->nis_window));
    p->nis_idx = 0;
    p->nis_count = 0;
    p->nis_avg = 0.0f;
#endif
#if EKF_R_ADAPTIVE
    p->r_nis_factor = 1.0f;  /* 初始无缩放 */
#endif
#if EKF_MANEUVER_DETECT
    p->maneuver_level = 0;
    p->gyro_mag_dps = 0.0f;
    p->acc_norm_err = 0.0f;
#endif

    f->update    = ekf_update;
    f->reset     = ekf_reset;
    f->set_param = ekf_set_param;
    f->destroy   = ekf_destroy;
    f->type      = FILTER_TYPE_EKF;
    f->degrade   = FILTER_DEGRADE_NONE;
    f->priv      = p;
    f->is_static = 0;

    return f;
}
