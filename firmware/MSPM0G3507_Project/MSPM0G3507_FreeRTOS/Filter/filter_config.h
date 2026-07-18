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

/* --- EKF参数 --- */
/* [PAPER] Simon, D., 2006, "Optimal State Estimation: Kalman, H Infinity, and Nonlinear Approaches"
 * [TUNED] 基于LSM6DSR传感器特性调优
 *
 * Q_angle: 过程噪声-角度（弧度²/s）
 *   - 来源：传感器陀螺仪噪声密度 × 采样时间
 *   - LSM6DSR陀螺仪噪声密度：0.01 dps/√Hz
 *   - 采样率100Hz时：0.01 × √100 = 0.1 dps = 0.0017 rad/s
 *   - 推荐值：0.001~0.01
 *
 * Q_bias: 过程噪声-偏置（dps²/s）
 *   - 来源：陀螺仪偏置稳定性
 *   - LSM6DSR偏置稳定性：±10 dps
 *   - 推荐值：0.001~0.01
 *
 * R_measure: 测量噪声（g²）
 *   - 来源：加速度计噪声密度
 *   - LSM6DSR加速度计噪声密度：0.08 mg/√Hz
 *   - 采样率100Hz时：0.08 × √100 = 0.8 mg = 0.0008 g
 *   - 推荐值：0.01~0.1

 * Bias bounds for EKF (防止偏置发散)
 *   - LSM6DSR陀螺仪偏置稳定性：±10 dps
 *   - 设置偏置幅值上限防止发散
 *   - 单位：dps (陀螺仪原始单位，内部会转为 rad/s)
 *   - 限制范围：0.0 ~ 50.0 dps
 *   - 默认 20.0 dps (约 0.35 rad/s，远大于正常偏置，防止误限制正常偏置) */
#define EKF_Q_ANGLE_DEFAULT    0.001f
#define EKF_Q_ANGLE_MIN        0.0001f
#define EKF_Q_ANGLE_MAX        0.1f

#define EKF_Q_BIAS_DEFAULT     0.002f
#define EKF_Q_BIAS_MIN         0.0001f
#define EKF_Q_BIAS_MAX         0.1f

#define EKF_R_MEASURE_DEFAULT  0.001f
#define EKF_R_MEASURE_MIN      0.0001f
#define EKF_R_MEASURE_MAX      1.0f

/* EKF偏置幅值限制 (dps) - 防止偏置发散 */
#define EKF_BIAS_LIMIT_DEFAULT  20.0f   /* dps */
#define EKF_BIAS_LIMIT_MIN      5.0f    /* dps */
#define EKF_BIAS_LIMIT_MAX      50.0f   /* dps */

/* EKF Chi-squared 门限 (自由度=3, 99%置信度 ≈ 11.34) */
#define EKF_CHI2_THRESHOLD_DEFAULT  11.34f
#define EKF_CHI2_THRESHOLD_MIN      5.0f
#define EKF_CHI2_THRESHOLD_MAX      20.0f

/* EKF 加速度计测量更新门控（有意收紧，区别于 DEGRADE_ACC_LOW/HIGH）
 * [TUNED] 收紧至 [0.7, 1.3]g 覆盖正常静态/缓动态场景（含安装倾角），
 * 拒绝机动时线性加速度（典型 >0.3g 偏差），仅做陀螺积分预测。
 * ⚠️ 此门限与 DEGRADE_ACC_LOW(0.5)/DEGRADE_ACC_HIGH(2.0) 语义不同：
 *    DEGRADE_* 用于传感器质量评估/退化模式选择；
 *    EKF_ACC_GATE_* 用于 EKF 测量更新门控（更严格）。 */
#define EKF_ACC_GATE_LOW        0.7f
#define EKF_ACC_GATE_HIGH       1.3f

/* 动态 R 适配参数 */
#define EKF_R_ADAPT_ENABLE_DEFAULT  0  /* 默认关闭，动态适配在实测中会导致不稳定 */
#define EKF_R_ADAPT_FACTOR_MIN      0.1f  /* R 最小缩放因子 */
#define EKF_R_ADAPT_FACTOR_MAX      10.0f /* R 最大缩放因子 */

/* [B3] 协方差更新形式选择
 * 1 = Joseph 形式 P=(I-KH)P(I-KH)^T+KRK^T（数值稳定，运算量大）
 * 0 = 标准形式 P=(I-KH)P（运算量小33%，已有对称化+对角保护兜底）
 * 默认 0：标准形式。EKF 已有定期正则化(每100帧)，Joseph 的稳定性优势
 * 在 float 精度下不显著，省 33% 运算量对实时性更有价值。 */
#ifndef EKF_USE_JOSEPH_FORM
#define EKF_USE_JOSEPH_FORM         0
#endif

/* [PAPER] Mahony et al., 2008, "Nonlinear complementary filters on the special orthogonal group"
 * 论文推荐kp = 1.0, ki = 0.3
 * 工程优化：kp = 10.0（快速收敛，适合嵌入式实时场景）
 *
 * kp: 比例增益
 *   - 来源：论文范围0.5~10.0，工程实践取高端
 *   - 范围：0.1~10.0
 *   - 说明：越大响应越快，收敛越快；过大可能导致振荡
 *
 * ki: 积分增益
 *   - 来源：论文默认值
 *   - 范围：0.0~0.5
 *   - 说明：用于估计陀螺仪偏置，过大可能导致超调 */
#define MAHONY_KP_DEFAULT      10.0f
#define MAHONY_KP_MIN          0.1f
#define MAHONY_KP_MAX          10.0f

#define MAHONY_KI_DEFAULT      0.3f
#define MAHONY_KI_MIN          0.0f
#define MAHONY_KI_MAX          0.5f

/* ============================================================
 * 方案 D: 抗机动干扰
 *   D1. 自适应加速度门限: 基于陀螺角速度动态扩展门限
 *   D2. Huber 鲁棒估计: 减大创新量权重, 抑制离群值
 * ============================================================ */

/* D1. 自适应加速度门限
 * 转动越快, 允许越大的离心加速度偏差
 * gate_expand = min(gyro_mag * GYRO_FACTOR, MAX_EXPAND)
 * 有效门限: [LOW - expand, HIGH + expand] */
#ifndef EKF_ACC_GATE_ADAPTIVE
#define EKF_ACC_GATE_ADAPTIVE      1  /* 1=启用自适应门限, 0=固定门限 */
#endif
#define EKF_ACC_GATE_GYRO_FACTOR   0.002f  /* dps→g 转换系数 */
#define EKF_ACC_GATE_MAX_EXPAND    0.3f    /* 最大门限扩展(g) */

/* D2. Huber 鲁棒估计
 * 创新量范数超过阈值时, 按阈值/范数比例缩放
 * 阈值取卡方门限的平方根 */
#ifndef EKF_HUBER_WEIGHT
#define EKF_HUBER_WEIGHT           1  /* 1=启用Huber加权, 0=标准卡尔曼 */
#endif

/* ============================================================
 * 方案 H: 浮点运算精度提升
 *   H1. double 累加: 协方差矩阵乘法用double累加, 结果转回float
 *   H2. 条件归一化: 仅在范数偏离超过阈值时归一化
 * ============================================================ */

/* H1. double 累加
 * float 累加在7×7矩阵运算中易丢精度, double累加可显著减少漂移
 * 代价: 每次矩阵乘法增加~10%计算时间 */
#ifndef EKF_DOUBLE_ACCUM
#define EKF_DOUBLE_ACCUM           1  /* 1=double累加, 0=float累加 */
#endif

/* H2. 条件归一化
 * 仅在四元数范数偏离1.0超过阈值时归一化
 * 多数帧范数接近1, 可省~80%归一化计算 */
#ifndef EKF_COND_NORMALIZE
#define EKF_COND_NORMALIZE         1  /* 1=条件归一化, 0=每帧归一化 */
#endif
#define EKF_NORM_TOL               0.001f  /* 范数偏差容限 */
#define EKF_NORM_FORCE_INTERVAL    10      /* 强制归一化周期(帧) */

/* ============================================================
 * 方案 N: NIS (归一化创新平方) 监测
 *   滑动窗口统计 NIS = y^T * S^-1 * y, 验证滤波器调参正确性
 *   NIS 应落在 chi2 置信区间 [low, high] 内, 否则调参有误
 *   纯监测, 不改变滤波器行为, 零风险
 * ============================================================ */
#ifndef EKF_NIS_MONITOR
#define EKF_NIS_MONITOR            1  /* 1=启用NIS监测, 0=关闭 (与Keil工程定义一致) */
#endif
#define EKF_NIS_WINDOW_SIZE        32  /* NIS 滑动窗口大小 */
/* chi2 95% 置信区间 (自由度=3):
 *   下限 chi2(0.025, 3) ≈ 0.216
 *   上限 chi2(0.975, 3) ≈ 9.348
 * 注: 若 task_imu.c 中 chi2_threshold 已改为 20.0,
 *     NIS 上限可适当放宽至 EKF_NIS_CHI2_HIGH */
#define EKF_NIS_CHI2_LOW           0.216f
#define EKF_NIS_CHI2_HIGH          9.348f

/* ============================================================
 * 方案 R: R 自适应 (保守版本)
 *   基于 NIS 滑动平均, 慢速有界调整 R_measure
 *   仅调 R, 不调 Q (Q 调整风险更高, 列入路线图)
 *   自适应因子有界 [R_MIN, R_MAX], 步长极小, 防发散
 * ============================================================ */
#ifndef EKF_R_ADAPTIVE
#define EKF_R_ADAPTIVE             0  /* 1=启用R自适应, 0=固定R */
#endif
/* R 自适应依赖 NIS 监测数据, 若未启用 NIS 则强制关闭 R 自适应 */
#if EKF_R_ADAPTIVE && !EKF_NIS_MONITOR
#error "EKF_R_ADAPTIVE requires EKF_NIS_MONITOR to be enabled"
#endif
#define EKF_R_ADAPT_STEP           0.02f   /* 每窗口调整步长 */
#define EKF_R_NIS_FACTOR_MIN       0.5f    /* NIS R 缩放因子下限 */
#define EKF_R_NIS_FACTOR_MAX       2.0f    /* NIS R 缩放因子上限 */
/* NIS 持续超上限 → R 偏小(滤波器过于自信) → 增大 R
 * NIS 持续低于下限 → R 偏大(滤波器不够自信) → 减小 R */

/* ============================================================
 * 方案 E3: 机动检测状态机 + 分级 Q/R
 *   基于陀螺角速度幅值 + 加速度幅值偏差, 分类 4 级机动等级
 *   - 等级 0(静止):    gyro<2dps,  acc_err<0.05g  → 信任ACC
 *   - 等级 1(准静态):  gyro<10dps, acc_err<0.1g   → 正常
 *   - 等级 2(缓动态):  gyro<50dps, acc_err<0.2g   → 降ACC权重
 *   - 等级 3(高动态):  其他                → 近纯陀螺
 *
 *   两阶段使能:
 *     EKF_MANEUVER_DETECT=1   仅分类(纯诊断, 零行为变更)
 *     EKF_MANEUVER_QR_ADAPT=1 分级Q/R缩放(改变行为, 需验证)
 * ============================================================ */
#ifndef EKF_MANEUVER_DETECT
#define EKF_MANEUVER_DETECT         1  /* 1=启动机动等级分类, 0=关闭 (与Keil工程定义一致) */
#endif
#ifndef EKF_MANEUVER_QR_ADAPT
#define EKF_MANEUVER_QR_ADAPT       0  /* 1=分级Q/R缩放, 0=不调参 */
#endif
/* QR_ADAPT 依赖 DETECT */
#if EKF_MANEUVER_QR_ADAPT && !EKF_MANEUVER_DETECT
#error "EKF_MANEUVER_QR_ADAPT requires EKF_MANEUVER_DETECT to be enabled"
#endif

/* 机动等级阈值 (基于 LSM6DSR FS=250dps, ACC FS=4g) */
#define EKF_MANEUVER_GYRO_STATIC    2.0f    /* dps, 静态: 3σ≈0.3, 留6×余量 */
/* 滞回阈值: 进入静止用严格值, 保持静止用宽松值, 避免边界抖动导致 ZUPT 间歇触发
 * 注意: STATIC_EXIT 从 3.0 降到 1.5, 防止慢转(2-3dps)被误判为静止 */
#define EKF_MANEUVER_GYRO_STATIC_ENTER  1.0f   /* dps, 非静止→静止: 严格 */
#define EKF_MANEUVER_GYRO_STATIC_EXIT   1.5f   /* dps, 静止→非静止: 收紧防慢转误判 */
#define EKF_MANEUVER_GYRO_QUASI     10.0f   /* dps, 准静态 */
#define EKF_MANEUVER_GYRO_SLOW      50.0f   /* dps, 缓动态 */
#define EKF_MANEUVER_ACC_STATIC     0.05f   /* g, 静态 acc 偏差 */
#define EKF_MANEUVER_ACC_QUASI      0.1f    /* g, 准静态 acc 偏差 */
#define EKF_MANEUVER_ACC_SLOW       0.2f    /* g, 缓动态 acc 偏差 */

/* 分级 Q/R 缩放因子 (等级 0/1/2/3) */
#define EKF_MANEUVER_Q_SCALE_0      0.3f    /* 静态: 降低Q, 平滑输出 */
#define EKF_MANEUVER_Q_SCALE_1      0.7f    /* 准静态 */
#define EKF_MANEUVER_Q_SCALE_2      1.0f     /* 缓动态: 标称Q */
#define EKF_MANEUVER_Q_SCALE_3      2.0f     /* 高动态: 增大Q, 加快跟踪 */
#define EKF_MANEUVER_R_SCALE_0      1.0f     /* 静态: 信任ACC */
#define EKF_MANEUVER_R_SCALE_1      1.0f     /* 准静态 */
#define EKF_MANEUVER_R_SCALE_2      2.0f     /* 缓动态: 降ACC权重 */
#define EKF_MANEUVER_R_SCALE_3      5.0f     /* 高动态: 近纯陀螺 */

/* ============================================================
 * ZUPT (Zero-velocity Update) - 零速更新
 * ============================================================
 * 原理: 静止时陀螺读数=零偏, 直接注入偏置观测
 * 解决: yaw在无磁力计时不可观测, EKF无法估计bz
 *       ZUPT提供独立的偏置观测源, 不依赖加速度计
 *
 * EKF_ZUPT_ENABLE: 1=启用ZUPT, 0=关闭
 * EKF_ZUPT_RATE: 静止时偏置更新速率 (EMA系数)
 *   - 0.02: 慢速, 平滑 (~50帧收敛)
 *   - 0.05: 中速 (~20帧收敛, 推荐)
 *   - 0.1:  快速 (~10帧收敛, 噪声略大)
 *
 * 依赖: EKF_MANEUVER_DETECT=1 (需要maneuver_level判断静止) */
#ifndef EKF_ZUPT_ENABLE
#define EKF_ZUPT_ENABLE             1
#endif

#ifndef EKF_ZUPT_RATE
#define EKF_ZUPT_RATE               0.04f
#endif

/* ZUPT 偏置更新限幅: 防止单帧噪声大幅改变偏置状态 */
#ifndef EKF_ZUPT_MAX_DELTA
#define EKF_ZUPT_MAX_DELTA           0.01f   /* dps, 单帧偏置最大变化量 */
#endif

/* ZUPT 滑动窗口: 静止时累积 N 帧陀螺读数取均值作为偏置观测
 * 降噪比 = √N, 8帧降噪 ≈ 2.83倍
 * 仅在窗口满时更新偏置, 避免每帧噪声直接注入 */
#ifndef EKF_ZUPT_WINDOW_SIZE
#define EKF_ZUPT_WINDOW_SIZE         8
#endif

/* ZUPT 单轴门控: 即使 maneuver_level=0, 若该轴窗口均值非零则不更新该轴偏置
 * 根因: 慢转(2-3dps)可能被滞回逻辑误判为静止(level=0), 但窗口均值会暴露真实角速度
 *       若用真实角速度更新偏置, 会造成 gz_eff = gz - bias ≈ 0, yaw 不增长
 * 阈值依据:
 *   - 8帧窗口均值降噪 √8≈2.83 倍, 单帧 3σ≈0.3dps → 窗口 3σ≈0.1dps
 *   - 0.5dps 是 5σ 余量, 能可靠区分慢转(≥1dps)与噪声
 *   - 远低于 EKF_MANEUVER_GYRO_STATIC_EXIT(1.5dps), 形成第二层防护 */
#ifndef EKF_ZUPT_AXIS_GATE
#define EKF_ZUPT_AXIS_GATE           0.5f   /* dps, 单轴窗口均值门控阈值 */
#endif

/* D1: 动态偏置跟踪参数
 * 运动时不完全冻结偏置, 改为门控弱 EMA 跟踪
 * 仅当陀螺读数接近当前偏置估计时才更新, 防止运动数据污染
 * 衰减因子越小越保守, 0=完全冻结(等效原逻辑)
 *
 * 设计依据:
 *   level=1(准静态, gyro<10dps): RATE×0.1, DELTA×0.1, 门控2dps
 *   level=2(缓动态, gyro<50dps): RATE×0.05, DELTA×0.05, 门控5dps
 *   level=3(高动态): 不更新, 完全冻结
 *
 * 最坏情况验证(level=2, 持续50dps转弯):
 *   |50-bias|≈50 > gate=5 → 不更新, 零污染 */
#ifndef EKF_ZUPT_DYN_RATE_FACTOR_L1
#define EKF_ZUPT_DYN_RATE_FACTOR_L1   0.1f   /* 准静态: ZUPT_RATE × 0.1 = 0.004 */
#endif
#ifndef EKF_ZUPT_DYN_RATE_FACTOR_L2
#define EKF_ZUPT_DYN_RATE_FACTOR_L2   0.05f  /* 缓动态: ZUPT_RATE × 0.05 = 0.002 */
#endif
#ifndef EKF_ZUPT_DYN_DELTA_FACTOR_L1
#define EKF_ZUPT_DYN_DELTA_FACTOR_L1  0.1f   /* 准静态限幅: MAX_DELTA × 0.1 = 0.001 dps */
#endif
#ifndef EKF_ZUPT_DYN_DELTA_FACTOR_L2
#define EKF_ZUPT_DYN_DELTA_FACTOR_L2  0.05f  /* 缓动态限幅: MAX_DELTA × 0.05 = 0.0005 dps */
#endif
#ifndef EKF_ZUPT_DYN_GATE_L1
#define EKF_ZUPT_DYN_GATE_L1          2.0f   /* 准静态门控阈值 (dps) */
#endif
#ifndef EKF_ZUPT_DYN_GATE_L2
#define EKF_ZUPT_DYN_GATE_L2          5.0f   /* 缓动态门控阈值 (dps) */
#endif

#if EKF_ZUPT_ENABLE && !EKF_MANEUVER_DETECT
#error "EKF_ZUPT_ENABLE requires EKF_MANEUVER_DETECT to be enabled"
#endif

/* ============================================================
 * 方案 GYRO_RESIDUAL: 陀螺仪残留抑制 (v2 - maneuver_level 状态转换触发)
 *
 * 根因: LSM6DSR 在 FS=250dps 高速转动后突然停止时, 陀螺仪机械结构会有
 *       振荡残留, 持续输出非零角速度 (典型 5-10 dps, 持续 1-3 秒).
 *       EKF 盲信陀螺仪继续积分 yaw, 导致 yaw 反向漂移.
 *
 * [v1 缺陷] 原 "acc静止 + gyro非零" 双检测无法区分:
 *   - 陀螺仪残留 (停转后): acc≈1g, gyro=5-10dps
 *   - 原地旋转 (真实转动): acc≈1g (无线性加速度!), gyro=12-38dps
 *   → v1 误杀原地旋转, yaw 仅增长 -1.443° (应 ~90°)
 *
 * [v2 修复] 改用 maneuver_level 状态转换触发:
 *   仅在 maneuver_level 从高(≥2, 缓动态/高动态)降到低(≤1, 准静态/静止)
 *   的转换窗口内激活残留抑制. 真实转动时 level 保持 ≥2, 不触发.
 *
 * 触发条件 (全部满足):
 *   1. prev_maneuver_level >= 3  (上一帧处于高动态, >50dps)
 *   2. curr_maneuver_level <= 1  (当前帧降到准静态/静止)
 *   3. gyro_mag > GYRO_THRESHOLD  (陀螺仪仍输出非零, 确认残留存在)
 *
 * 抑制窗口:
 *   - 触发后 WINDOW 帧内施加指数衰减
 *   - 若窗口内 gyro_mag < GYRO_THRESHOLD, 提前结束 (残留消散)
 *   - 真实转动期间 maneuver_level 保持 >= 2, 不触发
 *
 * 衰减公式: decay = exp(-active_frames / DECAY_TAU)
 *   DECAY_TAU=10 帧 → 100ms 衰减到 37%, 200ms 到 13%, 300ms 到 5%
 *
 * 与 ZUPT 的关系:
 *   - ZUPT 在 level=0 时更新偏置, 解决静态偏置漂移
 *   - GYRO_RESIDUAL 在 level 高→低转换时抑制残留角速度
 *   - 二者互补: ZUPT 处理偏置, GYRO_RESIDUAL 处理残留 */
#ifndef EKF_GYRO_RESIDUAL_ENABLE
#define EKF_GYRO_RESIDUAL_ENABLE    1
#endif
#ifndef EKF_GYRO_RESIDUAL_GYRO_TH
#define EKF_GYRO_RESIDUAL_GYRO_TH   1.5f    /* dps, 陀螺仪残留判定阈值 */
#endif
#ifndef EKF_GYRO_RESIDUAL_WINDOW
#define EKF_GYRO_RESIDUAL_WINDOW    100     /* 抑制窗口帧数 (1.0s @100Hz) */
#endif
#ifndef EKF_GYRO_RESIDUAL_DECAY_TAU
#define EKF_GYRO_RESIDUAL_DECAY_TAU 10.0f   /* 衰减时间常数 (帧), ~100ms 衰减到 37% */
#endif

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
 * 轴门控: 重用 EKF_ZUPT_AXIS_GATE(0.5dps),
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
