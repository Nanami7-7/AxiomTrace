/**
 * @file    bsp_lsm6dsr.h
 * @brief   BSP 业务层 — 姿态互补滤波器与生产 API
 *
 * 三层架构的中间层，封装完整的 IMU 姿态估计算法：
 *   - 互补滤波器 (pitch/roll/yaw)
 *   - 自适应 α (运动时近纯 GYRO，静止时 ACC 主导)
 *   - 三重静止检测 (方差滑动窗口 + ACC 幅值 + GYRO 幅值)
 *   - Runtime 陀螺偏置跟踪 (X/Y 与 Z 轴独立速率)
 *   - 平台抽象计时 (通过 platform_t 接口)
 *   - VOFA+ 10 通道格式化输出
 *
 * 所有宏通过 #ifndef 定义，允许编译器 -D 覆盖。
 */
#ifndef BSP_LSM6DSR_H
#define BSP_LSM6DSR_H

#include <stdint.h>
#include "filter.h"        /**< 滤波器统一接口 */
#include "project_config.h" /**< IMU校准与自适应参数 (IMU_CALIB_*, IMU_ALPHA_*, etc.) */

/* 兼容别名 - 配置已迁移到 project_config.h */
#define BSP_CALIB_SAMPLES              IMU_CALIB_SAMPLES
#define BSP_CALIB_SETTLE_MS            IMU_CALIB_SETTLE_MS
#define BSP_CALIB_ACC_MAG_REF          IMU_CALIB_ACC_MAG_REF
#define BSP_CALIB_ACC_MAG_TOL          IMU_CALIB_ACC_MAG_TOL
#define BSP_CALIB_ACC_DELTA_MAX        IMU_CALIB_ACC_DELTA_MAX
#define BSP_CALIB_SAMPLE_DELAY_MS      IMU_CALIB_SAMPLE_DELAY_MS
#define BSP_ACC_VAR_WINDOW             IMU_ACC_VAR_WINDOW
#define BSP_IMU_DT_READ_COMPENSATION_US IMU_DT_READ_COMPENSATION_US
#define BSP_ACC_VAR_THRESHOLD          IMU_ACC_VAR_THRESHOLD
#define BSP_ALPHA_MOVING               IMU_ALPHA_MOVING
#define BSP_ALPHA_STATIONARY           IMU_ALPHA_STATIONARY
#define BSP_ALPHA_SMOOTH_STEP          IMU_ALPHA_SMOOTH_STEP
#define BSP_BIAS_STATIONARY_RATE       IMU_BIAS_STATIONARY_RATE
#define BSP_BIAS_STATIONARY_RATE_Z     IMU_BIAS_STATIONARY_RATE_Z
#define BSP_GYRO_MOTION_THRESHOLD      IMU_GYRO_MOTION_THRESHOLD
#define BSP_ODR_ALIGN                  IMU_ODR_ALIGN
#define BSP_DT_ANOMALY_MIN_S           IMU_DT_ANOMALY_MIN_S
#define BSP_DT_ANOMALY_MAX_S           IMU_DT_ANOMALY_MAX_S

/** @brief  IMU 姿态数据结构体 (10 通道输出) */
typedef struct {
    float ax, ay, az;       /**< 加速度   m/s²  (G × 9.80665) */
    float gx, gy, gz;       /**< 角速度   dps    (偏置补偿后) */
    float pitch, roll, yaw; /**< 姿态角   deg    (互补滤波) */
    float temperature;      /**< 温度     °C     */
} bsp_lsm6dsr_data_t;

/** @defgroup BSP_Context 上下文结构体 */
/**@{*/

/**
 * @brief BSP 上下文结构体
 *
 * 将所有运行时状态封装到结构体中，支持：
 * - 多个 IMU 同时工作
 * - RTOS 环境下线程安全
 * - 独立的滤波器实例
 * - 状态保存/恢复
 */
typedef struct {
    /* ---- IMU 状态 ---- */
    float   bgx, bgy, bgz;          /**< 陀螺偏置 (dps) */
    int     cal_ok;                  /**< 校准成功标志 */

    /* ---- 滤波器状态 ---- */
    double  pitch, roll, yaw;        /**< 姿态角 (deg) */
    uint64_t last_tick_us;           /**< 上一帧时间戳 (us) - 改用 64 位防溢出 */
    double  dt_ema;                  /**< dt EMA 滤波值(s), 抑制任务调度抖动 */
    int     initialized;             /**< 初始化完成标志 */

    /* ---- 自适应滤波器状态 ---- */
    float   ax_buf[BSP_ACC_VAR_WINDOW]; /**< ACC X 滑动窗口 */
    float   ay_buf[BSP_ACC_VAR_WINDOW]; /**< ACC Y 滑动窗口 */
    float   az_buf[BSP_ACC_VAR_WINDOW]; /**< ACC Z 滑动窗口 */
    int     var_buf_idx;               /**< 窗口循环索引 */
    int     var_samples;               /**< 已采帧数 */
    float   alpha;                     /**< 当前互补滤波 α */
    float   last_variance;             /**< 上一帧方差 (调试用) */

    /* ---- 滤波器实例 ---- */
    filter_t *active_filter;           /**< 当前活动滤波器实例 */
    filter_type_t current_filter_type; /**< 当前滤波器类型 */

    /* ---- 生产 API 缓存 ---- */
    bsp_lsm6dsr_data_t last_data;    /**< 最新数据缓存 */
    int     is_stationary;           /**< 最新静止状态 */

    /* ---- 错误统计 ---- */
    uint32_t update_count;           /**< 更新次数 */
    uint32_t error_count;            /**< 错误次数 */

    /* ---- KF 诊断缓存 (由 filter_kf_get_diag 填充) ---- */
    float   kf_bias_x;               /**< KF X轴偏置估计 (dps) */
    float   kf_bias_y;               /**< KF Y轴偏置估计 (dps) */
    float   kf_bias_z;               /**< KF Z轴偏置估计 (dps) */
    float   kf_p00_x;                /**< KF X轴角度协方差 */
    float   kf_p00_y;                /**< KF Y轴角度协方差 */
    float   kf_p00_z;                /**< KF Z轴角度协方差 */
    float   kf_p11_x;                /**< KF X轴偏置协方差 */
    float   kf_p11_y;                /**< KF Y轴偏置协方差 */
    float   kf_p11_z;                /**< KF Z轴偏置协方差 */

    /* ---- 机动状态诊断 (VOFA+/test runner 用) ---- */
    float   gyro_mag_dps;            /**< 陀螺角速度幅值 (dps) */
    float   acc_norm_err;            /**< 加速度幅值偏差 (g), |norm-1| */
} bsp_lsm6dsr_ctx_t;

/**@}*/

/** @defgroup BSP_API 生产 API */
/**@{*/

/* ========== 新 API（推荐：支持多实例） ========== */

/**
 * @brief  初始化 BSP 上下文
 * @param[out] ctx  上下文结构体指针（调用者分配）
 * @return 0=成功, -1=失败
 * @details 初始化所有状态，动态创建默认互补滤波器实例。
 *          生产任务优先使用 bsp_lsm6dsr_init_ctx_with_filter()。
 */
int bsp_lsm6dsr_init_ctx(bsp_lsm6dsr_ctx_t *ctx);

/**
 * @brief Initialize a context with a caller-owned filter instance.
 * @param[out] ctx             Context allocated by the caller.
 * @param[in]  initial_filter  Fully constructed filter retained by the context.
 * @return 0 on success, negative on failure.
 * @note The caller must keep the filter storage alive until destroy_ctx().
 */
int bsp_lsm6dsr_init_ctx_with_filter(bsp_lsm6dsr_ctx_t *ctx,
                                     filter_t *initial_filter);

/**
 * @brief  姿态更新（上下文版本）
 * @param[in,out] ctx  上下文结构体指针
 * @param[out]    data 输出数据结构体指针
 * @return 0=成功, <0=错误码
 */
int bsp_lsm6dsr_update_ctx(bsp_lsm6dsr_ctx_t *ctx, bsp_lsm6dsr_data_t *data);

/**
 * @brief  陀螺零偏校准（上下文版本）
 * @param[in,out] ctx  上下文结构体指针
 * @return 0=成功, -1=失败
 */
int bsp_lsm6dsr_calibrate_ctx(bsp_lsm6dsr_ctx_t *ctx);

/**
 * @brief  销毁上下文，释放滤波器资源
 * @param[in,out] ctx  上下文结构体指针
 */
void bsp_lsm6dsr_destroy_ctx(bsp_lsm6dsr_ctx_t *ctx);

/**
 * @brief  获取陀螺零偏（上下文版本）
 * @param[in]  ctx  上下文结构体指针
 * @param[out] bx   X 轴偏置 (dps)，允许 NULL
 * @param[out] by   Y 轴偏置 (dps)，允许 NULL
 * @param[out] bz   Z 轴偏置 (dps)，允许 NULL
 */
void bsp_lsm6dsr_get_bias_ctx(const bsp_lsm6dsr_ctx_t *ctx, float *bx, float *by, float *bz);

/**
 * @brief  查询静止状态（上下文版本）
 * @param[in] ctx  上下文结构体指针
 * @return 1=静止 / 0=运动
 */
int bsp_lsm6dsr_is_stationary_ctx(const bsp_lsm6dsr_ctx_t *ctx);

/**
 * @brief  切换滤波器类型（上下文版本）
 * @param[in,out] ctx   上下文结构体指针
 * @param[in]     type  滤波器类型
 * @return 0=成功, -1=失败
 */
int bsp_lsm6dsr_set_filter_ctx(bsp_lsm6dsr_ctx_t *ctx, filter_type_t type);

/**
 * @brief  设置滤波器参数（上下文版本）
 * @param[in,out] ctx    上下文结构体指针
 * @param[in]     param  参数枚举
 * @param[in]     value  参数值
 * @return 0=成功, -1=失败
 */
int bsp_lsm6dsr_set_filter_param_ctx(bsp_lsm6dsr_ctx_t *ctx, filter_param_t param, float value);

/* ========== 旧 API（向后兼容：使用默认全局上下文） ========== */

/**
 * @brief  传感器初始化
 * @details 执行完整初始化流程:
 *          SW_RESET → I3C 禁用 → IF_INC+BDU → ACC 104Hz/4G →
 *          GYRO 104Hz/250dps → 滤波状态初始化 → DWT 使能 → 校准
 * @note   使用默认全局上下文，不支持多实例
 */
void bsp_lsm6dsr_init(void);

/**
 * @brief  陀螺零偏校准
 * @details 采集 BSP_CALIB_SAMPLES 帧，用 ACC 静止检测拒斥运动帧，
 *          有效帧过半时取均值作为偏置。可在运行时重复调用。
 * @note   使用默认全局上下文
 */
void bsp_lsm6dsr_calibrate(void);

/**
 * @brief  姿态更新 (核心滤波)
 * @param  data 输出数据结构体指针
 * @details 每次调用完成一帧滤波:
 *          1. DWT 计时 → dt
 *          2. 读取 ACC/GYRO/TEMP
 *          3. 三重静止检测 (方差 + 幅值 + 陀螺)
 *          4. 偏置校正 + 运行时跟踪
 *          5. 自适应 α 平滑过渡
 *          6. 互补滤波器更新 (pitch/roll/yaw)
 *          7. 缓存 last_data
 * @note   使用默认全局上下文
 */
void bsp_lsm6dsr_update(bsp_lsm6dsr_data_t *data);

/**
 * @brief  获取当前陀螺零偏
 * @param[out] bx X 轴偏置 (dps)，允许 NULL
 * @param[out] by Y 轴偏置 (dps)，允许 NULL
 * @param[out] bz Z 轴偏置 (dps)，允许 NULL
 * @note   使用默认全局上下文
 */
void bsp_lsm6dsr_get_bias(float *bx, float *by, float *bz);

/**
 * @brief  获取上一帧 ACC 方差总和
 * @return 方差总和 (mg²)，用于调试静止检测门限
 * @note   使用默认全局上下文
 */
float bsp_lsm6dsr_get_last_variance(void);

/**
 * @brief  获取最新缓存数据 (只读)
 * @return 指向 last_data 的 const 指针
 * @note   使用默认全局上下文
 */
const bsp_lsm6dsr_data_t* bsp_lsm6dsr_get_data(void);

/**
 * @brief  查询静止状态
 * @return 1=静止 / 0=运动
 * @note   使用默认全局上下文
 */
int bsp_lsm6dsr_is_stationary(void);

/**
 * @brief  VOFA+ FireWater 格式化
 * @param[out] buf     输出缓冲区
 * @param[in]  buf_size 缓冲区大小
 * @param[in]  data     IMU 数据指针
 * @return 写入缓冲区的字符数 (不含 '\0')
 * @details 格式: "ax,ay,az,gx,gy,gz,pitch,roll,yaw,temp\\r\\n"
 *          10 通道逗号分隔，VOFA+ FireWater 协议。
 */
int bsp_lsm6dsr_vofa_format(char *buf, int buf_size, const bsp_lsm6dsr_data_t *data);

/**@}*/

/** @defgroup BSP_Filter_API 滤波器切换 API */
/**@{*/

/**
 * @brief  切换滤波器类型
 * @param  type  滤波器类型 (FILTER_TYPE_*)
 * @return 0=成功, -1=失败
 * @details 运行时切换滤波器会先创建新实例，成功后再销毁旧实例。
 *          新滤波器使用默认参数，可通过 bsp_lsm6dsr_set_filter_param() 调整。
 */
int bsp_lsm6dsr_set_filter(filter_type_t type);

/**
 * @brief  获取当前滤波器类型
 * @return 当前滤波器类型
 */
filter_type_t bsp_lsm6dsr_get_filter_type(void);

/**
 * @brief  设置滤波器参数
 * @param  param  参数枚举
 * @param  value  参数值
 * @return 0=成功, -1=失败
 */
int bsp_lsm6dsr_set_filter_param(filter_param_t param, float value);

/**
 * @brief  获取滤波器类型名称
 * @return 类型名称字符串
 */
const char* bsp_lsm6dsr_get_filter_name(void);

/**@}*/

#endif /* BSP_LSM6DSR_H */
