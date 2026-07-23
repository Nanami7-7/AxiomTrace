/**
 * @file    filter_core.c
 * @brief   滤波器核心实现：错误处理、工厂函数、静态分配、数值安全
 *
 * 实现：
 *   - 错误处理机制（回调、报告、查询）
 *   - 数值安全辅助函数（四元数归一化、欧拉角转换等）
 *   - 参数验证
 *   - 工厂函数 filter_create()
 *   - 退化模式 API
 *   - 静态分配支持
 *   - 数值安全保护
 *   - 硬件加速配置接口
 *   - 诊断信息接口
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
 * 错误处理机制
 * ============================================================ */

/* 全局错误回调 */
filter_error_callback_t g_error_callback = NULL;
void *g_error_user_data = NULL;

/* 最后的错误信息 */
filter_error_info_t g_last_error = {0};

/**
 * @brief 设置全局错误回调
 */
void filter_set_error_callback(filter_error_callback_t callback, void *user_data)
{
    g_error_callback = callback;
    g_error_user_data = user_data;
}

/**
 * @brief 获取最后的错误信息
 */
filter_error_info_t filter_get_last_error(void)
{
    return g_last_error;
}

/**
 * @brief 报告错误（内部函数）
 */
void report_error(filter_error_t code, const char *message,
                  const char *file, int line)
{
    filter_error_info_t info = {
        .code = code,
        .message = message,
        .file = file,
        .line = line
    };

    g_last_error = info;

    if (g_error_callback) {
        g_error_callback(&info, g_error_user_data);
    }

    #if FILTER_DEBUG_VERBOSE
    fprintf(stderr, "[FILTER ERROR] %s:%d: %s (code=%d)\n",
            file, line, message, code);
    #endif
}

/* ============================================================
 * 数值安全函数
 * ============================================================ */

/**
 * @brief 内部四元数归一化函数（统一实现）
 *
 * 处理所有边界情况：
 * - NaN/Inf 输入：重置为单位四元数
 * - 接近零的范数：重置为单位四元数
 * - 正常情况：归一化
 *
 * @param q0, q1, q2, q3  四元数（输入/输出）
 * @return 0=成功, -1=异常（已重置为单位四元数）
 */
int normalize_quaternion_internal(float *q0, float *q1, float *q2, float *q3)
{
    /* 检查 NaN */
    if (isnan(*q0) || isnan(*q1) || isnan(*q2) || isnan(*q3)) {
        *q0 = 1.0f; *q1 = 0.0f; *q2 = 0.0f; *q3 = 0.0f;
        return -1;
    }

    /* 检查 Inf */
    if (isinf(*q0) || isinf(*q1) || isinf(*q2) || isinf(*q3)) {
        *q0 = 1.0f; *q1 = 0.0f; *q2 = 0.0f; *q3 = 0.0f;
        return -1;
    }

    /* 计算范数 */
    float norm = mathacl_sqrtf((*q0)*(*q0) + (*q1)*(*q1) + (*q2)*(*q2) + (*q3)*(*q3));

    /* 统一阈值检查 */
    if (norm < NORM_EPS) {
        /* 范数太小，重置为单位四元数 */
        *q0 = 1.0f; *q1 = 0.0f; *q2 = 0.0f; *q3 = 0.0f;
        return -1;
    }

    /* 正常归一化 */
    float inv_norm = 1.0f / norm;
    *q0 *= inv_norm;
    *q1 *= inv_norm;
    *q2 *= inv_norm;
    *q3 *= inv_norm;

    return 0;
}

/* ============================================================
 * 公共辅助函数（重构提取，消除跨滤波器重复代码）
 * ============================================================ */

/**
 * @brief 四元数转欧拉角（统一实现）
 *
 * 提取自 EKF/Mahony/Madgwick 中重复的四元数→欧拉角转换。
 * 运算顺序与原代码完全一致，使用 MATHACL 硬件加速。
 *
 * @param q0,q1,q2,q3  四元数
 * @param out          输出（同时设置 q0..q3 和 pitch/roll/yaw）
 */
void filter_quat_to_euler(float q0, float q1, float q2, float q3,
                           filter_output_t *out)
{
    out->q0 = q0; out->q1 = q1; out->q2 = q2; out->q3 = q3;
    out->pitch = mathacl_asinf(fmaxf(-1.0f, fminf(1.0f, -2.0f * (q1*q3 - q0*q2)))) * 180.0f / M_PI;
    out->roll  = mathacl_atan2f(2.0f * (q0*q1 + q2*q3), 1.0f - 2.0f * (q1*q1 + q2*q2)) * 180.0f / M_PI;
    out->yaw   = mathacl_atan2f(2.0f * (q0*q3 + q1*q2), 1.0f - 2.0f * (q2*q2 + q3*q3)) * 180.0f / M_PI;
}

/**
 * @brief 从加速度计重建四元数（yaw=0）
 *
 * 提取自 EKF/Mahony/Madgwick 的 ACC_ONLY 退化模式。
 * 阈值 0.01f 与原代码一致。
 *
 * @param ax,ay,az  加速度（原始值，内部归一化）
 * @param q0..q3    输出四元数
 * @return 0=成功, -1=ACC 幅值过小（未更新四元数）
 */
int filter_acc_to_quat(float ax, float ay, float az,
                       float *q0, float *q1, float *q2, float *q3)
{
    float norm = mathacl_sqrtf(ax*ax + ay*ay + az*az);
    if (norm < 0.01f) {
        return -1;  /* ACC 幅值过小，不更新 */
    }
    float ax_n = ax/norm, ay_n = ay/norm, az_n = az/norm;
    float acc_pitch_rad = mathacl_atan2f(-ax_n, mathacl_sqrtf(ay_n*ay_n + az_n*az_n));
    float acc_roll_rad  = mathacl_atan2f( ay_n, mathacl_sqrtf(ax_n*ax_n + az_n*az_n));
    float cp, sp, cr, sr;
    mathacl_sincosf(acc_pitch_rad * 0.5f, &sp, &cp);
    mathacl_sincosf(acc_roll_rad * 0.5f, &sr, &cr);
    *q0 = cp * cr;
    *q1 = cp * sr;
    *q2 = sp * cr;
    *q3 = -sp * sr;  /* yaw=0 */
    return 0;
}

/**
 * @brief GYRO_ONLY 四元数积分
 *
 * 提取自 Mahony/Madgwick 的 GYRO_ONLY 退化模式及正常模式末尾积分。
 * 使用局部变量保存旧四元数值，与原代码实现方式一致。
 *
 * @param q0..q3   四元数（输入/输出）
 * @param gx,gy,gz  角速度（rad/s，调用方负责 dps→rad/s 转换）
 * @param dt        时间间隔（秒）
 */
void filter_quat_integrate_gyro(float *q0, float *q1, float *q2, float *q3,
                                float gx, float gy, float gz, float dt)
{
    float q0v = *q0, q1v = *q1, q2v = *q2, q3v = *q3;
    *q0 += 0.5f * (-q1v * gx - q2v * gy - q3v * gz) * dt;
    *q1 += 0.5f * ( q0v * gx + q2v * gz - q3v * gy) * dt;
    *q2 += 0.5f * ( q0v * gy - q1v * gz + q3v * gx) * dt;
    *q3 += 0.5f * ( q0v * gz + q1v * gy - q2v * gx) * dt;
    normalize_quaternion_internal(q0, q1, q2, q3);
}

/* ============================================================
 * 参数验证
 * ============================================================ */

/**
 * @brief 验证滤波器参数
 * @param param  参数类型
 * @param value  参数值
 * @return 0=有效, -1=无效
 */
int validate_filter_param(filter_param_t param, float value)
{
    /* 检查 NaN/Inf */
    if (isnan(value) || isinf(value)) {
        FILTER_REPORT_ERROR(FILTER_ERR_INVALID_PARAM, "Parameter is NaN or Inf");
        return -1;
    }

    switch (param) {
        case FILTER_PARAM_ALPHA:
            /* α 应该在 0-1 之间 */
            if (value < 0.0f || value > 1.0f) {
                FILTER_REPORT_ERROR(FILTER_ERR_INVALID_PARAM,
                                    "Alpha must be in [0.0, 1.0]");
                return -1;
            }
            break;

        case FILTER_PARAM_CUTOFF_FREQ:
            if (value < LPF_CUTOFF_MIN || value > LPF_CUTOFF_MAX) {
                FILTER_REPORT_ERROR(FILTER_ERR_INVALID_PARAM,
                                    "Cutoff frequency out of range");
                return -1;
            }
            break;

        case FILTER_PARAM_Q_ANGLE:
            if (value < EKF_Q_ANGLE_MIN || value > EKF_Q_ANGLE_MAX) {
                FILTER_REPORT_ERROR(FILTER_ERR_INVALID_PARAM,
                                    "Q_ANGLE out of range");
                return -1;
            }
            break;

        case FILTER_PARAM_Q_BIAS:
            if (value < EKF_Q_BIAS_MIN || value > EKF_Q_BIAS_MAX) {
                FILTER_REPORT_ERROR(FILTER_ERR_INVALID_PARAM,
                                    "Q_BIAS out of range");
                return -1;
            }
            break;

        case FILTER_PARAM_R_MEASURE:
            if (value < EKF_R_MEASURE_MIN || value > EKF_R_MEASURE_MAX) {
                FILTER_REPORT_ERROR(FILTER_ERR_INVALID_PARAM,
                                    "R_MEASURE out of range");
                return -1;
            }
            break;

        case FILTER_PARAM_KP:
        case FILTER_PARAM_KI:
            /* 增益参数必须非负 */
            if (value < 0.0f) {
                FILTER_REPORT_ERROR(FILTER_ERR_INVALID_PARAM,
                                    "Gain parameter must be non-negative");
                return -1;
            }
            break;

        case FILTER_PARAM_BIAS_LIMIT_DPS:
            if (value < EKF_BIAS_LIMIT_MIN || value > EKF_BIAS_LIMIT_MAX) {
                FILTER_REPORT_ERROR(FILTER_ERR_INVALID_PARAM,
                                    "BIAS_LIMIT_DPS out of range");
                return -1;
            }
            break;

        case FILTER_PARAM_CHI2_THRESHOLD:
            if (value < EKF_CHI2_THRESHOLD_MIN || value > EKF_CHI2_THRESHOLD_MAX) {
                FILTER_REPORT_ERROR(FILTER_ERR_INVALID_PARAM,
                                    "CHI2_THRESHOLD out of range");
                return -1;
            }
            break;

        case FILTER_PARAM_R_ADAPT_ENABLE:
            /* 必须为 0 或 1 */
            if (value < 0.0f || value > 1.0f) {
                FILTER_REPORT_ERROR(FILTER_ERR_INVALID_PARAM,
                                    "R_ADAPT_ENABLE must be 0 or 1");
                return -1;
            }
            break;

        case FILTER_PARAM_R_ADAPT_FACTOR:
            /* 范围 [0.1, 10] */
            if (value < EKF_R_ADAPT_FACTOR_MIN || value > EKF_R_ADAPT_FACTOR_MAX) {
                FILTER_REPORT_ERROR(FILTER_ERR_INVALID_PARAM,
                                    "R_ADAPT_FACTOR out of range");
                return -1;
            }
            break;

        /* ---- KF 纯卡尔曼滤波器参数 ---- */
        case FILTER_PARAM_KF_Q_ANGLE:
            if (value < KF_Q_ANGLE_MIN || value > KF_Q_ANGLE_MAX) {
                FILTER_REPORT_ERROR(FILTER_ERR_INVALID_PARAM,
                                    "KF Q_ANGLE out of range");
                return -1;
            }
            break;

        case FILTER_PARAM_KF_Q_BIAS:
            if (value < KF_Q_BIAS_MIN || value > KF_Q_BIAS_MAX) {
                FILTER_REPORT_ERROR(FILTER_ERR_INVALID_PARAM,
                                    "KF Q_BIAS out of range");
                return -1;
            }
            break;

        case FILTER_PARAM_KF_R_MEASURE:
            if (value < KF_R_MEASURE_MIN || value > KF_R_MEASURE_MAX) {
                FILTER_REPORT_ERROR(FILTER_ERR_INVALID_PARAM,
                                    "KF R_MEASURE out of range");
                return -1;
            }
            break;

        case FILTER_PARAM_KF_R_ZUPT:
            if (value < KF_R_ZUPT_MIN || value > KF_R_ZUPT_MAX) {
                FILTER_REPORT_ERROR(FILTER_ERR_INVALID_PARAM,
                                    "KF R_ZUPT out of range");
                return -1;
            }
            break;

        default:
            FILTER_REPORT_ERROR(FILTER_ERR_INVALID_PARAM, "Unknown parameter");
            return -1;
    }

    return 0;
}

/* ============================================================
 * 工厂函数
 * ============================================================ */

filter_t* filter_create(filter_type_t type)
{
    filter_t *f = NULL;
    switch (type) {
        case FILTER_TYPE_COMPLEMENTARY:
            f = filter_create_complementary(COMP_ALPHA_DEFAULT);
            break;
        case FILTER_TYPE_LPF:
            f = filter_create_lpf(LPF_CUTOFF_DEFAULT);
            break;
        case FILTER_TYPE_ESKF:
            f = filter_create_eskf(EKF_Q_ANGLE_DEFAULT, EKF_Q_BIAS_DEFAULT, EKF_R_MEASURE_DEFAULT);
            break;
        case FILTER_TYPE_LKF:
            f = filter_create_lkf(KF_Q_ANGLE_DEFAULT, KF_Q_BIAS_DEFAULT, KF_R_MEASURE_DEFAULT);
            break;
        case FILTER_TYPE_MAHONY:
            f = filter_create_mahony(MAHONY_KP_DEFAULT, MAHONY_KI_DEFAULT);
            break;
        case FILTER_TYPE_MADGWICK:
            f = filter_create_madgwick(MADGWICK_BETA_DEFAULT);
            break;
        case FILTER_TYPE_KF:
            f = filter_create_kf(KF_Q_ANGLE_DEFAULT, KF_Q_BIAS_DEFAULT, KF_R_MEASURE_DEFAULT);
            break;
        default:
            FILTER_REPORT_ERROR(FILTER_ERR_INVALID_TYPE, "Invalid filter type");
            return NULL;
    }
    if (f) {
        f->is_static = 0;
        f->safety_config = (filter_safety_config_t)FILTER_SAFETY_DEFAULT;
    } else {
        FILTER_REPORT_ERROR(FILTER_ERR_ALLOC_FAILED, "Failed to allocate filter");
    }
    return f;
}

const char* filter_type_name(filter_type_t type)
{
    static const char *names[] = {
        "Complementary",
        "LPF",
        "ESKF",
        "LKF",
        "Mahony",
        "Madgwick",
        "KF"
    };
    if (type >= 0 && type < FILTER_TYPE_COUNT) {
        return names[type];
    }
    return "Unknown";
}

/* ============================================================
 * 退化模式API
 * ============================================================ */

static const char *degrade_names[] = {
    "None",
    "StaticOnly",
    "GyroOnly",
    "AccOnly",
    "HoldLast"
};

void filter_set_degrade(filter_t *f, filter_degrade_t degrade) {
    if (f && degrade >= 0 && degrade < FILTER_DEGRADE_COUNT) {
        f->degrade = degrade;
    }
}

/**
 * @brief 安全销毁滤波器
 *
 * 处理所有边界情况：
 * - NULL 指针：安全忽略
 * - 静态分配：只重置状态，不释放内存
 * - destroy 函数指针为 NULL：安全忽略
 * - 动态分配：正常释放内存
 */
void filter_destroy_safe(filter_t *f)
{
    if (!f) {
        return;  /* NULL 指针安全 */
    }

    if (f->is_static) {
        /* 静态分配：重置状态但不释放内存 */
        if (f->reset) {
            f->reset(f);
        }
        return;
    }

    /* 动态分配：正常销毁 */
    if (f->destroy) {
        f->destroy(f);
    }
}

const char* filter_degrade_name(filter_degrade_t degrade) {
    if (degrade >= 0 && degrade < FILTER_DEGRADE_COUNT) {
        return degrade_names[degrade];
    }
    return "Unknown";
}

int filter_check_acc_quality(float ax, float ay, float az) {
    float norm = mathacl_sqrtf(ax*ax + ay*ay + az*az);
    return (norm >= DEGRADE_ACC_LOW && norm <= DEGRADE_ACC_HIGH) ? 1 : 0;
}

int filter_check_gyro_quality(float gx, float gy, float gz) {
    return (fabsf(gx) <= DEGRADE_GYRO_THRESHOLD && fabsf(gy) <= DEGRADE_GYRO_THRESHOLD && fabsf(gz) <= DEGRADE_GYRO_THRESHOLD) ? 1 : 0;
}

/* ============================================================
 * 静态分配支持（MCU友好）
 * ============================================================ */

/**
 * @brief 空操作的销毁函数（用于静态分配）
 */
void static_destroy_noop(filter_t *self)
{
    /* 静态分配不需要销毁，但调用是安全的 */
    (void)self;
}

/* 各滤波器私有数据大小 */
const size_t priv_sizes[] = {
    [FILTER_TYPE_COMPLEMENTARY] = sizeof(complementary_priv_t),
    [FILTER_TYPE_LPF]           = sizeof(lpf_priv_t),
    [FILTER_TYPE_ESKF]          = sizeof(eskf_priv_t),
    [FILTER_TYPE_LKF]           = sizeof(lkf_priv_t),
    [FILTER_TYPE_MAHONY]        = sizeof(mahony_priv_t),
    [FILTER_TYPE_MADGWICK]      = sizeof(madgwick_priv_t),
    [FILTER_TYPE_KF]            = sizeof(kf_priv_t),
};

#define FILTER_STORAGE_ASSERT(type_) \
    _Static_assert(sizeof(filter_t) + sizeof(type_) <= FILTER_STATIC_STORAGE_SIZE, \
                   "FILTER_STATIC_STORAGE_SIZE is too small")
FILTER_STORAGE_ASSERT(complementary_priv_t);
FILTER_STORAGE_ASSERT(lpf_priv_t);
FILTER_STORAGE_ASSERT(eskf_priv_t);
FILTER_STORAGE_ASSERT(lkf_priv_t);
FILTER_STORAGE_ASSERT(mahony_priv_t);
FILTER_STORAGE_ASSERT(madgwick_priv_t);
FILTER_STORAGE_ASSERT(kf_priv_t);
#undef FILTER_STORAGE_ASSERT

size_t filter_get_static_size(filter_type_t type) {
    if (type < 0 || type >= FILTER_TYPE_COUNT) return 0;
    return sizeof(filter_t) + priv_sizes[type];
}

filter_t* filter_create_static(filter_type_t type, void *buf, size_t buf_size) {
    if (!buf || type < 0 || type >= FILTER_TYPE_COUNT) return NULL;
    
    size_t required = filter_get_static_size(type);
    if (buf_size < required) return NULL;
    
    /* 对齐检查 */
    if ((uintptr_t)buf % sizeof(void*) != 0) return NULL;
    
    filter_t *f = (filter_t *)buf;
    void *priv = (uint8_t*)buf + sizeof(filter_t);
    
    /* 初始化私有数据 */
    memset(priv, 0, priv_sizes[type]);
    
    /* 根据类型初始化 */
    switch (type) {
        case FILTER_TYPE_COMPLEMENTARY: {
            complementary_priv_t *p = (complementary_priv_t *)priv;
            p->alpha = COMP_ALPHA_DEFAULT;
            f->update = complementary_update;
            f->reset = complementary_reset;
            f->set_param = complementary_set_param;
            break;
        }
        case FILTER_TYPE_LPF: {
            lpf_priv_t *p = (lpf_priv_t *)priv;
            p->cutoff_freq = LPF_CUTOFF_DEFAULT;
            p->alpha = 0.0f;
            p->pitch = p->roll = p->yaw = 0.0f;
            p->prev_pitch = p->prev_roll = p->prev_yaw = 0.0f;
            f->update = lpf_update;
            f->reset = lpf_reset;
            f->set_param = lpf_set_param;
            break;
        }
        case FILTER_TYPE_ESKF: {
            eskf_priv_t *p = (eskf_priv_t *)priv;
            p->q0 = 1.0f; /* q0 = 1 */
            p->Q_angle = EKF_Q_ANGLE_DEFAULT;
            p->Q_bias = EKF_Q_BIAS_DEFAULT;
            p->R_measure = EKF_R_MEASURE_DEFAULT;
            p->bias_limit_dps = EKF_BIAS_LIMIT_DEFAULT;
            p->update_count = 0;
            for (int i = 0; i < 6; i++) p->P[i][i] = 1.0f;
            f->update = eskf_update;
            f->reset = eskf_reset;
            f->set_param = eskf_set_param;
            break;
        }
        case FILTER_TYPE_LKF: {
            lkf_priv_t *p = (lkf_priv_t *)priv;
            p->Q_angle = EKF_Q_ANGLE_DEFAULT;
            p->Q_bias = EKF_Q_BIAS_DEFAULT;
            p->R_measure = EKF_R_MEASURE_DEFAULT;
            for (int i = 0; i < LKF_STATE_SIZE; i++) p->P[i][i] = 1.0f;
            f->update = lkf_update;
            f->reset = lkf_reset;
            f->set_param = lkf_set_param;
            break;
        }
        case FILTER_TYPE_MAHONY: {
            mahony_priv_t *p = (mahony_priv_t *)priv;
            p->q0 = 1.0f;
            p->kp_base = MAHONY_KP_DEFAULT;
            p->ki_base = MAHONY_KI_DEFAULT;
            p->kp_eff = MAHONY_KP_DEFAULT;
            p->ki_eff = MAHONY_KI_DEFAULT;
            p->acc_gate_low  = 0.9f;
            p->acc_gate_high = 1.1f;
            p->gyro_sat_thresh = 212.0f;   /* 0.85 * 250dps FS */
            p->bias_limit = 20.0f;
            p->static_gyro_th = 2.0f;
            p->static_acc_th  = 0.05f;
            f->update = mahony_update;
            f->reset = mahony_reset;
            f->set_param = mahony_set_param;
            break;
        }
        case FILTER_TYPE_MADGWICK: {
            madgwick_priv_t *p = (madgwick_priv_t *)priv;
            p->q0 = 1.0f;
            p->beta = MADGWICK_BETA_DEFAULT;
            f->update = madgwick_update;
            f->reset = madgwick_reset;
            f->set_param = madgwick_set_param;
            break;
        }
        case FILTER_TYPE_KF: {
            kf_priv_t *p = (kf_priv_t *)priv;
            p->Q_angle   = KF_Q_ANGLE_DEFAULT;
            p->Q_bias    = KF_Q_BIAS_DEFAULT;
            p->R_measure = KF_R_MEASURE_DEFAULT;
            p->R_zupt    = KF_R_ZUPT_DEFAULT;  /* 默认禁用 ZUPT */
            p->angle_min = KF_ANGLE_MIN_DEFAULT;
            p->angle_max = KF_ANGLE_MAX_DEFAULT;
#if defined(BSP_MATHACL_KF_HW)
            p->use_hw    = 1;
#else
            p->use_hw    = 0;
#endif
            for (int axis = 0; axis < KF_AXIS_COUNT; axis++) {
                p->P[axis][0][0] = 1.0f;
                p->P[axis][1][1] = 1.0f;
            }
            f->update = kf_update;
            f->reset  = kf_reset;
            f->set_param = kf_set_param;
            break;
        }
        default:
            return NULL;
    }
    
    f->type = type;
    f->degrade = FILTER_DEGRADE_NONE;
    f->priv = priv;
    f->is_static = 1;
    f->destroy = static_destroy_noop; /* 静态分配使用空操作销毁函数，调用安全 */
    f->safety_config = (filter_safety_config_t)FILTER_SAFETY_DEFAULT;

    return f;
}

/* ============================================================
 * 数值安全保护
 * ============================================================ */

/**
 * @brief 验证输出有效性
 */
int filter_validate_output(const filter_output_t *out) {
    if (!out) return 0;

    /* 检查角度 NaN/Inf */
    if (isnan(out->pitch) || isinf(out->pitch) ||
        isnan(out->roll)  || isinf(out->roll)  ||
        isnan(out->yaw)   || isinf(out->yaw)) {
        return 0;
    }

    /* 检查角度范围 */
    if (out->pitch < -180.0f || out->pitch > 180.0f ||
        out->roll  < -180.0f || out->roll  > 180.0f ||
        out->yaw   < -180.0f || out->yaw   > 180.0f) {
        return 0;
    }

    /* 检查四元数 NaN/Inf 与范数（EKF/Mahony/Madgwick 使用） */
    if (isnan(out->q0) || isinf(out->q0) ||
        isnan(out->q1) || isinf(out->q1) ||
        isnan(out->q2) || isinf(out->q2) ||
        isnan(out->q3) || isinf(out->q3)) {
        return 0;
    }
    float q_norm = mathacl_sqrtf(out->q0*out->q0 + out->q1*out->q1 + out->q2*out->q2 + out->q3*out->q3);
    if (q_norm < 0.5f || q_norm > 2.0f) {
        return 0;
    }

    return 1;
}

filter_error_t filter_update_checked(filter_t *f,
                                     const filter_input_t *in,
                                     filter_output_t *out)
{
    if (f == NULL || f->update == NULL || f->priv == NULL ||
        in == NULL || out == NULL) {
        FILTER_REPORT_ERROR(FILTER_ERR_NOT_INITIALIZED,
                            "Filter instance or update argument is invalid");
        return FILTER_ERR_NOT_INITIALIZED;
    }

    if (!isfinite(in->ax) || !isfinite(in->ay) || !isfinite(in->az) ||
        !isfinite(in->gx) || !isfinite(in->gy) || !isfinite(in->gz) ||
        !isfinite(in->dt) || in->dt <= 0.0) {
        FILTER_REPORT_ERROR(FILTER_ERR_INVALID_INPUT,
                            "Filter input contains NaN/Inf or invalid dt");
        return FILTER_ERR_INVALID_INPUT;
    }

    filter_output_t candidate = {
        .q0 = 1.0f,
        .q1 = 0.0f,
        .q2 = 0.0f,
        .q3 = 0.0f,
    };
    f->update(f, in, &candidate);

    if (!filter_validate_output(&candidate)) {
        FILTER_REPORT_ERROR(FILTER_ERR_NUMERICAL,
                            "Filter produced an invalid output");
        return FILTER_ERR_NUMERICAL;
    }

    *out = candidate;
    return FILTER_OK;
}

int filter_supports_param(filter_type_t type, filter_param_t param)
{
    switch (type) {
        case FILTER_TYPE_COMPLEMENTARY:
            return param == FILTER_PARAM_ALPHA;
        case FILTER_TYPE_LPF:
            return param == FILTER_PARAM_CUTOFF_FREQ;
        case FILTER_TYPE_ESKF:
            return param == FILTER_PARAM_Q_ANGLE ||
                   param == FILTER_PARAM_Q_BIAS ||
                   param == FILTER_PARAM_R_MEASURE ||
                   param == FILTER_PARAM_BIAS_LIMIT_DPS;
        case FILTER_TYPE_LKF:
            return param == FILTER_PARAM_Q_ANGLE ||
                   param == FILTER_PARAM_Q_BIAS ||
                   param == FILTER_PARAM_R_MEASURE;
        case FILTER_TYPE_MAHONY:
            return param == FILTER_PARAM_KP || param == FILTER_PARAM_KI;
        case FILTER_TYPE_MADGWICK:
            return param == FILTER_PARAM_KP;
        case FILTER_TYPE_KF:
            return param == FILTER_PARAM_KF_Q_ANGLE ||
                   param == FILTER_PARAM_KF_Q_BIAS ||
                   param == FILTER_PARAM_KF_R_MEASURE ||
                   param == FILTER_PARAM_KF_R_ZUPT;
        default:
            return 0;
    }
}

filter_error_t filter_set_param_checked(filter_t *f,
                                        filter_param_t param,
                                        float value)
{
    if (f == NULL || f->set_param == NULL || f->priv == NULL) {
        FILTER_REPORT_ERROR(FILTER_ERR_NOT_INITIALIZED,
                            "Filter instance cannot accept parameters");
        return FILTER_ERR_NOT_INITIALIZED;
    }
    if (param < 0 || param >= FILTER_PARAM_COUNT ||
        !filter_supports_param(f->type, param) ||
        validate_filter_param(param, value) != 0) {
        FILTER_REPORT_ERROR(FILTER_ERR_INVALID_PARAM,
                            "Parameter is invalid or unsupported by filter");
        return FILTER_ERR_INVALID_PARAM;
    }
    f->set_param(f, param, value);
    return FILTER_OK;
}

/**
 * @brief 四元数归一化（公开 API）
 *
 * 委托给内部统一实现
 */
void filter_normalize_quaternion(float *q0, float *q1, float *q2, float *q3) {
    if (!q0 || !q1 || !q2 || !q3) return;
    normalize_quaternion_internal(q0, q1, q2, q3);
}

void filter_regularize_covariance(float P[][7], int size, float factor) {
    if (!P || size <= 0) return;
    
    /* 确保对角线元素为正 */
    for (int i = 0; i < size; i++) {
        if (P[i][i] < factor) {
            P[i][i] = factor;
        }
    }
    
    /* 限制非对角线元素（防止数值爆炸） */
    for (int i = 0; i < size; i++) {
        for (int j = 0; j < size; j++) {
            if (i != j) {
                float max_val = mathacl_sqrtf(P[i][i] * P[j][j]);
                if (fabsf(P[i][j]) > max_val) {
                    P[i][j] = (P[i][j] > 0) ? max_val : -max_val;
                }
            }
        }
    }
}

void filter_set_safety_config(filter_t *f, const filter_safety_config_t *config) {
    if (!f || !config) return;
    f->safety_config = *config;
}

/* ============================================================
 * 硬件加速配置接口
 * ============================================================ */

void filter_kf_set_hw(filter_t *f, int enable)
{
    if (!f || f->type != FILTER_TYPE_KF || !f->priv) {
        return;
    }
#if defined(BSP_MATHACL_KF_HW)
    kf_priv_t *p = (kf_priv_t *)f->priv;
    p->use_hw = (enable != 0) ? 1U : 0U;
#else
    (void)enable;
#endif
}

int filter_kf_get_hw(const filter_t *f)
{
    if (!f || f->type != FILTER_TYPE_KF || !f->priv) {
        return 0;
    }
#if defined(BSP_MATHACL_KF_HW)
    const kf_priv_t *p = (const kf_priv_t *)f->priv;
    return (int)p->use_hw;
#else
    return 0;
#endif
}

void filter_eskf_set_hw(filter_t *f, int enable)
{
    if (!f || f->type != FILTER_TYPE_ESKF || !f->priv) {
        return;
    }
    (void)enable;  /* ESKF 暂不使用硬件加速 */
}

int filter_eskf_get_hw(const filter_t *f)
{
    (void)f;
    return 0;  /* ESKF 暂不使用硬件加速 */
}

/* ============================================================
 * 诊断信息接口
 * ============================================================ */

int filter_eskf_get_diag(const filter_t *f, filter_eskf_diag_t *diag)
{
    if (!f || !diag || f->type != FILTER_TYPE_ESKF || !f->priv) {
        if (diag) memset(diag, 0, sizeof(*diag));
        return -1;
    }
    const eskf_priv_t *p = (const eskf_priv_t *)f->priv;
    memset(diag, 0, sizeof(*diag));
    diag->bias_x = p->bias_x;
    diag->bias_y = p->bias_y;
    diag->bias_z = p->bias_z;
    diag->p00 = p->P[0][0];
    diag->p11 = p->P[1][1];
    diag->p22 = p->P[2][2];
    diag->p33 = p->P[3][3];
    diag->p44 = p->P[4][4];
    diag->p55 = p->P[5][5];
    return 0;
}

int filter_kf_get_diag(const filter_t *f, filter_kf_diag_t *diag)
{
    if (!f || !diag || f->type != FILTER_TYPE_KF || !f->priv) {
        if (diag) memset(diag, 0, sizeof(*diag));
        return -1;
    }
    const kf_priv_t *p = (const kf_priv_t *)f->priv;
    memset(diag, 0, sizeof(*diag));
    diag->bias_x = p->x[0][1];
    diag->bias_y = p->x[1][1];
    diag->bias_z = p->x[2][1];
    diag->p00_x  = p->P[0][0][0];
    diag->p00_y  = p->P[1][0][0];
    diag->p00_z  = p->P[2][0][0];
    diag->p11_x  = p->P[0][1][1];
    diag->p11_y  = p->P[1][1][1];
    diag->p11_z  = p->P[2][1][1];
    return 0;
}
