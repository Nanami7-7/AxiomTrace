/**
 * @file    filter.h
 * @brief   滤波器统一接口定义
 *
 * 设计理念：
 *   - 0成本抽象：函数指针调用，无虚函数表开销
 *   - 可读性：清晰的命名和注释
 *   - 可扩展性：添加新滤波器只需实现接口
 *   - 向后兼容：保留原有互补滤波器
 *
 * 支持的滤波器（7种）：
 *   - 互补滤波器 (Complementary) — 经典陀螺仪+加速度计融合，α权重
 *   - 一阶低通滤波器 (LPF) — 截止频率可调，适合噪声滤除
 *   - 误差状态卡尔曼滤波器 (ESKF) — 6状态ESKF，含陀螺偏置估计
 *   - 线性卡尔曼滤波器 (LKF) — 6状态LKF，线性近似，低算力场景
 *   - Mahony滤波器 — PI控制器互补滤波，快速收敛
 *   - Madgwick滤波器 — 梯度下降法，四元数姿态估计
 *   - 三轴标量KF — 每轴 angle+bias，最低计算量路径
 *
 * 退化模式（5种）：
 *   - NONE: 正常运行
 *   - STATIC_ONLY: 仅静态模式（禁用动态补偿）
 *   - GYRO_ONLY: ACC不可靠时仅用陀螺仪
 *   - ACC_ONLY: GYRO饱和时仅用加速度计
 *   - HOLD_LAST: 冻结输出（保持上次结果）
 *
 * 配置系统：
 *   - filter_config.h: 参数来源标注、范围验证、退化策略、预设配置
 *   - 所有参数标明来源（论文/经验值/传感器手册/调优）
 *   - 支持运行时参数验证和自动退化模式选择
 *
 * 使用示例：
 * @code
 *   #include "filter.h"
 *
 *   filter_t *f = filter_create(FILTER_TYPE_MADGWICK);
 *   filter_input_t in = { .ax=ax, .ay=ay, .az=az, .gx=gx, .gy=gy, .gz=gz, .dt=0.01 };
 *   filter_output_t out;
 *   f->update(f, &in, &out);
 *   printf("pitch=%.2f roll=%.2f yaw=%.2f\n", out.pitch, out.roll, out.yaw);
 *   f->destroy(f);
 * @endcode
 *
 * @see filter_config.h  参数配置与退化策略
 * @see bsp_lsm6dsr.h   BSP层集成接口
 */

#ifndef FILTER_H
#define FILTER_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @defgroup Filter_Error 错误处理 */
/**@{*/

/**
 * @brief 滤波器错误码
 */
typedef enum {
    FILTER_OK = 0,                    /**< 成功 */
    FILTER_ERR_INVALID_TYPE,          /**< 无效的滤波器类型 */
    FILTER_ERR_ALLOC_FAILED,          /**< 内存分配失败 */
    FILTER_ERR_INVALID_PARAM,         /**< 无效的参数 */
    FILTER_ERR_NOT_INITIALIZED,       /**< 未初始化 */
    FILTER_ERR_INVALID_INPUT,         /**< 无效的输入数据（NaN/Inf） */
    FILTER_ERR_NUMERICAL,             /**< 数值错误（如除零） */
    FILTER_ERR_QUATERNION,            /**< 四元数异常 */
} filter_error_t;

/**
 * @brief 错误信息结构体
 */
typedef struct {
    filter_error_t code;              /**< 错误码 */
    const char *message;              /**< 错误消息 */
    const char *file;                 /**< 源文件名 */
    int line;                         /**< 行号 */
} filter_error_info_t;

/**
 * @brief 错误回调函数类型
 * @param info  错误信息
 * @param user_data 用户数据
 */
typedef void (*filter_error_callback_t)(const filter_error_info_t *info, void *user_data);

/**
 * @brief 设置全局错误回调
 * @param callback  回调函数（NULL 取消）
 * @param user_data 用户数据
 */
void filter_set_error_callback(filter_error_callback_t callback, void *user_data);

/**
 * @brief 获取最后的错误信息
 * @return 错误信息结构体
 */
filter_error_info_t filter_get_last_error(void);

/**@}*/

/* ---- 滤波器类型枚举 ---- */
typedef enum {
    FILTER_TYPE_COMPLEMENTARY = 0,  /**< 现有互补滤波器 */
    FILTER_TYPE_LPF,                /**< 一阶低通滤波器 */
    FILTER_TYPE_ESKF,               /**< 误差状态卡尔曼滤波器 (MCU优化) */
    FILTER_TYPE_LKF,                /**< 线性卡尔曼滤波器 */
    FILTER_TYPE_MAHONY,             /**< Mahony 互补滤波器 */
    FILTER_TYPE_MADGWICK,           /**< Madgwick 梯度下降滤波器 */
    FILTER_TYPE_KF,                 /**< 纯卡尔曼滤波器 (3×标量, 每轴: angle+bias) */
    FILTER_TYPE_COUNT               /**< 滤波器数量（勿用） */
} filter_type_t;

/* ---- 滤波器输入数据 ---- */
typedef struct {
    float ax, ay, az;   /**< 加速度 (g) */
    float gx, gy, gz;   /**< 角速度 (dps) */
    float dt;           /**< 时间差 (秒；MSPM0 原生单精度路径) */
} filter_input_t;

/* ---- 退化模式（用于传感器数据质量差时） ---- */
typedef enum {
    FILTER_DEGRADE_NONE = 0,        /**< 正常运行 */
    FILTER_DEGRADE_STATIC_ONLY,     /**< 仅静态模式（禁用动态补偿） */
    FILTER_DEGRADE_GYRO_ONLY,       /**< 仅陀螺仪（禁用ACC修正） */
    FILTER_DEGRADE_ACC_ONLY,        /**< 仅加速度计（禁用GYRO积分） */
    FILTER_DEGRADE_HOLD_LAST,       /**< 保持上次输出（冻结） */
    FILTER_DEGRADE_COUNT
} filter_degrade_t;

/* ---- 滤波器输出数据 ---- */
typedef struct {
    float pitch;        /**< 俯仰角 (度) */
    float roll;         /**< 横滚角 (度) */
    float yaw;          /**< 偏航角 (度) */
    float q0, q1, q2, q3;  /**< 四元数（可选，某些滤波器内部使用） */
} filter_output_t;

/* ---- 滤波器参数枚举（用于 set_param） ---- */
typedef enum {
    FILTER_PARAM_ALPHA = 0,         /**< 互补滤波器 α */
    FILTER_PARAM_CUTOFF_FREQ,       /**< 低通滤波器截止频率 (Hz) */
    FILTER_PARAM_Q_ANGLE,           /**< ESKF/KF/LKF 过程噪声-角度 */
    FILTER_PARAM_Q_BIAS,            /**< ESKF/KF/LKF 过程噪声-偏置 */
    FILTER_PARAM_R_MEASURE,         /**< ESKF/KF/LKF 测量噪声 */
    FILTER_PARAM_KP,                /**< Mahony/Madgwick 比例增益 */
    FILTER_PARAM_KI,                /**< Mahony/Madgwick 积分增益 */
    FILTER_PARAM_BIAS_LIMIT_DPS,    /**< ESKF 偏置幅值限制 (dps) */
    FILTER_PARAM_CHI2_THRESHOLD,    /**< [已废弃] 原 EKF Chi-squared 门限 */
    FILTER_PARAM_R_ADAPT_ENABLE,    /**< [已废弃] 原 EKF 动态 R 适配使能 */
    FILTER_PARAM_R_ADAPT_FACTOR,    /**< [已废弃] 原 EKF 动态 R 缩放因子 */

    /* ---- KF 纯卡尔曼滤波器参数 ---- */
    FILTER_PARAM_KF_Q_ANGLE,        /**< KF 过程噪声-角度 (deg²/s) */
    FILTER_PARAM_KF_Q_BIAS,         /**< KF 过程噪声-偏置 (dps²/s) */
    FILTER_PARAM_KF_R_MEASURE,      /**< KF 测量噪声 (deg²) */
    FILTER_PARAM_KF_R_ZUPT,         /**< KF ZUPT 伪测量噪声 (dps²), 1e6=禁用 */

    FILTER_PARAM_COUNT              /**< 参数数量（勿用） */
} filter_param_t;

/* ---- 滤波器接口（前向声明） ---- */
typedef struct filter filter_t;

/** Maximum storage required by any built-in filter implementation. */
#define FILTER_STATIC_STORAGE_SIZE (512U)

/**
 * @brief Aligned caller-owned storage for heap-free filter construction.
 *
 * The union guarantees alignment for pointers, doubles and all current private
 * filter states on both the MSPM0 target and host-test platforms.
 */
typedef union {
    void    *align_ptr;
    double   align_double;
    uint8_t  bytes[FILTER_STATIC_STORAGE_SIZE];
} filter_static_storage_t;

/**
 * @brief 滤波器更新函数类型
 * @param self  滤波器实例指针
 * @param in    输入数据（加速度、角速度、dt）
 * @param out   输出数据（姿态角、四元数）
 */
typedef void (*filter_update_fn)(filter_t *self, const filter_input_t *in, filter_output_t *out);

/**
 * @brief 滤波器重置函数类型
 * @param self  滤波器实例指针
 */
typedef void (*filter_reset_fn)(filter_t *self);

/**
 * @brief 滤波器参数设置函数类型
 * @param self  滤波器实例指针
 * @param param 参数枚举
 * @param value 参数值
 */
typedef void (*filter_set_param_fn)(filter_t *self, filter_param_t param, float value);

/**
 * @brief 滤波器销毁函数类型
 * @param self  滤波器实例指针
 */
typedef void (*filter_destroy_fn)(filter_t *self);

/* ---- 数值安全保护配置 ---- */
typedef struct {
    float angle_min;        /**< 角度最小值 (度) */
    float angle_max;        /**< 角度最大值 (度) */
    float q_norm_thresh;    /**< 四元数归一化阈值 */
    float cov_reg_factor;   /**< 协方差矩阵正则化因子 */
    uint16_t norm_interval; /**< 四元数归一化间隔（帧数） */
} filter_safety_config_t;

/* 默认安全配置 */
#define FILTER_SAFETY_DEFAULT { \
    .angle_min = -180.0f, \
    .angle_max =  180.0f, \
    .q_norm_thresh = 0.001f, \
    .cov_reg_factor = 1e-6f, \
    .norm_interval = 10 \
}

/* ---- 滤波器接口结构体 ---- */
struct filter {
    filter_update_fn    update;     /**< 更新函数 */
    filter_reset_fn     reset;      /**< 重置函数 */
    filter_set_param_fn set_param;  /**< 参数设置函数 */
    filter_destroy_fn   destroy;    /**< 销毁函数（释放私有数据） */
    filter_type_t       type;       /**< 滤波器类型 */
    filter_degrade_t    degrade;    /**< 退化模式（传感器数据异常时降级） */
    void               *priv;       /**< 私有数据指针 */
    uint8_t             is_static;  /**< 是否静态分配（1=静态，0=动态） */
    filter_safety_config_t safety_config; /**< 安全保护配置 */
};


/* ---- 工厂函数 ---- */

/**
 * @brief 创建滤波器实例
 * @param type  滤波器类型
 * @return 滤波器指针，失败返回 NULL
 *
 * 使用示例：
 * @code
 *   filter_t *f = filter_create(FILTER_TYPE_MADGWICK);
 *   if (!f) { error handling... }
 *
 *   filter_input_t in = { .ax=ax, .ay=ay, .az=az, .gx=gx, .gy=gy, .gz=gz, .dt=dt };
 *   filter_output_t out;
 *   f->update(f, &in, &out);
 *
 *   printf("pitch=%.2f roll=%.2f yaw=%.2f\n", out.pitch, out.roll, out.yaw);
 *
 *   f->destroy(f);  // 释放资源
 * @endcode
 */
filter_t* filter_create(filter_type_t type);

/**
 * @brief 静态创建滤波器实例（无动态内存分配）
 * @param type  滤波器类型
 * @param buf   预分配的缓冲区（大小需 >= filter_get_static_size(type)）
 * @param buf_size  缓冲区大小
 * @return 滤波器指针，失败返回 NULL
 *
 * MCU友好：不使用malloc/free，适合无MMU的嵌入式系统
 */
filter_t* filter_create_static(filter_type_t type, void *buf, size_t buf_size);

/**
 * @brief 获取静态分配所需的缓冲区大小
 * @param type  滤波器类型
 * @return 所需字节数
 */
size_t filter_get_static_size(filter_type_t type);

/**
 * @brief Validate input, run one update and publish only a valid output.
 * @return FILTER_OK on success, otherwise a filter_error_t code.
 */
filter_error_t filter_update_checked(filter_t *f,
                                     const filter_input_t *in,
                                     filter_output_t *out);

/** Validate a parameter before dispatching it to the implementation. */
filter_error_t filter_set_param_checked(filter_t *f,
                                        filter_param_t param,
                                        float value);

/** Return 1 when the selected implementation supports the parameter. */
int filter_supports_param(filter_type_t type, filter_param_t param);

/**
 * @brief 设置滤波器安全配置
 * @param f       滤波器实例
 * @param config  安全配置
 */
void filter_set_safety_config(filter_t *f, const filter_safety_config_t *config);

/**
 * @brief 验证输出有效性
 * @param out  输出数据
 * @return 1=有效, 0=无效（包含NaN/Inf）
 */
int filter_validate_output(const filter_output_t *out);

/**
 * @brief 四元数归一化
 * @param q0, q1, q2, q3  四元数（输入/输出）
 */
void filter_normalize_quaternion(float *q0, float *q1, float *q2, float *q3);

/**
 * @brief 协方差矩阵正则化（确保正定性）
 * @param P       协方差矩阵
 * @param size    矩阵维度
 * @param factor  正则化因子
 */
void filter_regularize_covariance(float P[][7], int size, float factor);
/**
 * @brief 获取滤波器类型名称（用于调试）
 * @param type  滤波器类型
 * @return 类型名称字符串
 */
const char* filter_type_name(filter_type_t type);

/**
 * @brief 设置滤波器退化模式
 * @param f       滤波器实例
 * @param degrade 退化模式
 *
 * 当传感器数据质量差时，可以降级滤波器运行模式：
 * - FILTER_DEGRADE_NONE: 正常运行
 * - FILTER_DEGRADE_GYRO_ONLY: ACC不可靠时仅用陀螺仪
 * - FILTER_DEGRADE_ACC_ONLY: GYRO饱和时仅用加速度计
 * - FILTER_DEGRADE_HOLD_LAST: 冻结输出
 */
void filter_set_degrade(filter_t *f, filter_degrade_t degrade);

/**
 * @brief 安全销毁滤波器
 * @param f  滤波器实例指针（可为 NULL）
 *
 * 推荐使用此函数替代直接调用 f->destroy(f)：
 * - 自动处理 NULL 指针（安全）
 * - 自动检测静态分配（只重置状态，不释放内存）
 * - 自动检测 NULL destroy 函数指针
 */
void filter_destroy_safe(filter_t *f);

/**
 * @brief 获取退化模式名称
 * @param degrade 退化模式
 * @return 名称字符串
 */
const char* filter_degrade_name(filter_degrade_t degrade);

/**
 * @brief 设置 KF 是否使用 MATHACL 硬件加速路径
 * @param f       滤波器实例（必须是 FILTER_TYPE_KF）
 * @param enable  1=启用硬件加速，0=使用纯软浮点
 *
 * @note 仅在 BSP_MATHACL_ENABLE 已定义且硬件路径编译进工程时生效。
 *       对非 KF 类型调用会被安全忽略。
 */
void filter_kf_set_hw(filter_t *f, int enable);

/**
 * @brief 查询 KF 硬件加速是否启用
 * @param f  滤波器实例
 * @return   1=硬件加速已启用，0=未启用（包括非 KF 类型）
 */
int filter_kf_get_hw(const filter_t *f);

/**
 * @brief ESKF 硬件加速兼容接口（当前实现保留为 no-op）
 * @param f       滤波器实例（必须是 FILTER_TYPE_ESKF）
 * @param enable  1=启用硬件加速，0=使用纯软浮点
 *
 * @note ESKF 的稀疏浮点路径当前不使用 MATHACL；保留接口避免破坏调用方。
 */
void filter_eskf_set_hw(filter_t *f, int enable);

/**
 * @brief 查询 ESKF 硬件加速是否启用（当前恒为 0）
 * @param f  滤波器实例
 * @return   1=硬件加速已启用，0=未启用（包括非 ESKF 类型）
 */
int filter_eskf_get_hw(const filter_t *f);

/**
 * @brief ESKF 诊断信息结构体
 * 简化版：偏置估计 + 协方差对角线
 */
typedef struct {
    float bias_x, bias_y, bias_z;   /**< 陀螺偏置估计 (dps) */
    float p00, p11, p22;            /**< 角度误差协方差对角线 */
    float p33, p44, p55;            /**< 偏置误差协方差对角线 */
} filter_eskf_diag_t;

/**
 * @brief 获取 ESKF 诊断信息
 * @param f    滤波器实例 (必须是 ESKF 类型)
 * @param diag 输出诊断结构体指针
 * @return 0=成功, -1=非ESKF类型或NULL
 */
int filter_eskf_get_diag(const filter_t *f, filter_eskf_diag_t *diag);

/**
 * @brief KF 诊断信息结构体
 * 3轴独立偏置估计 + 协方差对角线
 */
typedef struct {
    float bias_x, bias_y, bias_z;   /**< 3轴偏置估计 (dps) */
    float p00_x, p00_y, p00_z;     /**< 3轴角度协方差 (deg²) */
    float p11_x, p11_y, p11_z;     /**< 3轴偏置协方差 (dps²) */
} filter_kf_diag_t;

/**
 * @brief 获取 KF 诊断信息 (偏置/协方差)
 * @param f    滤波器实例 (必须是 KF 类型)
 * @param diag 输出诊断结构体指针
 * @return 0=成功, -1=非KF类型或NULL
 */
int filter_kf_get_diag(const filter_t *f, filter_kf_diag_t *diag);

int filter_check_acc_quality(float ax, float ay, float az);

/**
 * @brief 评估陀螺仪数据质量
 * @param gx, gy, gz  角速度（dps）
 * @return 1=正常（未饱和）, 0=异常
 */
int filter_check_gyro_quality(float gx, float gy, float gz);

#ifdef __cplusplus
}
#endif

#endif /* FILTER_H */
