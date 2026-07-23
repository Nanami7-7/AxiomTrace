/**
 * @file    filter_config.h
 * @brief   滤波器参数配置 - 包含来源标注、范围验证和退化策略
 *
 * 设计原则：
 *   1. 所有参数标明来源（论文/经验值/传感器手册）
 *   2. 所有参数有有效范围和默认值
 *   3. 提供退化策略（传感器数据质量差时的降级方案）
 *   4. 支持运行时参数验证
 *
 * 参数来源标注格式：
 *   [来源类型] 来源名称 | 推荐值 | 说明
 *   来源类型：PAPER(论文) / EMPIRICAL(经验值) / DATASHEET(数据手册) / TUNED(调优)
 */

#ifndef FILTER_CONFIG_H
#define FILTER_CONFIG_H

#include "filter.h"

/* ============================================================
 * 调试详细输出开关
 * 0 = 关闭（默认，省 Flash，避免拉入浮点 printf 库）
 * 1 = 开启（输出滤波器中间值，用于调试）
 * ============================================================ */
#ifndef FILTER_DEBUG_VERBOSE
#define FILTER_DEBUG_VERBOSE 0
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 参数来源类型枚举
 * ============================================================ */
typedef enum {
    PARAM_SOURCE_PAPER = 0,     /**< 论文/学术研究 */
    PARAM_SOURCE_EMPIRICAL,     /**< 经验值/工程实践 */
    PARAM_SOURCE_DATASHEET,     /**< 传感器数据手册 */
    PARAM_SOURCE_TUNED,         /**< 实际调优结果 */
    PARAM_SOURCE_COUNT
} param_source_t;

/* ============================================================
 * 参数描述结构体
 * ============================================================ */
typedef struct {
    filter_param_t  param;          /**< 参数枚举 */
    float           default_value;  /**< 默认值 */
    float           min_value;      /**< 最小有效值 */
    float           max_value;      /**< 最大有效值 */
    param_source_t  source_type;    /**< 来源类型 */
    const char     *source_name;    /**< 来源名称/论文标题 */
    const char     *source_detail;  /**< 来源详细说明 */
    const char     *unit;           /**< 单位 */
} filter_param_desc_t;

/* ============================================================
 * 退化策略枚举
 * ============================================================ */
typedef enum {
    DEGRADE_NONE = 0,           /**< 无退化（正常运行） */
    DEGRADE_STATIC_ONLY,        /**< 仅静态模式（禁用动态补偿） */
    DEGRADE_GYRO_ONLY,          /**< 仅陀螺仪（禁用ACC修正） */
    DEGRADE_ACC_ONLY,           /**< 仅加速度计（禁用GYRO积分） */
    DEGRADE_HOLD_LAST,          /**< 保持上次输出（冻结） */
    DEGRADE_COUNT
} degrade_mode_t;

/* ============================================================
 * 传感器质量状态
 * ============================================================ */
typedef enum {
    SENSOR_QUALITY_GOOD = 0,    /**< 数据质量好 */
    SENSOR_QUALITY_NOISY,       /**< 数据有噪声 */
    SENSOR_QUALITY_SATURATED,   /**< 数据饱和/超限 */
    SENSOR_QUALITY_INVALID,     /**< 数据无效（NaN/Inf） */
    SENSOR_QUALITY_COUNT
} sensor_quality_t;

/* ============================================================
 * 退化策略配置
 * ============================================================ */
typedef struct {
    degrade_mode_t  mode;               /**< 退化模式 */
    float           acc_threshold_low;  /**< ACC幅值下限（g） */
    float           acc_threshold_high; /**< ACC幅值上限（g） */
    float           gyro_threshold;     /**< GYRO幅值阈值（dps） */
    float           variance_threshold; /**< 方差阈值 */
    const char     *description;        /**< 退化策略说明 */
} degrade_config_t;

/* ============================================================
 * 滤波器预设配置
 * ============================================================ */
typedef enum {
    FILTER_PRESET_DEFAULT = 0,      /**< 默认配置（平衡） */
    FILTER_PRESET_HIGH_PRECISION,   /**< 高精度（低噪声） */
    FILTER_PRESET_FAST_RESPONSE,    /**< 快速响应（低延迟） */
    FILTER_PRESET_ROBUST,           /**< 鲁棒（抗干扰） */
    FILTER_PRESET_LOW_POWER,        /**< 低功耗（简化计算） */
    FILTER_PRESET_COUNT
} filter_preset_t;

/* ============================================================
 * 配置常量定义
 * ============================================================ */

/* --- 互补滤波器参数 --- */
/* [PAPER] Mahony et al., 2008, "Nonlinear complementary filters on the special orthogonal group"
 * 推荐α = 0.98（对应截止频率约0.1Hz）
 * 范围：0.90~0.99
 * 说明：α越大，越信任陀螺仪，响应快但漂移大
 *       α越小，越信任加速度计，稳定但响应慢 */
#define COMP_ALPHA_DEFAULT      0.98f
#define COMP_ALPHA_MIN          0.90f
#define COMP_ALPHA_MAX          0.99f

/* double 版本（供 bsp_lsm6dsr.c 中 double 精度的互补滤波 fallback 使用）
 * 注意：float 版 COMP_ALPHA_DEFAULT(0.98f) 精度较低，不可用于 double 上下文
 * ⚠️ COMP_ALPHA_INV_DB 独立定义为 0.02，不可用 (1.0 - COMP_ALPHA_DEFAULT_DB)
 *    替代——浮点运算 1.0-0.98=0.020000000000000018 ≠ 字面量 0.02 */
#define COMP_ALPHA_DEFAULT_DB   0.98
#define COMP_ALPHA_INV_DB       0.02

/* --- LPF参数 --- */
/* [EMPIRICAL] 一阶低通滤波器截止频率
 * 推荐：5~20Hz（取决于应用）
 * 说明：截止频率越低，滤波越强，延迟越大
 *       截止频率越高，滤波越弱，延迟越小 */
#define LPF_CUTOFF_DEFAULT      10.0f   /* Hz */
#define LPF_CUTOFF_MIN          1.0f    /* Hz */
#define LPF_CUTOFF_MAX          50.0f   /* Hz */

/* --- ESKF / EKF 噪声参数 (ESKF 复用 EKF 默认值) --- */
#define EKF_Q_ANGLE_DEFAULT    0.001f
#define EKF_Q_ANGLE_MIN        0.0001f
#define EKF_Q_ANGLE_MAX        0.1f

#define EKF_Q_BIAS_DEFAULT     0.002f
#define EKF_Q_BIAS_MIN         0.0001f
#define EKF_Q_BIAS_MAX         0.1f

#define EKF_R_MEASURE_DEFAULT  0.001f
#define EKF_R_MEASURE_MIN      0.0001f
#define EKF_R_MEASURE_MAX      1.0f

/* 偏置限幅 (dps) - LSM6DSR 规格 */
#define EKF_BIAS_LIMIT_DEFAULT  20.0f
#define EKF_BIAS_LIMIT_MIN      5.0f
#define EKF_BIAS_LIMIT_MAX      50.0f

/* [已废弃] EKF Chi-squared - ESKF 不使用 */
#define EKF_CHI2_THRESHOLD_DEFAULT  11.34f
#define EKF_CHI2_THRESHOLD_MIN      5.0f
#define EKF_CHI2_THRESHOLD_MAX      20.0f

/* [已废弃] EKF R 自适应 - ESKF 不使用 */
#define EKF_R_ADAPT_ENABLE_DEFAULT  0
#define EKF_R_ADAPT_FACTOR_MIN      0.1f
#define EKF_R_ADAPT_FACTOR_MAX      10.0f

/* ============================================================
 * 自适应 Mahony 参数 (非线性互补滤波)
 * ============================================================ */
#define MAHONY_KP_DEFAULT      10.0f
#define MAHONY_KP_MIN          0.1f
#define MAHONY_KP_MAX          10.0f

#define MAHONY_KI_DEFAULT      0.3f
#define MAHONY_KI_MIN          0.0f
#define MAHONY_KI_MAX          0.5f

/* 物理约束阈值 */
#define MAHONY_ACC_GATE_LOW    0.9f    /* g, ACC 幅值下限 */
#define MAHONY_ACC_GATE_HIGH   1.1f    /* g, ACC 幅值上限 */
#define MAHONY_GYRO_SAT_TH     212.0f  /* dps, 0.85 * FS250 */
#define MAHONY_BIAS_LIMIT      20.0f   /* dps, LSM6DSR bias stability */
#define MAHONY_STATIC_GYRO_TH  2.0f    /* dps, 静态判定 */
#define MAHONY_STATIC_ACC_TH   0.05f   /* g, 静态加速度偏差判定 */
#define MAHONY_KP_STATIC_BOOST 1.5f    /* 静态时 kp 增幅 */
#define MAHONY_KP_DYN_ACC_FACTOR 0.7f  /* 动态时 kp ACC 质量衰减因子 */
#define MAHONY_KI_DYN1_FACTOR  0.1f    /* gyro 5-20dps 时 ki 衰减 */
#define MAHONY_KI_DYN2_FACTOR  0.01f   /* gyro >20dps 时 ki 衰减 */
#define MAHONY_KI_ACC_POOR     0.5f    /* ACC 质量差时 ki 额外衰减 */
#define MAHONY_INTEGRAL_LIMIT  0.5f    /* rad/s, 积分抗饱和限幅 */

/* KF 静止偏置伪测量的单轴创新门限 (dps)。超过门限视为真实转动。 */
#define KF_ZUPT_AXIS_GATE      0.5f

/* [PAPER] Madgwick, S.O.H., 2010, "An efficient orientation filter for inertial and
 *          inertial/magnetic sensor arrays"
 * 论文推荐β = 0.033（IMU）或 0.041（MARG）
 * 工程优化：β = 0.5（快速收敛，适合嵌入式实时场景）
 *
 * β: 梯度下降步长
 *   - 来源：论文第3.6节 "Filter gains"
 *   - 定义：陀螺仪测量误差的四元数导数幅值
 *   - 范围：0.001~0.5
 *   - 说明：越大收敛越快，但噪声越大；
 *          论文实验：β=0.033（IMU）时静态RMS误差<0.6°；
 *          工程实践：β=0.5时1000次迭代内可收敛到0.04°以内 */
#define MADGWICK_BETA_DEFAULT  0.5f
#define MADGWICK_BETA_MIN      0.001f
#define MADGWICK_BETA_MAX      0.5f

/* ============================================================
 * KF (纯卡尔曼滤波器) 参数
 * 状态: x = [angle, bias] × 3轴 (每轴独立 2状态标量 KF)
 * 参考:
 *   [1] Kris Winer MPU-6050 KF 实现 (标量 KF 经典参考)
 *   [2] ST LSM6DSR Datasheet (DocID029673 Rev 2)
 *   [3] Starlino, "A Guide To Using IMU in Embedded Applications"
 *
 * Q_angle: 角度过程噪声 (deg²/s)
 *   - 来源: [TUNED] 角加速度建模误差
 *   - LSM6DSR 角加速度噪声: 0.01 dps/√Hz × √100Hz ≈ 0.1 dps
 *   - 转换为角度: (0.1 dps × 0.01s)² = 1e-6 deg², 放大100x → 0.0001
 *   - 工程推荐: 0.0001 ~ 0.01
 *   - [KFTUNE] 静态扫描最优 Set 7: 0.003 (增大以加快角度跟踪)
 *
 * Q_bias: 偏置过程噪声 (dps²/s)
 *   - 来源: [DATASHEET] LSM6DSR 偏置稳定性 ±10 dps
 *   - 偏置随机游走方差: (10 dps/3600s)² ≈ 7.7e-6 dps²/s
 *   - 工程推荐: 0.0001 ~ 0.01
 *   - [KFTUNE] 静态扫描最优 Set 7: 0.001 (减小以稳定 bias 估计)
 *
 * R_measure: 测量噪声 (deg²)
 *   - 来源: [DATASHEET+TUNED] LSM6DSR 加速度计噪声密度 0.08 mg/√Hz
 *   - @100Hz 理论方差: (0.08e-3 × √100 × 9.80665 × 180/π)² ≈ 0.002 deg²
 *   - 动态含线性加速度干扰, 放大 15x → 0.03
 *   - 工程推荐: 0.001 ~ 0.5, 默认 0.03
 *
 * 使用时在 task_imu.c 中调用 set_param 覆盖默认值 */
#define KF_Q_ANGLE_DEFAULT     0.003f
#define KF_Q_ANGLE_MIN         0.0001f
#define KF_Q_ANGLE_MAX         0.01f

#define KF_Q_BIAS_DEFAULT      0.001f
#define KF_Q_BIAS_MIN          0.0001f
#define KF_Q_BIAS_MAX          0.01f

#define KF_R_MEASURE_DEFAULT   0.03f
#define KF_R_MEASURE_MIN       0.001f
#define KF_R_MEASURE_MAX       0.5f

#define KF_ANGLE_MIN_DEFAULT  -180.0f
#define KF_ANGLE_MAX_DEFAULT   180.0f

/* KF ZUPT (零速更新) 参数
 * R_zupt: ZUPT 伪测量噪声 (dps²)
 *   - 来源: [TUNED] LSM6DSR静止时gyro噪声RMS≈0.2dps → 方差≈0.04
 *   - 设置 1e6 等效禁用 ZUPT（无穷大噪声 = 零增益）
 *   - 启用典型值: 0.04 (对应 0.2dps RMS)
 *   - 启用激进值: 0.01 (对应 0.1dps RMS, 收敛更快, 更易受噪声影响)
 *   - 范围: 0.0001 ~ 1e6
 *
 * 轴门控: 使用 KF_ZUPT_AXIS_GATE (0.5dps),
 *         创新量幅值超过门控时不更新该轴偏置 */
#define KF_R_ZUPT_DEFAULT      1e6f     /* 默认禁用 ZUPT */
#define KF_R_ZUPT_MIN          0.0001f  /* 最小噪声方差 */
#define KF_R_ZUPT_MAX          1e6f     /* 最大 (等效禁用) */

/* ============================================================
 * 退化策略默认配置
 * ============================================================ */

/* ACC幅值检查阈值（单位：g） */
#define DEGRADE_ACC_LOW         0.5f    /* 幅值过小（可能自由落体） */
#define DEGRADE_ACC_HIGH        2.0f    /* 幅值过大（可能碰撞） */

/* GYRO幅值检查阈值（单位：dps） */
#define DEGRADE_GYRO_THRESHOLD  400.0f  /* 接近满量程（±500dps） */

/* 方差阈值（用于静止检测） */
#define DEGRADE_VARIANCE_THRESH 0.01f   /* 加速度方差阈值（g²） */

/* ============================================================
 * API函数声明
 * ============================================================ */

/**
 * @brief 获取参数描述表
 * @param type  滤波器类型
 * @param count 输出参数数量
 * @return 参数描述数组指针（静态存储，无需释放）
 */
const filter_param_desc_t* filter_config_get_params(filter_type_t type, int *count);

/**
 * @brief 获取参数默认值
 * @param type  滤波器类型
 * @param param 参数枚举
 * @return 默认值
 */
float filter_config_get_default(filter_type_t type, filter_param_t param);

/**
 * @brief 验证参数值是否在有效范围内
 * @param type  滤波器类型
 * @param param 参数枚举
 * @param value 参数值
 * @return 1=有效, 0=无效
 */
int filter_config_validate(filter_type_t type, filter_param_t param, float value);

/**
 * @brief 钳位参数值到有效范围
 * @param type  滤波器类型
 * @param param 参数枚举
 * @param value 输入值
 * @return 钳位后的值
 */
float filter_config_clamp(filter_type_t type, filter_param_t param, float value);

/**
 * @brief 获取退化策略配置
 * @param mode  退化模式
 * @return 退化配置指针（静态存储，无需释放）
 */
const degrade_config_t* filter_config_get_degrade(degrade_mode_t mode);

/**
 * @brief 评估传感器数据质量
 * @param ax, ay, az  加速度（g）
 * @param gx, gy, gz  角速度（dps）
 * @return 传感器质量状态
 */
sensor_quality_t filter_config_assess_quality(float ax, float ay, float az,
                                               float gx, float gy, float gz);

/**
 * @brief 根据传感器质量确定退化模式
 * @param acc_quality   加速度计质量
 * @param gyro_quality  陀螺仪质量
 * @return 推荐的退化模式
 */
degrade_mode_t filter_config_select_degrade(sensor_quality_t acc_quality,
                                            sensor_quality_t gyro_quality);

/**
 * @brief 应用预设配置到滤波器
 * @param f       滤波器实例
 * @param preset  预设类型
 */
void filter_config_apply_preset(filter_t *f, filter_preset_t preset);

/**
 * @brief 获取预设配置名称
 * @param preset  预设类型
 * @return 名称字符串
 */
const char* filter_config_preset_name(filter_preset_t preset);

/**
 * @brief 打印参数配置信息（调试用）
 * @param type  滤波器类型
 */
void filter_config_print(filter_type_t type);

#ifdef __cplusplus
}
#endif

#endif /* FILTER_CONFIG_H */
