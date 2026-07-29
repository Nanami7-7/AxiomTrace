/**
 * @file    project_config.h
 * @brief   项目硬件与软件配置集中定义
 * @note    所有引脚映射、外设实例和硬件参数在此集中定义。
 *          更换硬件或引脚时优先修改本文件，无需改动驱动实现。
 *          引脚编号来源于 ti_msp_dl_config.h（由 SysConfig 生成）。
 */
#ifndef PROJECT_CONFIG_H
#define PROJECT_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

/* ======================== 头文件 ======================== */
#include "hal_common.h"
#include "ti_msp_dl_config.h"

/* ==========================================================================
 * IMU filter parameter defaults and tuning
 * Consolidated from filter_param_defaults.h and filter_tuning.h.
 * ========================================================================== */

#define FILTER_COMP_ALPHA_DEFAULT       (0.98f)
/** 互补滤波系数 alpha 的最小值，float 类型。 */
#define FILTER_COMP_ALPHA_MIN           (0.90f)
/** 互补滤系数 alpha 的最大值，float 类型。 */
#define FILTER_COMP_ALPHA_MAX           (0.99f)
/** 互补滤默认系数的 double 精度备用值。 */
#define FILTER_COMP_ALPHA_DEFAULT_DB    (0.98)
/** 互补滤系数的补量（1-alpha），double 精度备用值。 */
#define FILTER_COMP_ALPHA_INV_DB        (0.02)

/** KF 角度过程噪声参数。 */
#define FILTER_KF_Q_ANGLE_DEFAULT       (0.003f)
/** KF 角度过程噪声参数。 */
#define FILTER_KF_Q_ANGLE_MIN           (0.0001f)
/** KF 角度过程噪声参数。 */
#define FILTER_KF_Q_ANGLE_MAX           (0.01f)
/** KF 陀螺仪偏置过程噪声参数。 */
#define FILTER_KF_Q_BIAS_DEFAULT        (0.001f)
/** KF 陀螺仪偏置过程噪声参数。 */
#define FILTER_KF_Q_BIAS_MIN            (0.0001f)
/** KF 陀螺仪偏置过程噪声参数。 */
#define FILTER_KF_Q_BIAS_MAX            (0.01f)
/** KF 测量噪声参数。 */
#define FILTER_KF_R_MEASURE_DEFAULT     (0.03f)
/** KF 测量噪声参数。 */
#define FILTER_KF_R_MEASURE_MIN         (0.001f)
/** KF 测量噪声参数。 */
#define FILTER_KF_R_MEASURE_MAX         (0.5f)
/** KF 角度状态的允许范围。 */
#define FILTER_KF_ANGLE_MIN_DEFAULT     (-180.0f)
/** KF 角度状态的允许范围。 */
#define FILTER_KF_ANGLE_MAX_DEFAULT     (180.0f)
/** KF 零速更新（ZUPT）测量噪声配置。 */
#define FILTER_KF_R_ZUPT_DEFAULT        (1e6f)
/** KF 零速更新（ZUPT）测量噪声配置。 */
#define FILTER_KF_R_ZUPT_MIN            (0.0001f)
/** KF 零速更新（ZUPT）测量噪声配置。 */
#define FILTER_KF_R_ZUPT_MAX            (1e6f)

/** KF 协方差矩阵的数值上限。 */
#define FILTER_KF_COVARIANCE_MAX         (1.0e6f)
/** KF 协方差矩阵对称性检查的容差。 */
#define FILTER_KF_COVARIANCE_SYMMETRY_EPS (1.0e-5f)
/** KF 协方差矩阵行列式检查的最小容差。 */
#define FILTER_KF_COVARIANCE_DET_EPS     (1.0e-6f)

#ifndef FILTER_DEBUG_VERBOSE
#define FILTER_DEBUG_VERBOSE 0
#endif


/* ============================================================
 * 配置常量定义
 * ============================================================ */

/* --- 互补滤波器参数 --- */
/* [PAPER] Mahony et al., 2008, "Nonlinear complementary filters on the special orthogonal group"
 * 推荐α = 0.98（对应截止频率约0.1Hz）
 * 范围：0.90~0.99
 * 说明：α越大，越信任陀螺仪，响应快但漂移大
 *       α越小，越信任加速度计，稳定但响应慢 */
#define COMP_ALPHA_DEFAULT      FILTER_COMP_ALPHA_DEFAULT
#define COMP_ALPHA_MIN          FILTER_COMP_ALPHA_MIN
#define COMP_ALPHA_MAX          FILTER_COMP_ALPHA_MAX

/* double 版本（供 bsp_lsm6dsr.c 中 double 精度的互补滤波 fallback 使用）
 * 注意：float 版 COMP_ALPHA_DEFAULT(0.98f) 精度较低，不可用于 double 上下文
 * ⚠️ COMP_ALPHA_INV_DB 独立定义为 0.02，不可用 (1.0 - COMP_ALPHA_DEFAULT_DB)
 *    替代——浮点运算 1.0-0.98=0.020000000000000018 ≠ 字面量 0.02 */
#define COMP_ALPHA_DEFAULT_DB   FILTER_COMP_ALPHA_DEFAULT_DB
#define COMP_ALPHA_INV_DB       FILTER_COMP_ALPHA_INV_DB

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
#define EKF_NIS_MONITOR            1  /* 1=启用NIS监测, 0=关闭 */
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
#define EKF_MANEUVER_DETECT         1  /* 1=启动机动等级分类, 0=关闭 */
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
 * EKF 与 MATHACL 数值稳定性参数
 * 用于浮点和 Q24 硬件路径的边界保护。
 * ============================================================ */
/** EKF 协方差矩阵的最大允许值。 */
#define FILTER_EKF_COV_MAX_LIMIT       (1.0e6f)
/** EKF 协方差矩阵的最小允许值。 */
#define FILTER_EKF_COV_MIN_LIMIT       (1.0e-10f)
/** EKF 矩阵求逆时的行列式最小阈值。 */
#define FILTER_EKF_INV_DET_EPS         (1.0e-12f)
/** EKF 归一化运算的最小阈值。 */
#define FILTER_EKF_NORM_EPS            (1.0e-10f)
/** EKF 硬件除法结果的限幅值。 */
#define FILTER_EKF_HW_DIV_CLAMP_MAX    (120.0f)
/** KF MATHACL Q24 协方差状态的硬件限幅值。 */
#define FILTER_KF_HW_P_CLAMP_MAX       (120.0f)
/** KF MATHACL Q24 创新协方差的最小阈值。 */
#define FILTER_KF_HW_S_MIN             (1.0e-10f)


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
#define KF_Q_ANGLE_DEFAULT     FILTER_KF_Q_ANGLE_DEFAULT
#define KF_Q_ANGLE_MIN         FILTER_KF_Q_ANGLE_MIN
#define KF_Q_ANGLE_MAX         FILTER_KF_Q_ANGLE_MAX

#define KF_Q_BIAS_DEFAULT      FILTER_KF_Q_BIAS_DEFAULT
#define KF_Q_BIAS_MIN          FILTER_KF_Q_BIAS_MIN
#define KF_Q_BIAS_MAX          FILTER_KF_Q_BIAS_MAX

#define KF_R_MEASURE_DEFAULT   FILTER_KF_R_MEASURE_DEFAULT
#define KF_R_MEASURE_MIN       FILTER_KF_R_MEASURE_MIN
#define KF_R_MEASURE_MAX       FILTER_KF_R_MEASURE_MAX

#define KF_ANGLE_MIN_DEFAULT  FILTER_KF_ANGLE_MIN_DEFAULT
#define KF_ANGLE_MAX_DEFAULT   FILTER_KF_ANGLE_MAX_DEFAULT

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
#define KF_R_ZUPT_DEFAULT      FILTER_KF_R_ZUPT_DEFAULT     /* 默认禁用 ZUPT */
#define KF_R_ZUPT_MIN          FILTER_KF_R_ZUPT_MIN  /* 最小噪声方差 */
#define KF_R_ZUPT_MAX          FILTER_KF_R_ZUPT_MAX     /* 最大 (等效禁用) */

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

/* ==========================================================================
 * Key and button configuration
 * Runtime key descriptor objects remain in app_main.c; this section contains
 * only compile-time values and pin mapping.
 * ========================================================================== */
#ifndef PRJ_KEY_ENABLE
#define PRJ_KEY_ENABLE               (1U)
#endif
#define PRJ_KEY_COUNT                (2U)
#define PRJ_KEY_SCAN_PERIOD_MS       (10U)
#define PRJ_KEY_BUTTON_DEBOUNCE_MS   (20U)
#define PRJ_KEY_BUTTON_LONG_PRESS_MS (800U)
#define PRJ_KEY_BUTTON_MAX_HOLD_MS   (5000U)
#define PRJ_KEY_SWITCH_DEBOUNCE_MS   (20U)
/** 位置环按键启动后，四路累计编码器计数的 UART0 DMA 发送周期。 */
#define PRJ_ENCODER_TELEMETRY_PERIOD_MS (200U)

#define PRJ_KEY_FORWARD_RPM          (200.0f)
#define PRJ_KEY_FORWARD_TIMEOUT_MS   (3000U)
#define PRJ_KEY_TURN_TARGET_DEG      (90.0f)
#define PRJ_KEY_TURN_CRUISE_RPM      (120.0f)
#define PRJ_KEY_TURN_TIMEOUT_MS      (5000U)
#define PRJ_KEY_TURN_SETTLE_MS       (100U)
/** STUCK事件位置环目标距离，单位：米。 */
#define PRJ_KEY_STUCK_DISTANCE_M    (2.0f)
/** STUCK事件位置环巡航速度，单位：RPM。 */
#define PRJ_KEY_STUCK_CRUISE_RPM    (120.0f)
/** STUCK事件位置环最大运行时间，单位：毫秒。 */
#define PRJ_KEY_STUCK_TIMEOUT_MS    (10000U)

#ifndef TASK_PRIO_KEY
#define TASK_PRIO_KEY                (3U)
#endif
#ifndef TASK_STACK_KEY
#define TASK_STACK_KEY               (256U)
#endif

#ifndef KEY_key_PORT
#define KEY_key_PORT                 (GPIOA)
#endif
#ifndef KEY_key_PIN
#define KEY_key_PIN                  (DL_GPIO_PIN_7)
#endif
#ifndef KEY_key_IOMUX
#define KEY_key_IOMUX                (IOMUX_PINCM14)
#endif
#ifndef KEY_switch_PORT
#define KEY_switch_PORT              (GPIOB)
#endif
#ifndef KEY_switch_PIN
#define KEY_switch_PIN               (DL_GPIO_PIN_3)
#endif
#ifndef KEY_switch_IOMUX
#define KEY_switch_IOMUX             (IOMUX_PINCM16)
#endif


/* ================================================================
 * IMU filter backend feature switches
 *
 * The Keil target may override these macros in its preprocessor
 * definitions.  The normal target keeps only KF; the factory target
 * enables all backends for diagnostic comparison.  Enum values remain
 * stable even when a backend is compiled out.
 * ================================================================ */
#ifndef PRJ_FILTER_ENABLE_COMPLEMENTARY
#define PRJ_FILTER_ENABLE_COMPLEMENTARY (0U)
#endif
#ifndef PRJ_FILTER_ENABLE_LPF
#define PRJ_FILTER_ENABLE_LPF           (0U)
#endif
#ifndef PRJ_FILTER_ENABLE_EKF
#define PRJ_FILTER_ENABLE_EKF           (0U)
#endif
#ifndef PRJ_FILTER_ENABLE_LKF
#define PRJ_FILTER_ENABLE_LKF           (0U)
#endif
#ifndef PRJ_FILTER_ENABLE_MAHONY
#define PRJ_FILTER_ENABLE_MAHONY        (0U)
#endif
#ifndef PRJ_FILTER_ENABLE_MADGWICK
#define PRJ_FILTER_ENABLE_MADGWICK      (0U)
#endif
#ifndef PRJ_FILTER_ENABLE_KF
#define PRJ_FILTER_ENABLE_KF            (1U)
#endif

#if ((PRJ_FILTER_ENABLE_COMPLEMENTARY != 0U) && (PRJ_FILTER_ENABLE_COMPLEMENTARY != 1U)) || \
    ((PRJ_FILTER_ENABLE_LPF           != 0U) && (PRJ_FILTER_ENABLE_LPF           != 1U)) || \
    ((PRJ_FILTER_ENABLE_EKF           != 0U) && (PRJ_FILTER_ENABLE_EKF           != 1U)) || \
    ((PRJ_FILTER_ENABLE_LKF           != 0U) && (PRJ_FILTER_ENABLE_LKF           != 1U)) || \
    ((PRJ_FILTER_ENABLE_MAHONY        != 0U) && (PRJ_FILTER_ENABLE_MAHONY        != 1U)) || \
    ((PRJ_FILTER_ENABLE_MADGWICK      != 0U) && (PRJ_FILTER_ENABLE_MADGWICK      != 1U)) || \
    ((PRJ_FILTER_ENABLE_KF            != 0U) && (PRJ_FILTER_ENABLE_KF            != 1U))
#error "PRJ_FILTER_ENABLE_* macros must be 0 or 1"
#endif

#if (PRJ_FILTER_ENABLE_COMPLEMENTARY == 0U) && \
    (PRJ_FILTER_ENABLE_LPF == 0U) && \
    (PRJ_FILTER_ENABLE_EKF == 0U) && \
    (PRJ_FILTER_ENABLE_LKF == 0U) && \
    (PRJ_FILTER_ENABLE_MAHONY == 0U) && \
    (PRJ_FILTER_ENABLE_MADGWICK == 0U) && \
    (PRJ_FILTER_ENABLE_KF == 0U)
#error "At least one IMU filter backend must be enabled"
#endif

/* The current IMU application uses KF for its static instance. */
#if (PRJ_FILTER_ENABLE_KF == 0U)
#error "This application target requires PRJ_FILTER_ENABLE_KF=1"
#endif

/* ================================================================
 * Project identity canonical configuration
 *
 * PRJ_* is the canonical configuration used by this firmware project.
 * The standalone project_version.h compatibility header has been removed.
 * Only the protocol/board/motor PROJECT_* aliases remain below for legacy
 * modules and external tooling; new firmware code must use the PRJ_* names.
 * ================================================================ */
/** 固件版本主版号。 */
#define PRJ_VERSION_MAJOR        (0U)
/** 固件版本次版号。 */
#define PRJ_VERSION_MINOR        (1U)
/** 固件版本修订号。 */
#define PRJ_VERSION_PATCH        (0U)
/** 固件版本字符串。 */
#define PRJ_VERSION_STRING       "0.1.0"
/** 固件与上位机文本协议的兼容级别。 */
#define PRJ_PROTOCOL_VERSION     (1U)
/**
 * AB 板通信已取消 HEARTBEAT 保活和协议 watchdog。
 * 0：不因通信间隔自动进入 LINK_LOST；1：启用旧版 watchdog 机制。
 */
#ifndef PRJ_PROTOCOL_WATCHDOG_ENABLE
#define PRJ_PROTOCOL_WATCHDOG_ENABLE (0U)
#endif
/** 当前硬件板卡名称。 */
#define PRJ_BOARD_NAME           "MSPM0G3507"
/** 当前电机驱动器名称。 */
#define PRJ_MOTOR_DRIVER_NAME    "DRV8870"

#ifndef PROJECT_PROTOCOL_VERSION
#define PROJECT_PROTOCOL_VERSION     PRJ_PROTOCOL_VERSION
#endif
#ifndef PROJECT_BOARD_NAME
#define PROJECT_BOARD_NAME           PRJ_BOARD_NAME
#endif
#ifndef PROJECT_MOTOR_DRIVER_NAME
#define PROJECT_MOTOR_DRIVER_NAME    PRJ_MOTOR_DRIVER_NAME
#endif

/* ================================================================
 * LED 配置
 * 由 SysConfig 配置 GPIOA.27。
 * ================================================================ */

/** LED端口(GPIOA) */
#define PRJ_LED_PORT            HAL_GPIO_PORT_A
/** LED寮曡剼缂栧彿 */
#define PRJ_LED_PIN             LED_A27_PIN

/* ================================================================
 * 调试 UART 配置
 * SysConfig：UART0，TX=PA10/PINCM21，RX=PA11/PINCM22。
 * 默认波特率为 115200，时钟源为 40 MHz。
 * ================================================================ */

/** 璋冭瘯涓插彛HAL瀹炰緥 */
#define PRJ_UART_DEBUG_ID       HAL_UART_DEBUG

/* ================================================================
 * VOFA+ communication limits
 * Keep protocol safety limits in the project configuration so they
 * can be reviewed and rolled back independently of the VOFA module.
 * ================================================================ */
/** Maximum absolute PID parameter accepted by VOFA commands. */
#define PRJ_VOFA_PID_PARAM_MAX    (100.0f)
/** Maximum absolute target speed accepted by VOFA commands (RPM). */
#define PRJ_VOFA_TARGET_RPM_MAX   (800.0f)

/* ================================================================
 *  电机驱动选择与统一业务命令
 *
 *  分层关系:
 *    Application -> bsp_motor(统一门面) -> 芯片后端 -> HAL
 *
 *  默认使用 DRV8870；TB6612 是备用硬件后端。上层统一使用
 *  -PRJ_MOTOR_COMMAND_MAX ~ +PRJ_MOTOR_COMMAND_MAX，切换后端不改变
 *  PID、模型辨识和通信协议中的命令量纲。
 * ================================================================ */
#define PRJ_MOTOR_DRIVER_DRV8870    (1U)
#define PRJ_MOTOR_DRIVER_TB6612     (2U)

#ifndef PRJ_MOTOR_DRIVER
#define PRJ_MOTOR_DRIVER            PRJ_MOTOR_DRIVER_DRV8870
#endif

#if (PRJ_MOTOR_DRIVER != PRJ_MOTOR_DRIVER_DRV8870) && \
    (PRJ_MOTOR_DRIVER != PRJ_MOTOR_DRIVER_TB6612)
#error "PRJ_MOTOR_DRIVER must select DRV8870 or TB6612"
#endif

/** 后端无关的有符号业务命令最大绝对值。 */
#define PRJ_MOTOR_COMMAND_MAX       (500U)

/** 电机安装方向；正命令必须统一对应车体前进方向。 */
#define PRJ_MOTOR_A_INSTALL_DIR_SIGN  (-1)
#define PRJ_MOTOR_B_INSTALL_DIR_SIGN  (-1)
#define PRJ_MOTOR_C_INSTALL_DIR_SIGN  (+1)
#define PRJ_MOTOR_D_INSTALL_DIR_SIGN  (+1)

/*
 * 当前底盘只有 M1/A 和 M4/D 是驱动轮，M2/B 与 M3/C 是万向轮。
 * 位序固定为 A/M1=bit0、B/M2=bit1、C/M3=bit2、D/M4=bit3。
 * 以后更换底盘时只需修改下面的配置，不要在控制算法中散落硬编码。
 */
#define PRJ_MOTOR_A_MOTION_ACTIVE    (1U)
#define PRJ_MOTOR_B_MOTION_ACTIVE    (1U)
#define PRJ_MOTOR_C_MOTION_ACTIVE    (1U)
#define PRJ_MOTOR_D_MOTION_ACTIVE    (1U)
#define PRJ_MOTION_MOTOR_ACTIVE_MASK \
    ((PRJ_MOTOR_A_MOTION_ACTIVE << 0) | \
     (PRJ_MOTOR_B_MOTION_ACTIVE << 1) | \
     (PRJ_MOTOR_C_MOTION_ACTIVE << 2) | \
     (PRJ_MOTOR_D_MOTION_ACTIVE << 3))
#define PRJ_MOTION_MOTOR_ACTIVE_COUNT \
    (PRJ_MOTOR_A_MOTION_ACTIVE + PRJ_MOTOR_B_MOTION_ACTIVE + \
     PRJ_MOTOR_C_MOTION_ACTIVE + PRJ_MOTOR_D_MOTION_ACTIVE)

/*
 * 输出开放和车体反馈是两个独立概念：
 * - PRJ_MOTION_MOTOR_ACTIVE_MASK：哪些 A/B/C/D 通道允许输出 PWM；
 * - PRJ_MOTION_FEEDBACK_MASK：哪些 A/B/C/D 编码器参与车体 RPM、位置和里程计算。
 *
 * 当前只有 M1/A、M4/D 接了实际电机和编码器，因此不让备用接口 B/C
 * 的零值或悬空输入稀释车体反馈；以后将电机换到 B/C 时，只需把对应
 * FEEDBACK_ACTIVE 改为 1，不需要修改控制算法。
 */
#define PRJ_MOTOR_A_FEEDBACK_ACTIVE  (1U)
#define PRJ_MOTOR_B_FEEDBACK_ACTIVE  (0U)
#define PRJ_MOTOR_C_FEEDBACK_ACTIVE  (0U)
#define PRJ_MOTOR_D_FEEDBACK_ACTIVE  (1U)
#define PRJ_MOTION_FEEDBACK_MASK \
    ((PRJ_MOTOR_A_FEEDBACK_ACTIVE << 0) | \
     (PRJ_MOTOR_B_FEEDBACK_ACTIVE << 1) | \
     (PRJ_MOTOR_C_FEEDBACK_ACTIVE << 2) | \
     (PRJ_MOTOR_D_FEEDBACK_ACTIVE << 3))
#define PRJ_MOTION_FEEDBACK_COUNT \
    (PRJ_MOTOR_A_FEEDBACK_ACTIVE + PRJ_MOTOR_B_FEEDBACK_ACTIVE + \
     PRJ_MOTOR_C_FEEDBACK_ACTIVE + PRJ_MOTOR_D_FEEDBACK_ACTIVE)

#if (PRJ_MOTION_MOTOR_ACTIVE_COUNT == 0U)
#error "At least one driven motor must be enabled"
#endif

/* ================================================================
 *  TB6612 备用后端配置
 *
 *  当前 Config/empty.syscfg 是 DRV8870 默认板级配置，不包含以下8个
 *  方向GPIO。选择 TB6612 前必须在独立 SysConfig 板级配置中恢复
 *  MOTOR_AIN1~MOTOR_DIN2，并重新生成 ti_msp_dl_config.c/h。
 * ================================================================ */
#define PRJ_TB6612_PWM_TIMER        HAL_TIMER_PWM_MOTOR
#define PRJ_TB6612_PWM_CLK_HZ       ((unsigned long)(PWM_MOTOR_INST_CLK_FREQ))
#define PRJ_TB6612_PWM_PERIOD       (1000U)
#define PRJ_TB6612_POWER_STARTUP_MS (1U)

/** 设为1时由软件控制TB6612 STBY；0表示STBY已由硬件固定为有效。 */
#ifndef PRJ_TB6612_STANDBY_CONTROL_ENABLE
#define PRJ_TB6612_STANDBY_CONTROL_ENABLE (0U)
#endif

/** 仅供编译门面判断板级引脚与STBY配置是否完整；禁止手工强制置1。 */
#define PRJ_TB6612_BOARD_CONFIG_AVAILABLE (0U)

#if (PRJ_MOTOR_DRIVER == PRJ_MOTOR_DRIVER_TB6612)
#if !defined(MOTOR_AIN1_PIN) || !defined(MOTOR_AIN2_PIN) || \
    !defined(MOTOR_BIN1_PIN) || !defined(MOTOR_BIN2_PIN) || \
    !defined(MOTOR_CIN1_PIN) || !defined(MOTOR_CIN2_PIN) || \
    !defined(MOTOR_DIN1_PIN) || !defined(MOTOR_DIN2_PIN)
#error "TB6612 selected: restore MOTOR_AIN1..MOTOR_DIN2 in SysConfig and regenerate ti_msp_dl_config"
#else

/* TB6612 M1 电机对应的 PWM 通道。 */
#define PRJ_TB6612_A_PWM_CH      (0U) /* M1 / 电机1 */
#define PRJ_TB6612_A_IN1_PORT    HAL_GPIO_PORT_B
#define PRJ_TB6612_A_IN1_PIN     MOTOR_AIN1_PIN  /* PB24 */
#define PRJ_TB6612_A_IN2_PORT    HAL_GPIO_PORT_B
#define PRJ_TB6612_A_IN2_PIN     MOTOR_AIN2_PIN  /* PB20 */

#define PRJ_TB6612_B_PWM_CH      (1U) /* M2 / 电机2 */
#define PRJ_TB6612_B_IN1_PORT    HAL_GPIO_PORT_A
#define PRJ_TB6612_B_IN1_PIN     MOTOR_BIN1_PIN  /* PA24 */
#define PRJ_TB6612_B_IN2_PORT    HAL_GPIO_PORT_A
#define PRJ_TB6612_B_IN2_PIN     MOTOR_BIN2_PIN  /* PA31 */

#define PRJ_TB6612_C_PWM_CH      (2U) /* M3 / 电机3 */
#define PRJ_TB6612_C_IN1_PORT    HAL_GPIO_PORT_A
#define PRJ_TB6612_C_IN1_PIN     MOTOR_CIN1_PIN  /* PA3 */
#define PRJ_TB6612_C_IN2_PORT    HAL_GPIO_PORT_A
#define PRJ_TB6612_C_IN2_PIN     MOTOR_CIN2_PIN  /* PA7 */

#define PRJ_TB6612_D_PWM_CH      (3U) /* M4 / 电机4 */
#define PRJ_TB6612_D_IN1_PORT    HAL_GPIO_PORT_B
#define PRJ_TB6612_D_IN1_PIN     MOTOR_DIN1_PIN  /* PB6 */
#define PRJ_TB6612_D_IN2_PORT    HAL_GPIO_PORT_B
#define PRJ_TB6612_D_IN2_PIN     MOTOR_DIN2_PIN  /* PB7 */

#define PRJ_TB6612_CONFIGS { \
    { PRJ_TB6612_A_PWM_CH, PRJ_TB6612_A_IN1_PORT, PRJ_TB6612_A_IN1_PIN, \
      PRJ_TB6612_A_IN2_PORT, PRJ_TB6612_A_IN2_PIN, \
      PRJ_MOTOR_A_INSTALL_DIR_SIGN }, \
    { PRJ_TB6612_B_PWM_CH, PRJ_TB6612_B_IN1_PORT, PRJ_TB6612_B_IN1_PIN, \
      PRJ_TB6612_B_IN2_PORT, PRJ_TB6612_B_IN2_PIN, \
      PRJ_MOTOR_B_INSTALL_DIR_SIGN }, \
    { PRJ_TB6612_C_PWM_CH, PRJ_TB6612_C_IN1_PORT, PRJ_TB6612_C_IN1_PIN, \
      PRJ_TB6612_C_IN2_PORT, PRJ_TB6612_C_IN2_PIN, \
      PRJ_MOTOR_C_INSTALL_DIR_SIGN }, \
    { PRJ_TB6612_D_PWM_CH, PRJ_TB6612_D_IN1_PORT, PRJ_TB6612_D_IN1_PIN, \
      PRJ_TB6612_D_IN2_PORT, PRJ_TB6612_D_IN2_PIN, \
      PRJ_MOTOR_D_INSTALL_DIR_SIGN }, \
}

#if (PRJ_TB6612_STANDBY_CONTROL_ENABLE != 0U)
#if !defined(PRJ_TB6612_STANDBY_PORT) || \
    !defined(PRJ_TB6612_STANDBY_PIN) || \
    !defined(PRJ_TB6612_STANDBY_ACTIVE_LEVEL)
#error "TB6612 STBY control enabled: define port, pin and active level"
#else
#define PRJ_TB6612_POWER_CONFIG { \
    true, PRJ_TB6612_STANDBY_PORT, PRJ_TB6612_STANDBY_PIN, \
    PRJ_TB6612_STANDBY_ACTIVE_LEVEL \
}
#undef PRJ_TB6612_BOARD_CONFIG_AVAILABLE
#define PRJ_TB6612_BOARD_CONFIG_AVAILABLE (1U)
#endif
#else
#define PRJ_TB6612_POWER_CONFIG { false, HAL_GPIO_PORT_A, 0U, true }
#undef PRJ_TB6612_BOARD_CONFIG_AVAILABLE
#define PRJ_TB6612_BOARD_CONFIG_AVAILABLE (1U)
#endif
#endif /* direction GPIO macros available */
#endif /* selected TB6612 */

/* ================================================================
 * 编码器输入捕获配置
 * SysConfig：TIMG7/TIMA1/TIMG6/TIMG0，四路定时器用于编码器捕获。
 * ================================================================ */

/**
 * 电机与编码器机械参数。
 *
 * PPR定义为编码器A相在电机轴旋转一圈时的完整脉冲周期数；当前捕获逻辑
 * 同时统计A相上升沿和下降沿，因此解码倍频固定为2。减速比用分数表示，
 * 可准确配置20:1、30:1或298:11等非整数标称减速比。
 */
#define PRJ_MOTOR_ENCODER_PPR              (13U)
/* 实测轮胎转一圈约变化368个计数：13 PPR × 2倍频 × 184/13。 */
#define PRJ_MOTOR_GEAR_RATIO_NUMERATOR     (184U)
#define PRJ_MOTOR_GEAR_RATIO_DENOMINATOR   (13U)
#define PRJ_ENCODER_DECODE_MULTIPLIER      (2U)
/* Encoder M/T speed-mode hysteresis and stop timeout. */
#define PRJ_ENCODER_SPEED_MODE_ENTER_M_COUNT  (3U)
#define PRJ_ENCODER_SPEED_MODE_EXIT_M_COUNT   (1U)
#define PRJ_ENCODER_SPEED_MODE_CONFIRM_CYCLES (3U)
#define PRJ_ENCODER_STOP_TIMEOUT_MS           (100U)

#if (PRJ_MOTOR_ENCODER_PPR == 0U)
#error "PRJ_MOTOR_ENCODER_PPR must be greater than zero"
#endif
#if (PRJ_MOTOR_GEAR_RATIO_NUMERATOR == 0U) || \
    (PRJ_MOTOR_GEAR_RATIO_DENOMINATOR == 0U)
#error "Motor gear-ratio numerator and denominator must be greater than zero"
#endif
#if (PRJ_ENCODER_DECODE_MULTIPLIER != 2U)
#error "Current encoder ISR counts both A-phase edges; multiplier must remain 2"
#endif
#if (((PRJ_MOTOR_ENCODER_PPR * PRJ_MOTOR_GEAR_RATIO_NUMERATOR * \
       PRJ_ENCODER_DECODE_MULTIPLIER) % \
      PRJ_MOTOR_GEAR_RATIO_DENOMINATOR) != 0U)
#error "Configured PPR and gear ratio do not produce an integer output-shaft count"
#endif

/** 输出轴每转计数，用于位置和RPM换算。 */
#define PRJ_MOTOR_OUTPUT_PULSES_PER_REV \
    ((PRJ_MOTOR_ENCODER_PPR * PRJ_MOTOR_GEAR_RATIO_NUMERATOR * \
      PRJ_ENCODER_DECODE_MULTIPLIER) / \
     PRJ_MOTOR_GEAR_RATIO_DENOMINATOR)

/** 兼容现有编码器BSP调用。 */
#define PRJ_ENCODER_PULSES_PER_REV  PRJ_MOTOR_OUTPUT_PULSES_PER_REV
/**
 * CC1 周期对应的输出轴周期数。
 *
 * 当前捕获配置为：CC1 只捕获 A 相上升沿，因此一个 CC1->CC1
 * 周期对应 A 相的一个完整脉冲周期，而不是 CC0+CC1 的双边沿数。
 * M 法的位置计数仍使用 PRJ_ENCODER_PULSES_PER_REV（双边沿计数），
 * T 法必须使用本宏，避免低速反馈被缩小约 2 倍。
 */
#define PRJ_ENCODER_PERIODS_PER_OUTPUT_REV \
    ((PRJ_MOTOR_ENCODER_PPR * PRJ_MOTOR_GEAR_RATIO_NUMERATOR) / \
     PRJ_MOTOR_GEAR_RATIO_DENOMINATOR)

#if (PRJ_ENCODER_PERIODS_PER_OUTPUT_REV == 0U)
#error "PRJ_ENCODER_PERIODS_PER_OUTPUT_REV must be greater than zero"
#endif

/** LF 编码器 HAL 实例 */
#define PRJ_ENCODER_LF_TIMER    HAL_TIMER_CAPTURE_LF
/** LB 编码器 HAL 实例 */
#define PRJ_ENCODER_LB_TIMER    HAL_TIMER_CAPTURE_LB
/** RF 编码器 HAL 实例 */
#define PRJ_ENCODER_RF_TIMER    HAL_TIMER_CAPTURE_RF
/** RB 编码器 HAL 实例 */
#define PRJ_ENCODER_RB_TIMER    HAL_TIMER_CAPTURE_RB

/**
 * @brief SysConfig-generated encoder timer ISR mapping.
 * @note  The logical wheel order retains its A-phase capture timer allocation:
 *        LF TIMG7/M3, LB TIMA1/M4, RF TIMG6/M2, RB TIMG0/M1.
 */
#define PRJ_ENCODER_LF_IRQ_HANDLER  M3_INST_IRQHandler
#define PRJ_ENCODER_LB_IRQ_HANDLER  M4_INST_IRQHandler
#define PRJ_ENCODER_RF_IRQ_HANDLER  M2_INST_IRQHandler
#define PRJ_ENCODER_RB_IRQ_HANDLER  M1_INST_IRQHandler
/** 编码器A相端口/引脚(SysConfig捕获复用输入) */
#define PRJ_ENCODER_LF_A_PORT        HAL_GPIO_PORT_A
#define PRJ_ENCODER_LB_A_PORT        HAL_GPIO_PORT_A
#define PRJ_ENCODER_RF_A_PORT        HAL_GPIO_PORT_A
#define PRJ_ENCODER_RB_A_PORT        HAL_GPIO_PORT_A
#define PRJ_ENCODER_LF_A_PIN         GPIO_M3_C0_PIN
#define PRJ_ENCODER_LB_A_PIN         GPIO_M4_C0_PIN
#define PRJ_ENCODER_RF_A_PIN         GPIO_M2_C0_PIN
#define PRJ_ENCODER_RB_A_PIN         GPIO_M1_C0_PIN
/** 编码器B相端口(SysConfig已配置) */
#define PRJ_ENCODER_LF_B_PORT        HAL_GPIO_PORT_A
#define PRJ_ENCODER_LB_B_PORT        HAL_GPIO_PORT_A
#define PRJ_ENCODER_RF_B_PORT        HAL_GPIO_PORT_A
#define PRJ_ENCODER_RB_B_PORT        HAL_GPIO_PORT_A
/** Left-front encoder B phase: PA25 (SysConfig M3_B). */
#define PRJ_ENCODER_LF_B_PIN    ENCODER_M3_B_PIN
/** Left-back encoder B phase: PA4 (SysConfig M4_B). */
#define PRJ_ENCODER_LB_B_PIN    ENCODER_M4_B_PIN
/** Right-front encoder B phase: PA14 (SysConfig M2_B). */
#define PRJ_ENCODER_RF_B_PIN    ENCODER_M2_B_PIN
/** Right-back encoder B phase: PA13 (SysConfig M1_B). */
#define PRJ_ENCODER_RB_B_PIN    ENCODER_M1_B_PIN

/**
 * 编码器安装方向修正：车体前进时四路编码器RPM应统一为正。
 * 若只更换某一路电机/编码器安装方向，只修改对应宏，不改ISR判向逻辑。
 */
#define PRJ_ENCODER_LF_DIR_SIGN (-1)
#define PRJ_ENCODER_LB_DIR_SIGN (-1)
#define PRJ_ENCODER_RF_DIR_SIGN (+1)
#define PRJ_ENCODER_RB_DIR_SIGN (+1)

/**
 * 电机输出通道与编码器反馈的物理对应关系。
 * A/M1=右后(RB)，B/M2=右前(RF)，C/M3=左前(LF)，D/M4=左后(LB)。
 * task_control.c据此将编码器反馈和位置控制目标的车轮顺序
 * (LF/LB/RF/RB)统一重排为电机顺序(A/B/C/D)。
 */
#define PRJ_MOTOR_A_ENCODER_ID  BSP_ENCODER_RB
#define PRJ_MOTOR_B_ENCODER_ID  BSP_ENCODER_RF
#define PRJ_MOTOR_C_ENCODER_ID  BSP_ENCODER_LF
#define PRJ_MOTOR_D_ENCODER_ID  BSP_ENCODER_LB
#define PRJ_MOTOR_ENCODER_MAP { \
    PRJ_MOTOR_A_ENCODER_ID, PRJ_MOTOR_B_ENCODER_ID, \
    PRJ_MOTOR_C_ENCODER_ID, PRJ_MOTOR_D_ENCODER_ID \
}

/** 编码器配置表(顺序需与BSP_ENCODER_x一致) */
#define PRJ_ENCODER_CONFIGS { \
		{ PRJ_ENCODER_LF_TIMER, PRJ_ENCODER_LF_A_PORT, PRJ_ENCODER_LF_A_PIN, \
			PRJ_ENCODER_LF_B_PORT, PRJ_ENCODER_LF_B_PIN, PRJ_ENCODER_LF_DIR_SIGN }, \
		{ PRJ_ENCODER_LB_TIMER, PRJ_ENCODER_LB_A_PORT, PRJ_ENCODER_LB_A_PIN, \
			PRJ_ENCODER_LB_B_PORT, PRJ_ENCODER_LB_B_PIN, PRJ_ENCODER_LB_DIR_SIGN }, \
		{ PRJ_ENCODER_RF_TIMER, PRJ_ENCODER_RF_A_PORT, PRJ_ENCODER_RF_A_PIN, \
			PRJ_ENCODER_RF_B_PORT, PRJ_ENCODER_RF_B_PIN, PRJ_ENCODER_RF_DIR_SIGN }, \
		{ PRJ_ENCODER_RB_TIMER, PRJ_ENCODER_RB_A_PORT, PRJ_ENCODER_RB_A_PIN, \
			PRJ_ENCODER_RB_B_PORT, PRJ_ENCODER_RB_B_PIN, PRJ_ENCODER_RB_DIR_SIGN }, \
}

/** 电压ADC HAL实例 */

/** ADC 电压采样通道 HAL 实例 */
#define PRJ_ADC_VOLTAGE_ID      HAL_ADC_VOLTAGE
/** ADC参考电压(mV) */
#define PRJ_ADC_VREF_MV         (3300U)
/** ADC分辨率(12位) */
#define PRJ_ADC_RESOLUTION      (4096U)

/** Current sense shunt resistance and amplifier gain. */
#define PRJ_ADC_CURRENT_SHUNT_OHM     (0.15f)
#define PRJ_ADC_CURRENT_AMPLIFY       (10.0f)
#define PRJ_ADC_CURRENT_MA_PER_RAW \
    ((float)(PRJ_ADC_VREF_MV) / (float)(PRJ_ADC_RESOLUTION) / \
     PRJ_ADC_CURRENT_SHUNT_OHM / PRJ_ADC_CURRENT_AMPLIFY)

/** Overcurrent threshold and consecutive 5 ms control ticks. */
#define PRJ_ADC_CURRENT_OVERLOAD_MA   (9900U)
#define PRJ_ADC_CURRENT_OVERLOAD_TICKS (10U)

/* ================================================================
 * IMU 与 MATHACL 配置
 * ================================================================ */

/** 是否启用 MATHACL 硬件加速(1=启用，0=软件回退)。 */

/* MPU6050 兼容配置已保留；当前工程实际使用 LSM6DSR。 */

/* ================================================================
 * MATHACL 硬件加速配置
 * ================================================================ */

/** 是否启用 MATHACL 硬件加速(1=启用，0=软件回退)。 */
/** 是否启用 MATHACL 硬件加速(1=启用，0=软件回退)。 */
#define PRJ_MATHACL_ENABLE                  (1U)
/** 是否使用 MATHACL 硬件 ATAN2 路径。 */
#define PRJ_MATHACL_ATAN2_HW                (1U)
/** 是否使用 MATHACL 硬件 SINCOS 路径。 */
#define PRJ_MATHACL_SINCOS_HW               (1U)
/** 是否为 MATHACL 寄存器访问启用线程安全临界区。 */
#define PRJ_MATHACL_THREAD_SAFE             (0U)
/** 是否使用 MATHACL 硬件 SQRT 路径；默认关闭以保留已验证的软件路径。 */
#define PRJ_MATHACL_SQRT_HW                (0U)
/** 是否为 KF 编译 MATHACL 定点加速路径；默认关闭以保持现有运行行为。 */
#define PRJ_MATHACL_KF_HW                  (0U)
/** 是否为 EKF 编译 MATHACL 定点除法加速路径；默认关闭以保持现有运行行为。 */
#define PRJ_MATHACL_EKF_HW                 (0U)
/** 是否编译 MATHACL 矩阵实验实现；默认关闭以避免生产固件引入额外代码。 */
#define PRJ_MATHACL_MATRIX_ENABLE          (0U)

/* ================================================================
 *  IMU 任务配置
 * ================================================================ */

/** IMU 任务周期(ms), 100Hz */
#define PRJ_IMU_TASK_PERIOD_MS       (10U)

/** IMU 校准采样帧数。 */
#define PRJ_IMU_CALIB_SAMPLES                 (300U)
/** IMU 配置完成后的稳定等待时间(ms)。 */
#define PRJ_IMU_CALIB_SETTLE_MS               (50U)
/** 加速度模平方参考值(g^2)。 */
#define PRJ_IMU_CALIB_ACC_MAG_REF             (1.0f)
/** 加速度模平方静止判定容差(g^2)。 */
#define PRJ_IMU_CALIB_ACC_MAG_TOL             (0.065f)
/** 校准相邻帧加速度差分阈值(g)。 */
#define PRJ_IMU_CALIB_ACC_DELTA_MAX           (0.08f)
/** IMU 校准采样间隔(ms)。 */
#define PRJ_IMU_CALIB_SAMPLE_DELAY_MS         (9U)

/** 加速度方差滑动窗口长度(帧)。 */
#define PRJ_IMU_ACC_VAR_WINDOW                (10U)
/** 预留 IMU 读取耗时补偿(us)。 */
#define PRJ_IMU_DT_READ_COMPENSATION_US       (200U)
/** 加速度静止方差阈值(g^2 总和)。 */
#define PRJ_IMU_ACC_VAR_THRESHOLD             (0.0008f)
/** 运动状态互补滤波系数。 */
#define PRJ_IMU_ALPHA_MOVING                  (0.99f)
/** 静止状态互补滤波系数。 */
#define PRJ_IMU_ALPHA_STATIONARY              (0.30f)
/** 互补滤波系数单帧最大变化量。 */
#define PRJ_IMU_ALPHA_SMOOTH_STEP             (0.15f)

/** X/Y 轴静止陀螺偏置跟踪速率。 */
#define PRJ_IMU_BIAS_STATIONARY_RATE          (0.1f)
/** Z 轴静止陀螺偏置跟踪速率。 */
#define PRJ_IMU_BIAS_STATIONARY_RATE_Z        (0.1f)
/** 陀螺运动判定阈值(dps)。 */
#define PRJ_IMU_GYRO_MOTION_THRESHOLD         (5.0f)
/** 是否启用基于任务周期的 dt 异常收紧门限。 */
#define PRJ_IMU_ODR_ALIGN                     (0U)
/** dt 异常下限(s)。 */
#define PRJ_IMU_DT_ANOMALY_MIN_S              (0.003)
/** dt 异常上限(s)。 */
#define PRJ_IMU_DT_ANOMALY_MAX_S              (0.030)
/** IMU 异常 dt 或时间戳回绕时使用的默认周期(s)。 */
#define PRJ_IMU_DT_DEFAULT_S                  (0.01)


/** UART0 DMA buffer for the one-shot `imu` command (10 CSV fields). */
#define PRJ_IMU_SNAPSHOT_DMA_BUF_SIZE      (192U)

/** UART0 DMA continuous IMU stream period (milliseconds). */
#define PRJ_IMU_STREAM_PERIOD_MS            (200U)

/** Enable startup timing logs for diagnosing delayed IMU calibration. */
#ifndef PRJ_IMU_STARTUP_DIAG_ENABLE
#define PRJ_IMU_STARTUP_DIAG_ENABLE        (1U)
#endif

#define PRJ_IMU_KF_FILTER_BUF_SIZE          (2048U)
#if (PRJ_IMU_SNAPSHOT_DMA_BUF_SIZE < 128U)
#error "PRJ_IMU_SNAPSHOT_DMA_BUF_SIZE is too small"
#endif
#if (PRJ_IMU_KF_FILTER_BUF_SIZE < 256U)
#error "PRJ_IMU_KF_FILTER_BUF_SIZE is too small"
#endif

/** 控制任务优先级，数值越大优先级越高。 */

/** 控制任务优先级，数值越大优先级越高。 */
#define PRJ_TASK_PRIORITY_CONTROL       (5U)
/** IMU任务优先级。 */
#define PRJ_TASK_PRIORITY_IMU           (4U)
/** 菜单任务优先级。 */
#define PRJ_TASK_PRIORITY_MENU          (2U)

/** 控制任务栈大小，单位为 FreeRTOS 栈字。 */
#define PRJ_TASK_STACK_CONTROL          (256U)
/** IMU任务栈大小，单位为 FreeRTOS 栈字。 */
#define PRJ_TASK_STACK_IMU              (1280U)
/** 菜单任务栈大小，单位为 FreeRTOS 栈字。 */
#define PRJ_TASK_STACK_MENU             (384U)

/** 控制任务周期(ms)。 */
#define PRJ_CONTROL_PERIOD_MS           (5U)
/** 菜单任务轮询周期(ms)。 */
#define PRJ_MENU_POLL_PERIOD_MS         (100U)
/** 运行模式下的 RPM 输出周期(ms)。 */
#define PRJ_RPM_OUTPUT_PERIOD_MS        (30U)

/** 菜单命令行输入缓冲区大小(字节)。 */
#define PRJ_MENU_LINE_BUF_SIZE          (64U)

/** 速度环默认比例增益。 */
#define PRJ_PID_DEFAULT_KP              (0.8f)
/** 速度环默认积分增益。 */
#define PRJ_PID_DEFAULT_KI              (0.3f)
/** 速度环默认微分增益。 */
#define PRJ_PID_DEFAULT_KD              (0.0f)
/** 前馈模式 PID 默认比例增益。 */
#define PRJ_FF_PID_DEFAULT_KP           (0.5f)
/** 前馈模式 PID 默认积分增益。 */
#define PRJ_FF_PID_DEFAULT_KI           (0.1f)
/** 前馈模式 PID 默认微分增益。 */
#define PRJ_FF_PID_DEFAULT_KD           (0.0f)

/** 互补滤波系数(0~1, 0=全信任IMU, 1=全信任编码器) */

/* 互补滤波参数映射 */
#define PRJ_CF_ALPHA                 FILTER_COMP_ALPHA_DEFAULT
/** KF 过程噪声参数的工程映射。 */
#define PRJ_KF_Q_ANGLE_DEFAULT        FILTER_KF_Q_ANGLE_DEFAULT
/** KF 过程噪声参数的工程映射。 */
#define PRJ_KF_Q_BIAS_DEFAULT         FILTER_KF_Q_BIAS_DEFAULT
/** KF 测量噪声参数的工程映射。 */
#define PRJ_KF_R_MEASURE_DEFAULT      FILTER_KF_R_MEASURE_DEFAULT
/** KF 测量噪声参数的工程映射。 */
#define PRJ_KF_R_ZUPT_DEFAULT         FILTER_KF_R_ZUPT_DEFAULT
/** 轮胎外径(mm)，应以负载状态下的有效滚动直径标定。 */
#define PRJ_MOTOR_WHEEL_DIAMETER_MM  (60.0f)
/** 轮子有效滚动半径(m)，由轮径统一派生，避免重复配置。 */
#define PRJ_CF_WHEEL_RADIUS_M \
    (PRJ_MOTOR_WHEEL_DIAMETER_MM * 0.001f * 0.5f)
/** 轮距(m, 左右轮接地点中心距离)，应按实车标定。 */
#define PRJ_CF_WHEEL_BASE_M          (0.19f)

/** 位置环PID参数(位置式PID, 输出RPM修正) */

/** 位置环PID参数(位置式PID, 输出RPM修正) */
#define PRJ_POS_PID_KP              (0.5f)
#define PRJ_POS_PID_KI              (0.1f)
#define PRJ_POS_PID_KD              (0.0f)

/** 角度环PID参数(位置式PID, 输出差速RPM) */
#define PRJ_YAW_PID_KP              (2.0f)
#define PRJ_YAW_PID_KI              (0.0f)
#define PRJ_YAW_PID_KD              (0.0f)

/** 规划器加速度(RPM/s, 目标变化速率) */
#define PRJ_PLANNER_ACCEL           (500.0f)

/** 最大目标RPM(速度限幅, 防止过速) */
#define PRJ_PLANNER_MAX_RPM         (500.0f)

/** 到位判定阈值(位置:脉冲, 角度:度) */
#define PRJ_REACHED_THRESHOLD_POS   (5.0f)
#define PRJ_REACHED_THRESHOLD_YAW   (0.5f)

/** 到位持续周期数(20ms×10=200ms) */
#define PRJ_REACHED_COUNT           (10U)

/* 位置环专用保护：不改变速度环、角度环和测速逻辑。 */
/** 位置模式切换过渡时长(ms)，避免从零速缓慢爬升。 */
#define PRJ_POSITION_TRANSITION_MS       (150U)
/** 位置模式最小有效目标RPM，避开DRV8870死区附近的抖动。 */
#define PRJ_POSITION_MIN_ACTIVE_RPM      (80.0f)
/** 位置误差渐变补偿范围(count)，误差越接近停止阈值，最低RPM补偿越小。 */
#define PRJ_POSITION_MIN_ACTIVE_ERROR_COUNTS (120.0f)
/** 位置误差小于该值时不再用最小RPM强行顶动。 */
#define PRJ_POSITION_STOP_ERROR_COUNTS   (12.0f)
/**
 * 兼容保留：旧版位置环曾用此RPM阈值参与到位判定。
 * 当前版本不再用低速RPM判断位置完成，仅依据编码器位置误差。
 */
#define PRJ_POSITION_STOP_RPM             (15.0f)
/** A/D左右轮同步修正比例(RPM/count)。 */
#define PRJ_POSITION_SYNC_KP              (0.10f)
/** 左右轮同步修正最大值(RPM)。 */
#define PRJ_POSITION_SYNC_MAX_RPM         (20.0f)
/** 同步误差小于该值时不进行左右轮修正。 */
#define PRJ_POSITION_SYNC_DEADBAND_COUNTS (10.0f)
/** 位置完成前允许的最大左右轮累计差(count)。 */
#define PRJ_POSITION_SYNC_ERROR_COUNTS   (60.0f)

/** 模式切换过渡时长(ms, 角度环和其他模式使用) */
#define PRJ_MODE_TRANSITION_MS      (1000U)

/* ================================================================
 *  系统时间与数学常量
 * ================================================================ */

/** 系统节拍定时器 HAL 实例 */
#define PRJ_SYS_TICK_TIMER      HAL_TIMER_SYS_TICK

/** 圆周率(float精度, 供应用层避免魔数 3.14159265f) */

/** 圆周率(float精度, 供应用层避免魔数 3.14159265f) */
#define PRJ_PI_F                (3.14159265358979f)
/** 圆周率(double精度, 供滤波器等需要double精度的模块使用) */
#define PRJ_PI_D                (3.14159265358979323846)
/** 2π(float 精度) */
#define PRJ_TWO_PI_F            (6.28318530717958647692f)
/** π/2(float 精度) */
#define PRJ_PI_2_F              (1.57079632679489661923f)
/** 弧度→角度转换系数(float): 180/π */
#define PRJ_RAD2DEG_F           (57.29577951308232087685f)
/** 角度→弧度转换系数(float): π/180 */
#define PRJ_DEG2RAD_F           (0.01745329251994329577f)

/** 每秒毫秒数 */

/** 每秒毫秒数 */
#define PRJ_MS_PER_S            (1000U)
/** 每分钟毫秒数(60s × 1000ms) */
#define PRJ_MS_PER_MIN          (60000U)
/** 每毫秒微秒数 */
#define PRJ_US_PER_MS           (1000U)

/** 重力加速度 m/s^2 (float 精度) */
#define PRJ_GRAVITY_MS2         (9.80665f)

/** 16位无符号整数模数 (2^16), 用于定时器计数器回绕修正 */
#define PRJ_UINT16_MOD          (65536U)

/**
 * 编码器捕获定时器实际频率(Hz)
 * 来源: ti_msp_dl_config.c 中 CAPTURE_* 的 divideRatio=DIVIDE_4, prescale=199
 * 计算: BUSCLK(40MHz) / 4 / (199+1) = 100000 Hz
 * 用途: bsp_encoder.c / app_debug.c 的 M/T 法 RPM 计算
 * 依赖: 6000000LL = 60 × PRJ_CAPTURE_TIMER_FREQ_HZ
 *
 * ⚠️ sysconfig 修改 CAPTURE_* 的分频/prescale 后必须更新此值
 */

/**
 * 编码器捕获定时器实际频率(Hz)
 * 来源: ti_msp_dl_config.c 中 CAPTURE_* 的 divideRatio=DIVIDE_4, prescale=199
 * 计算: BUSCLK(40MHz) / 4 / (199+1) = 100000 Hz
 * 用途: bsp_encoder.c / app_debug.c 的 M/T 法 RPM 计算
 * 依赖: 6000000LL = 60 × PRJ_CAPTURE_TIMER_FREQ_HZ
 *
 * ⚠️ sysconfig 修改 CAPTURE_* 的分频/prescale 后必须更新此值
 */
#define PRJ_CAPTURE_TIMER_FREQ_HZ   (100000UL)

/**
 * 系统微秒计时器实际频率(Hz)
 * 来源: ti_msp_dl_config.c 中 TIMER_0 (TIMG8) 的 divideRatio=DIVIDE_8, prescale=9
 * 计算: BUSCLK(40MHz) / 8 / (9+1) = 500000 Hz (2us/tick)
 * 用途: platform_mspm0.c get_tick_us() 的微秒换算
 * 依赖: tick_to_us = count / (PRJ_SYS_TICK_TIMER_FREQ_HZ / 1000000UL)
 *        即 count * 2U (当前硬编码)
 *
 * ⚠️ sysconfig 修改 TIMER_0 的分频/prescale 后必须更新此值
 */
#define PRJ_SYS_TICK_TIMER_FREQ_HZ  (500000UL)

/**
 * 微秒计时器: 1 个 tick 对应的微秒数(×1000 扩大精度避免浮点)
 * 计算: 1000000 / PRJ_SYS_TICK_TIMER_FREQ_HZ = 2 (即 2us/tick)
 * 用途: platform_mspm0.c:85 替换硬编码 * 2U
 *
 * ⚠️ 与 PRJ_SYS_TICK_TIMER_FREQ_HZ 联动，修改一处需同步检查
 */
#define PRJ_SYS_TICK_US_PER_TICK_X1000  \
    (1000000UL * 1000UL / PRJ_SYS_TICK_TIMER_FREQ_HZ)

/**
 * 编码器 M/T 法 RPM 计算常数
 * 计算: 60 × PRJ_CAPTURE_TIMER_FREQ_HZ = 60 × 100000 = 6000000
 * 用途: bsp_encoder.c / app_debug.c 中 RPM = (delta × 60 × timer_freq) / (pulses × period)
 *
 * ⚠️ 与 PRJ_CAPTURE_TIMER_FREQ_HZ 联动
 */
#define PRJ_ENCODER_RPM_CALC_CONST  \
    (60LL * (int64_t)PRJ_CAPTURE_TIMER_FREQ_HZ)

/** DRV8870 PWM定时器HAL实例(复用TIMA0) */

/** DRV8870 PWM 通道 HAL 实例(复用 TIMA0) */
#define PRJ_DRV8870_PWM_TIMER     HAL_TIMER_PWM_MOTOR
/** PWM 时钟频率(Hz) - 由 SysConfig 提供 */
#define PRJ_DRV8870_PWM_CLK_HZ    ((unsigned long)(PWM_MOTOR_INST_CLK_FREQ))
/** PWM周期值(20kHz = 1000个20MHz时钟周期) */
#define PRJ_DRV8870_PWM_PERIOD    (1000U)
/** 有符号速度命令最大绝对值；业务层、PID和模型辨识均应保持一致。 */
#define PRJ_DRV8870_SPEED_COMMAND_MAX PRJ_MOTOR_COMMAND_MAX

/**
 * 实测机械死区边界（绝对PWM占空比百分数）。
 * 0%~39.9%为反转有效区，40%~55%为停止死区，55.1%~100%为正转有效区。
 * bsp_drv8870_set_speed()会把非零有符号命令分段映射到死区之外；
 * 工厂示波器接口仍是原始绝对compare，可直接进入死区用于测量。
 */
#define PRJ_DRV8870_DEADBAND_LOW_PERCENT     (40U)
#define PRJ_DRV8870_NEUTRAL_PERCENT          (50U)
#define PRJ_DRV8870_DEADBAND_HIGH_PERCENT    (55U)

#if (PRJ_DRV8870_DEADBAND_LOW_PERCENT == 0U) || \
    (PRJ_DRV8870_DEADBAND_HIGH_PERCENT >= 100U) || \
    (PRJ_DRV8870_DEADBAND_LOW_PERCENT >= \
     PRJ_DRV8870_DEADBAND_HIGH_PERCENT)
#error "DRV8870 deadband must satisfy 0 < low < high < 100"
#endif
#if (PRJ_DRV8870_NEUTRAL_PERCENT < PRJ_DRV8870_DEADBAND_LOW_PERCENT) || \
    (PRJ_DRV8870_NEUTRAL_PERCENT > PRJ_DRV8870_DEADBAND_HIGH_PERCENT)
#error "DRV8870 neutral duty must lie inside the configured deadband"
#endif

/** Motor power gate: PB19 is active-high; SysConfig initializes it low. */
#define PRJ_DRV8870_POWER_PORT       HAL_GPIO_PORT_B
#define PRJ_DRV8870_POWER_PIN        POWER_pb19_PIN
#define PRJ_DRV8870_POWER_ON_LEVEL   true
#define PRJ_DRV8870_POWER_OFF_LEVEL  false
/** Allow VIN_OUT/DRV8870 to settle after enable and before power-off. */
#define PRJ_DRV8870_POWER_STARTUP_MS  (20U)
#define PRJ_DRV8870_POWER_SETTLE_MS   (5U)

/*
 * 零点偏移补偿 (S8050 反相器开关不对称 + DRV8870 传播延迟)
 * 文档参考: DRV8870技术文档 §3.2.2, 典型偏移 2~5 步
 * 需实测标定: 找到使电机恰好静止的 duty 值, 减去 PWM_PERIOD/2
 */
#define PRJ_DRV8870_ZERO_DUTY_OFFSET  (0)

/*
 * DRV8870 方向与安全策略。
 * 启动时保持中立，占空比位于死区内；切换方向前由驱动层执行安全时序。
 */
#ifndef PRJ_DRV8870_FACTORY_TEST_ENABLE
#define PRJ_DRV8870_FACTORY_TEST_ENABLE       (0U)
#endif

/* ---- 电机 A/M1(电机1): CC0=PA8 ---- */
#define PRJ_DRV8870_A_PWM_CH      (0U)

/* ---- 电机 B/M2(电机2): CC1=PA9 ---- */
#define PRJ_DRV8870_B_PWM_CH      (1U)

/* ---- 电机 C/M3(电机3): CC2=PB17 ---- */
#define PRJ_DRV8870_C_PWM_CH      (2U)

/* ---- 电机 D/M4(电机4): CC3=PB2 ---- */
#define PRJ_DRV8870_D_PWM_CH      (3U)


/** 兼容原DRV8870配置宏名称。 */
#define PRJ_DRV8870_A_DIR_SIGN  PRJ_MOTOR_A_INSTALL_DIR_SIGN
#define PRJ_DRV8870_B_DIR_SIGN  PRJ_MOTOR_B_INSTALL_DIR_SIGN
#define PRJ_DRV8870_C_DIR_SIGN  PRJ_MOTOR_C_INSTALL_DIR_SIGN
#define PRJ_DRV8870_D_DIR_SIGN  PRJ_MOTOR_D_INSTALL_DIR_SIGN

/** DRV8870 电机配置表(顺序需与BSP_DRV8870_x一致) */
#define PRJ_DRV8870_CONFIGS { \
    { PRJ_DRV8870_A_PWM_CH, PRJ_DRV8870_A_DIR_SIGN, \
      PRJ_DRV8870_ZERO_DUTY_OFFSET, \
      PRJ_DRV8870_DEADBAND_LOW_PERCENT, PRJ_DRV8870_NEUTRAL_PERCENT, \
      PRJ_DRV8870_DEADBAND_HIGH_PERCENT }, \
    { PRJ_DRV8870_B_PWM_CH, PRJ_DRV8870_B_DIR_SIGN, \
      PRJ_DRV8870_ZERO_DUTY_OFFSET, \
      PRJ_DRV8870_DEADBAND_LOW_PERCENT, PRJ_DRV8870_NEUTRAL_PERCENT, \
      PRJ_DRV8870_DEADBAND_HIGH_PERCENT }, \
    { PRJ_DRV8870_C_PWM_CH, PRJ_DRV8870_C_DIR_SIGN, \
      PRJ_DRV8870_ZERO_DUTY_OFFSET, \
      PRJ_DRV8870_DEADBAND_LOW_PERCENT, PRJ_DRV8870_NEUTRAL_PERCENT, \
      PRJ_DRV8870_DEADBAND_HIGH_PERCENT }, \
    { PRJ_DRV8870_D_PWM_CH, PRJ_DRV8870_D_DIR_SIGN, \
      PRJ_DRV8870_ZERO_DUTY_OFFSET, \
      PRJ_DRV8870_DEADBAND_LOW_PERCENT, PRJ_DRV8870_NEUTRAL_PERCENT, \
      PRJ_DRV8870_DEADBAND_HIGH_PERCENT }, \
}
#if (PRJ_DRV8870_SPEED_COMMAND_MAX != (PRJ_DRV8870_PWM_PERIOD / 2U))
#error "DRV8870 backend requires command max equal to half of PWM period"
#endif

/* 上层只使用这些所选后端别名，不直接引用芯片专用参数。 */
#if (PRJ_MOTOR_DRIVER == PRJ_MOTOR_DRIVER_DRV8870)
#define PRJ_MOTOR_PWM_TIMER         PRJ_DRV8870_PWM_TIMER
#define PRJ_MOTOR_PWM_CLK_HZ        PRJ_DRV8870_PWM_CLK_HZ
#define PRJ_MOTOR_PWM_PERIOD        PRJ_DRV8870_PWM_PERIOD
#define PRJ_MOTOR_POWER_STARTUP_MS  PRJ_DRV8870_POWER_STARTUP_MS
#define PRJ_MOTOR_POWER_SETTLE_MS   PRJ_DRV8870_POWER_SETTLE_MS
#else
#define PRJ_MOTOR_PWM_TIMER         PRJ_TB6612_PWM_TIMER
#define PRJ_MOTOR_PWM_CLK_HZ        PRJ_TB6612_PWM_CLK_HZ
#define PRJ_MOTOR_PWM_PERIOD        PRJ_TB6612_PWM_PERIOD
#define PRJ_MOTOR_POWER_STARTUP_MS  PRJ_TB6612_POWER_STARTUP_MS
#define PRJ_MOTOR_POWER_SETTLE_MS   (0U)
#endif

#if (PRJ_DRV8870_FACTORY_TEST_ENABLE != 0U) && \
    (PRJ_MOTOR_DRIVER != PRJ_MOTOR_DRIVER_DRV8870)
#error "DRV8870 factory-test target requires the DRV8870 motor backend"
#endif

#ifdef __cplusplus
}
#endif

#endif /* PROJECT_CONFIG_H */
