/**
 * @file    filter_internal.h
 * @brief   滤波器模块内部共享头文件
 *
 * 声明各滤波器模块间需要共享的内容：
 *   - 私有数据结构 typedef
 *   - 各滤波器的 create/update/reset/set_param 函数声明
 *   - 共享的内部辅助函数声明
 *   - 共享的宏定义和全局变量
 *
 * @note 仅供 Filter/ 目录下的 .c 文件使用，外部代码不应包含此头文件。
 */

#ifndef FILTER_INTERNAL_H
#define FILTER_INTERNAL_H

#include "filter.h"
#include "filter_config.h"

#include <stdint.h>
#include <stddef.h>

/* ============================================================
 * 共享宏定义
 * ============================================================ */

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* 数值安全阈值（供 normalize_quaternion_internal 和 KF 使用） */
#define EKF_NORM_EPS            1e-10f
#define KF_HW_S_MIN             1e-10f

/* EKF 状态向量维度 */
#define EKF_STATE_SIZE 7
#define EKF_MEAS_SIZE 3

/* LKF 状态向量维度 */
#define LKF_STATE_SIZE 6

/* KF 状态向量维度 */
#define KF_STATE_SIZE 2     /* [angle, bias] */
#define KF_AXIS_COUNT 3     /* [pitch, roll, yaw] */

/* ============================================================
 * 共享全局变量（在 filter_core.c 中定义）
 * ============================================================ */

extern filter_error_callback_t g_error_callback;
extern void *g_error_user_data;
extern filter_error_info_t g_last_error;

/* 各滤波器私有数据大小数组（在 filter_core.c 中定义） */
extern const size_t priv_sizes[];

/* ============================================================
 * 错误报告（在 filter_core.c 中定义）
 * ============================================================ */

void report_error(filter_error_t code, const char *message,
                  const char *file, int line);

/* 错误报告宏 */
#define FILTER_REPORT_ERROR(code, msg) \
    report_error(code, msg, __FILE__, __LINE__)

/* ============================================================
 * 共享内部辅助函数（在 filter_core.c 中定义）
 * ============================================================ */

int normalize_quaternion_internal(float *q0, float *q1, float *q2, float *q3);

void filter_quat_to_euler(float q0, float q1, float q2, float q3,
                          filter_output_t *out);

int filter_acc_to_quat(float ax, float ay, float az,
                       float *q0, float *q1, float *q2, float *q3);

void filter_quat_integrate_gyro(float *q0, float *q1, float *q2, float *q3,
                                float gx, float gy, float gz, float dt);

int validate_filter_param(filter_param_t param, float value);

void static_destroy_noop(filter_t *self);

/* ============================================================
 * 各滤波器私有数据结构 typedef
 * ============================================================ */

/* --- 互补滤波器 --- */
typedef struct {
    float alpha;        /**< 融合系数 */
    float pitch, roll, yaw;  /**< 姿态角 */
} complementary_priv_t;

/* --- 一阶低通滤波器 --- */
typedef struct {
    float cutoff_freq;  /**< 截止频率 (Hz) */
    float alpha;        /**< 滤波系数 */
    float pitch, roll, yaw;  /**< 姿态角 */
    float prev_pitch, prev_roll, prev_yaw;  /**< 上一帧值 */
} lpf_priv_t;

/* --- EKF 扩展卡尔曼滤波器 --- */
typedef struct {
    float state[EKF_STATE_SIZE];        /**< 状态向量 */
    float P[EKF_STATE_SIZE][EKF_STATE_SIZE];  /**< 协方差矩阵 */
    float Q_angle;                      /**< 过程噪声-角度 */
    float Q_bias;                       /**< 过程噪声-偏置 */
    float R_measure;                    /**< 测量噪声 */
    float bias_limit_dps;               /**< 偏置幅值限制 (dps) */
    float chi2_threshold;               /**< Chi-squared 门限 */
    float R_adapt_factor;               /**< 动态 R 适配缩放因子 */
    int r_adapt_enable;                 /**< 动态 R 适配使能 */
    uint16_t update_count;              /**< 更新计数，用于周期性正则化 */
    uint8_t use_hw;                     /**< 1=使用 MATHACL 硬件除法，0=纯软浮点 */
#if EKF_COND_NORMALIZE
    uint8_t norm_skip_count;            /**< 条件归一化跳过计数 */
#endif
#if EKF_NIS_MONITOR
    float nis_window[EKF_NIS_WINDOW_SIZE]; /**< NIS 滑动窗口 */
    uint8_t nis_idx;                    /**< 环形缓冲区索引 */
    uint8_t nis_count;                  /**< 有效样本数(≤窗口大小) */
    float nis_avg;                      /**< 最新 NIS 平均值 */
#endif
#if EKF_R_ADAPTIVE
    float r_nis_factor;                 /**< NIS 驱动的 R 缩放因子(初始1.0) */
#endif
#if EKF_MANEUVER_DETECT
    uint8_t maneuver_level;             /**< 机动等级 0=静止,1=准静态,2=缓动态,3=高动态 */
    float gyro_mag_dps;                 /**< 最新陀螺角速度幅值 (dps) */
    float acc_norm_err;                 /**< 最新加速度幅值偏差 (g) */
#endif
#if EKF_ZUPT_ENABLE
    /* ZUPT 滑动窗口: 静止时累积陀螺读数, 满后取均值作为偏置观测 */
    float zupt_win_gx[EKF_ZUPT_WINDOW_SIZE];  /**< X 轴陀螺窗口 */
    float zupt_win_gy[EKF_ZUPT_WINDOW_SIZE];  /**< Y 轴陀螺窗口 */
    float zupt_win_gz[EKF_ZUPT_WINDOW_SIZE];  /**< Z 轴陀螺窗口 */
    uint8_t zupt_win_idx;               /**< 窗口环形索引 */
    uint8_t zupt_win_count;             /**< 窗口有效样本数 (≤WINDOW_SIZE) */
#endif
#if EKF_GYRO_RESIDUAL_ENABLE
    /* 陀螺仪残留抑制状态: maneuver_level 高→低转换触发窗口 */
    uint32_t residual_active_frames;     /**< 残留抑制已激活帧数 (0=未激活) */
    float    residual_decay_factor;      /**< 当前衰减因子 (1.0=不衰减, <1=残留抑制中) */
#endif
    float last_chi2;                    /**< 最新 chi2 统计量(诊断用, 始终存储) */
    /* 工作缓冲区：避免在任务栈上分配大型矩阵，每个实例独立，天然可重入 */
    float P_new[7][7];                  /**< 协方差预测临时矩阵 */
    float FP[4][7];                     /**< F*P 四元数部分 */
    float H[3][7];                      /**< 观测矩阵 */
    float PHt[7][3];                    /**< P * H^T */
    float K[7][3];                      /**< 卡尔曼增益 */
    float KH[7][7];                     /**< K * H */
    float I_KH[7][7];                   /**< I - KH */
    float temp[7][7];                   /**< Joseph 形式临时矩阵 */
} ekf_priv_t;

/* --- Mahony 滤波器 --- */
typedef struct {
    float kp;           /**< 比例增益 */
    float ki;           /**< 积分增益 */
    float q0, q1, q2, q3;  /**< 四元数 */
    float ix, iy, iz;   /**< 积分误差 */
} mahony_priv_t;

/* --- Madgwick 滤波器 --- */
typedef struct {
    float beta;         /**< 梯度下降步长 */
    float q0, q1, q2, q3;  /**< 四元数 */
} madgwick_priv_t;

/* --- LKF 线性卡尔曼滤波器 --- */
typedef struct {
    float x[LKF_STATE_SIZE];        /* 状态向量 [pitch, roll, yaw, bx, by, bz] */
    float P[LKF_STATE_SIZE][LKF_STATE_SIZE]; /* 协方差矩阵 */
    float Q_angle;   /* 过程噪声-角度 */
    float Q_bias;    /* 过程噪声-偏置 */
    float R_measure; /* 测量噪声 */
} lkf_priv_t;

/* --- KF 纯卡尔曼滤波器 --- */
typedef struct {
    float x[KF_AXIS_COUNT][KF_STATE_SIZE];          /* [axis][angle, bias] */
    float P[KF_AXIS_COUNT][KF_STATE_SIZE][KF_STATE_SIZE]; /* [axis][2][2] 协方差 */
    float Q_angle;   /* 过程噪声-角度 (deg²/s) */
    float Q_bias;    /* 过程噪声-偏置 (dps²/s) */
    float R_measure; /* 测量噪声 (deg²) */
    float R_zupt;    /* ZUPT 伪测量噪声 (dps²), 1e6=禁用 */
    float angle_min; /* 角度输出限幅 */
    float angle_max;
    uint8_t use_hw;  /* 1=使用 MATHACL 硬件路径，0=纯软浮点 */
} kf_priv_t;

/* ============================================================
 * 各滤波器函数声明
 * ============================================================ */

/* --- 互补滤波器 --- */
filter_t* filter_create_complementary(float alpha);
void complementary_update(filter_t *self, const filter_input_t *in, filter_output_t *out);
void complementary_reset(filter_t *self);
void complementary_set_param(filter_t *self, filter_param_t param, float value);

/* --- 一阶低通滤波器 --- */
filter_t* filter_create_lpf(float cutoff_freq);
void lpf_update(filter_t *self, const filter_input_t *in, filter_output_t *out);
void lpf_reset(filter_t *self);
void lpf_set_param(filter_t *self, filter_param_t param, float value);

/* --- EKF 扩展卡尔曼滤波器 --- */
filter_t* filter_create_ekf(float q_angle, float q_bias, float r_measure);
void ekf_update(filter_t *self, const filter_input_t *in, filter_output_t *out);
void ekf_reset(filter_t *self);
void ekf_set_param(filter_t *self, filter_param_t param, float value);

/* --- LKF 线性卡尔曼滤波器 --- */
filter_t* filter_create_lkf(float q_angle, float q_bias, float r_measure);
void lkf_update(filter_t *self, const filter_input_t *in, filter_output_t *out);
void lkf_reset(filter_t *self);
void lkf_set_param(filter_t *self, filter_param_t param, float value);

/* --- Mahony 滤波器 --- */
filter_t* filter_create_mahony(float kp, float ki);
void mahony_update(filter_t *self, const filter_input_t *in, filter_output_t *out);
void mahony_reset(filter_t *self);
void mahony_set_param(filter_t *self, filter_param_t param, float value);

/* --- Madgwick 滤波器 --- */
filter_t* filter_create_madgwick(float beta);
void madgwick_update(filter_t *self, const filter_input_t *in, filter_output_t *out);
void madgwick_reset(filter_t *self);
void madgwick_set_param(filter_t *self, filter_param_t param, float value);

/* --- KF 纯卡尔曼滤波器 --- */
filter_t* filter_create_kf(float q_angle, float q_bias, float r_measure);
void kf_update(filter_t *self, const filter_input_t *in, filter_output_t *out);
void kf_reset(filter_t *self);
void kf_set_param(filter_t *self, filter_param_t param, float value);

#endif /* FILTER_INTERNAL_H */
