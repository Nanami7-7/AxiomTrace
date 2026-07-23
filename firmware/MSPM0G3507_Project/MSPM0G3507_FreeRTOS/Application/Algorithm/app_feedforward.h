/**
 * @file    app_feedforward.h
 * @brief   前馈控制器接口(纯算法,无硬件依赖)
 * @note    前馈+PID复合控制: PID增量式从FF基线起步, 逐步修正残差和扰动
 *          启动时: pid->last_output = FF(target), PID增量补偿
 *
 *          支持两种前馈模型:
 *          1. 简单线性: FF = k × RPM + b
 *          2. 分段线性: 死区 + 线性区 (Phase 2)
 *
 *          扫频标定: 自动遍历不同RPM, 记录稳态duty, 最小二乘拟合k/b
 */
#ifndef APP_FEEDFORWARD_H
#define APP_FEEDFORWARD_H

#ifdef __cplusplus
extern "C" {
#endif

/* ======================== 包含 ======================== */
#include <stdint.h>
#include <stdbool.h>
#include "app_pid.h"

/* ======================== 常量定义 ======================== */

/** 扫频测试点数 */
#define FF_SWEEP_POINTS         (10U)

/** 每个转速稳态等待时间(ms) */
#define FF_SWEEP_SETTLE_MS      (3000U)

/** 稳态采样时间(ms) */
#define FF_SWEEP_SAMPLE_MS      (1000U)

/** 控制周期(ms, 用于计算采样次数) */
#define FF_SWEEP_CTRL_PERIOD_MS (5U)

/** 扫频采样次数 = 采样时间 / 控制周期 */
#define FF_SWEEP_SAMPLE_COUNT   (FF_SWEEP_SAMPLE_MS / FF_SWEEP_CTRL_PERIOD_MS)

/* ======================== 全局变量 ======================== */

/** Sweep 取消标志(Stop 命令设置, Sweep 循环检查) */
extern volatile bool g_sweep_cancel;

/* ======================== 类型定义 ======================== */

/** 前馈参数结构体(每电机独立) */
typedef struct {
    float k;              /**< 斜率 (duty/RPM) */
    float b;              /**< 截距 (duty) */
    float rpm_dead;       /**< 死区转速 (|RPM| < rpm_dead 时 FF=0) */
    float duty_dead;      /**< 死区duty (rpm_dead ≤ |RPM| < rpm_min 时的固定duty) */
    float rpm_min;        /**< 线性区起点 (|RPM| ≥ rpm_min 时使用线性公式) */
    bool  enabled;        /**< 前馈使能标志 */
} app_ff_params_t;

/** 扫频结果结构体 */
typedef struct {
    float rpm[FF_SWEEP_POINTS];   /**< 各测试点实际RPM */
    float duty[FF_SWEEP_POINTS];  /**< 各测试点稳态duty */
    uint32_t count;               /**< 有效数据点数 */
} app_ff_sweep_result_t;

/* ======================== 函数接口 ======================== */

/**
 * @brief  初始化前馈参数(默认禁用, k=0, b=0)
 * @param  ff 前馈参数指针
 */
void app_ff_init(app_ff_params_t *ff);

/**
 * @brief  计算前馈输出
 * @param  ff         前馈参数指针
 * @param  target_rpm 目标转速
 * @retval 前馈duty值(未限幅), 前馈禁用时返回0
 */
float app_ff_compute(const app_ff_params_t *ff, float target_rpm);

/**
 * @brief  设置前馈线性参数(k, b)
 * @param  ff 前馈参数指针
 * @param  k  斜率 (duty/RPM)
 * @param  b  截距 (duty)
 */
void app_ff_set_params(app_ff_params_t *ff, float k, float b);

/**
 * @brief  设置前馈死区参数(Phase 2)
 * @param  ff        前馈参数指针
 * @param  rpm_dead  死区转速
 * @param  duty_dead 死区duty
 * @param  rpm_min   线性区起点
 */
void app_ff_set_deadzone(app_ff_params_t *ff, float rpm_dead,
                          float duty_dead, float rpm_min);

/**
 * @brief  使能/禁用前馈
 * @param  ff      前馈参数指针
 * @param  enabled true=使能, false=禁用
 */
void app_ff_set_enabled(app_ff_params_t *ff, bool enabled);

/**
 * @brief  将前馈值应用到PID积分(电机启动/目标变化时调用)
 * @note   FF使能时: pid->last_output = FF(target)，首帧在此前馈基线上叠加比例项
 *         FF禁用时: pid->last_output = 0, is_first_run = true
 * @param  ff     前馈参数指针
 * @param  pid    PID控制器指针
 * @param  target 目标RPM
 */
void app_ff_apply_to_pid(const app_ff_params_t *ff,
                          app_pid_t *pid, float target);

/**
 * @brief  最小二乘线性拟合(从扫频结果计算k, b)
 * @param  result 扫频结果指针
 * @param  k_out  斜率输出指针
 * @param  b_out  截距输出指针
 * @retval true   拟合成功
 * @retval false  数据不足或退化
 */
bool app_ff_fit_linear(const app_ff_sweep_result_t *result,
                        float *k_out, float *b_out);

/**
 * @brief  执行前馈扫频标定(阻塞式, 需在任务中调用)
 * @note   遍历10个RPM测试点, 每点保持稳态后记录duty
 *         执行期间电机持续转动, 需确保安全
 * @param  ctx      共享上下文指针(需包含app_main.h)
 * @param  motor_id 目标电机索引
 * @param  result   扫频结果输出
 * @retval true     扫频成功
 * @retval false    扫频失败
 */
bool app_ff_sweep(void *ctx, uint32_t motor_id,
                   app_ff_sweep_result_t *result);

#ifdef __cplusplus
}
#endif

#endif /* APP_FEEDFORWARD_H */
