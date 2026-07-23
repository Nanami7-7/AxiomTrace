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

#include "filter.h"          /* filter_config 类型已合并到 filter.h */
#include "project_config.h"  /* 滤波器参数宏 */

#include <stdint.h>
#include <stddef.h>

/* ============================================================
 * 共享宏定义
 * ============================================================ */

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

/* 数值安全阈值（供 normalize_quaternion_internal 和 KF 使用） */
#define NORM_EPS            1e-10f
#define KF_HW_S_MIN         1e-10f

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

/* --- ESKF 误差状态卡尔曼滤波器 --- */
typedef struct {
    float q0, q1, q2, q3;              /**< 名义四元数 */
    float bias_x, bias_y, bias_z;       /**< 陀螺偏置 (dps) */
    float P[6][6];                      /**< 误差协方差矩阵 (6x6) */
    float Q_angle;                      /**< 过程噪声-角度 */
    float Q_bias;                       /**< 过程噪声-偏置 */
    float R_measure;                    /**< 测量噪声 */
    float bias_limit_dps;               /**< 偏置绝对值上限 (dps) */
    uint16_t update_count;              /**< 更新计数(周期正则化) */
} eskf_priv_t;

/* --- Mahony 自适应非线性互补滤波器 --- */
typedef struct {
    float kp_base;              /**< 基础比例增益 */
    float ki_base;              /**< 基础积分增益 */
    float kp_eff;               /**< 当前有效 kp */
    float ki_eff;               /**< 当前有效 ki */
    float q0, q1, q2, q3;       /**< 四元数 */
    float ix, iy, iz;           /**< 积分误差 (陀螺偏置估计, rad/s) */
    float bias_x, bias_y, bias_z; /**< 陀螺偏置 (dps, 物理单位) */
    /* 物理约束参数 */
    float acc_gate_low;         /**< ACC 幅值下限 (g) */
    float acc_gate_high;        /**< ACC 幅值上限 (g) */
    float gyro_sat_thresh;      /**< 陀螺饱和阈值 (dps) */
    float bias_limit;           /**< 偏置限幅 (dps) */
    float static_gyro_th;       /**< 静态判定 gyro 阈值 (dps) */
    float static_acc_th;        /**< 静态判定 ACC 偏差阈值 (g) */
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
    uint16_t update_count;  /* 更新计数(周期正定性检查) */
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

/* --- ESKF 误差状态卡尔曼滤波器 --- */
filter_t* filter_create_eskf(float q_angle, float q_bias, float r_measure);
void eskf_update(filter_t *self, const filter_input_t *in, filter_output_t *out);
void eskf_reset(filter_t *self);
void eskf_set_param(filter_t *self, filter_param_t param, float value);

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
