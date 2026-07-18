/**
 * @file    app_pid.h
 * @brief   PID控制器接口(纯算法,无硬件依赖)
 * @note    从bsp_pid.c迁移并重构,去除硬件依赖
 *          仅依赖stdint.h,可独立于硬件单元测试
 *
 *          支持两种PID模式:
 *          1. 位置式PID: output = Kp*e + Ki*∫e + Kd*de/dt
 *          2. 增量式PID: Δoutput = Kp*Δe + Ki*e + Kd*Δ²e
 *
 *          抗积分饱和: 积分项限幅,防止超调
 *          微分项滤波: 一阶低通滤波,抑制高频噪声
 */
#ifndef APP_PID_H
#define APP_PID_H

#ifdef __cplusplus
extern "C" {
#endif

/* ======================== 包含 ======================== */
#include <stdint.h>
#include <stdbool.h>

/* ======================== 类型定义 ======================== */

/** PID控制器模式 */
typedef enum {
    APP_PID_MODE_POSITION = 0, /**< 位置式PID */
    APP_PID_MODE_INCREMENT,    /**< 增量式PID */
} app_pid_mode_t;

/**
 * @brief PID控制器结构体
 * @note  所有状态通过此结构体管理,支持多实例,
 *        可重入(无全局/静态变量)
 */
typedef struct {
    /* ---- 参数(普通模式) ---- */
    float kp;              /**< 比例增益 */
    float ki;              /**< 积分增益 */
    float kd;              /**< 微分增益 */
    app_pid_mode_t mode;   /**< PID模式 */

    /* ---- 参数(FF模式) ---- */
    float ff_kp;           /**< FF模式比例增益 */
    float ff_ki;           /**< FF模式积分增益 */
    float ff_kd;           /**< FF模式微分增益 */
    bool  use_ff;          /**< 是否使用FF模式(位置式PID) */

    /* ---- 限幅 ---- */
    float out_min;         /**< 输出下限 */
    float out_max;         /**< 输出上限 */
    float integral_min;    /**< 积分项下限(抗饱和,普通模式) */
    float integral_max;    /**< 积分项上限(抗饱和,普通模式) */
    float ff_integral_min; /**< 积分项下限(抗饱和,FF模式) */
    float ff_integral_max; /**< 积分项上限(抗饱和,FF模式) */

    /* ---- 滤波 ---- */
    float d_filter_coeff;  /**< 微分滤波系数(0~1, 0=无滤波) */

    /* ---- 内部状态 ---- */
    float setpoint;        /**< 目标值 */
    float last_error;      /**< 上次偏差 */
    float last_last_error; /**< 上上次偏差(增量式) */
    float integral;        /**< 积分累计 */
    float last_derivative; /**< 上次微分项(滤波) */
    bool  is_first_run;    /**< 首次运行标志 */
} app_pid_t;

/* ======================== 函数接口 ======================== */

/**
 * @brief  初始化PID控制器
 * @param  pid      PID控制器指针
 * @param  kp       比例增益
 * @param  ki       积分增益
 * @param  kd       微分增益
 * @param  mode     PID模式(位置式/增量式)
 * @param  out_min  输出下限
 * @param  out_max  输出上限
 * @note   积分限幅自动设为输出限幅
 *         微分滤波系数默认0(无滤波)
 */
void app_pid_init(app_pid_t *pid, float kp, float ki, float kd,
                   app_pid_mode_t mode, float out_min, float out_max);

/**
 * @brief  设置PID参数(运行时调节)
 * @param  pid PID控制器指针
 * @param  kp  比例增益
 * @param  ki  积分增益
 * @param  kd  微分增益
 */
void app_pid_set_params(app_pid_t *pid, float kp, float ki,
                         float kd);

/**
 * @brief  设置积分限幅(抗饱和)
 * @param  pid         PID控制器指针
 * @param  integral_min 积分下限
 * @param  integral_max 积分上限
 */
void app_pid_set_integral_limit(app_pid_t *pid,
                                 float integral_min,
                                 float integral_max);

/**
 * @brief  设置微分滤波系数
 * @param  pid   PID控制器指针
 * @param  coeff 滤波系数(0~1, 0=无滤波, 越大滤波越强)
 */
void app_pid_set_d_filter(app_pid_t *pid, float coeff);

/**
 * @brief  设置目标值
 * @param  pid       PID控制器指针
 * @param  setpoint  目标值
 */
void app_pid_set_setpoint(app_pid_t *pid, float setpoint);

/**
 * @brief  执行一次PID计算
 * @param  pid        PID控制器指针
 * @param  feedback   当前反馈值
 * @param  dt_s       采样周期(秒), 0则使用上次间隔
 * @retval PID输出值(已限幅)
 * @note   位置式: out = Kp*e + Ki*∫e + Kd*de/dt
 *         增量式: Δout = Kp*(e-e') + Ki*e +
 *                        Kd*(e-2*e'+e'')
 */
float app_pid_compute(app_pid_t *pid, float feedback,
                       float dt_s);

/**
 * @brief  重置PID控制器状态(保留参数)
 * @param  pid PID控制器指针
 * @note   清零积分/偏差, 不改变Kp/Ki/Kd/限幅
 */
void app_pid_reset(app_pid_t *pid);

/**
 * @brief  获取上次偏差
 * @param  pid PID控制器指针
 * @retval 上次偏差值
 */
float app_pid_get_error(const app_pid_t *pid);

/**
 * @brief  获取当前积分累计值
 * @param  pid PID控制器指针
 * @retval 积分累计值
 */
float app_pid_get_integral(const app_pid_t *pid);

#ifdef __cplusplus
}
#endif

#endif /* APP_PID_H */
