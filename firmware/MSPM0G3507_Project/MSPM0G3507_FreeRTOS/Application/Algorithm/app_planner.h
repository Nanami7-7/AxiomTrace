/**
 * @file    app_planner.h
 * @brief   梯形速度规划器接口(纯算法,无硬件依赖)
 * @note    用于位置-速度串级控制的外层速度曲线生成.
 *          生成梯形速度曲线: 加速段→巡航段→减速段→到位保持.
 *
 *          工作流程:
 *          1. start(target, cruise_speed) 启动规划
 *          2. 每个控制周期调用 update(dt) 获取当前规划速度
 *          3. is_done() 检查是否到达目标
 *
 *          特性:
 *          - 支持正负目标(前进/后退, 左转/右转)
 *          - 短距离自动跳过巡航段(三角速度曲线)
 *          - 无 sqrt 运算(减速距离用 v²/(2a) 公式)
 *          - NaN/Inf 输入保护
 *          - 可重入(无全局/静态变量)
 *
 *          坐标系:
 *          - 启动时 current_pos=0, current_speed=0
 *          - target 为相对位移(脉冲)或相对角度(度)
 *          - cruise_speed 符号自动跟随 target
 */
#ifndef APP_PLANNER_H
#define APP_PLANNER_H

#ifdef __cplusplus
extern "C" {
#endif

/* ======================== 包含 ======================== */
#include <stdint.h>
#include <stdbool.h>

/* ======================== 类型定义 ======================== */

/** 规划器状态 */
typedef enum {
    APP_PLANNER_STATE_IDLE = 0,  /**< 空闲(未启动) */
    APP_PLANNER_STATE_ACCEL,      /**< 加速段 */
    APP_PLANNER_STATE_CRUISE,     /**< 巡航段 */
    APP_PLANNER_STATE_DECEL,     /**< 减速段 */
    APP_PLANNER_STATE_DONE       /**< 完成(保持位置) */
} app_planner_state_t;

/**
 * @brief 梯形速度规划器
 * @note  单位由调用者决定(脉冲/度), 但 accel/cruise_speed/dt 必须一致
 */
typedef struct {
    /* ---- 配置参数 ---- */
    float accel;          /**< 加速度(单位/s², 恒为正) */
    float out_min;        /**< 输出速度下限(负值) */
    float out_max;        /**< 输出速度上限(正值) */
    float done_threshold; /**< 到位阈值(单位: 脉冲或度) */

    /* ---- 目标参数(start 时设置) ---- */
    float target;         /**< 目标位置(相对, 可正可负) */
    float cruise_speed;   /**< 巡航速度(符号跟随target) */

    /* ---- 内部状态 ---- */
    app_planner_state_t state;
    float current_pos;    /**< 当前规划位置(从0开始累积) */
    float current_speed;  /**< 当前规划速度 */
} app_planner_t;

/* ======================== 函数接口 ======================== */

/**
 * @brief  初始化规划器
 * @param  p         规划器指针
 * @param  accel     加速度(>0, 单位/s²)
 * @param  out_min   输出速度下限(负值)
 * @param  out_max   输出速度上限(正值)
 * @note   done_threshold 默认为 5.0 (脉冲或度)
 */
void app_planner_init(app_planner_t *p, float accel,
                      float out_min, float out_max);

/**
 * @brief  启动一次梯形规划
 * @param  p             规划器指针
 * @param  target        目标位置(相对位移, 可正可负)
 * @param  cruise_speed  巡航速度(>0, 符号自动跟随target)
 * @note   清零内部状态, 进入加速段
 *         若 |target| < done_threshold, 直接进入 DONE
 */
void app_planner_start(app_planner_t *p, float target,
                       float cruise_speed);

/**
 * @brief  执行一次规划更新
 * @param  p   规划器指针
 * @param  dt  时间步长(秒, >0)
 * @retval 当前规划速度
 * @note   同时更新 current_pos
 *         IDLE/DONE 状态返回 0
 */
float app_planner_update(app_planner_t *p, float dt);

/**
 * @brief  查询是否到达目标
 * @param  p 规划器指针
 * @retval true=已到达(DONE状态)
 */
bool app_planner_is_done(const app_planner_t *p);

/**
 * @brief  获取当前规划位置
 * @param  p 规划器指针
 * @retval 当前规划位置(相对启动点)
 */
float app_planner_get_pos(const app_planner_t *p);

/**
 * @brief  获取当前规划速度
 * @param  p 规划器指针
 * @retval 当前规划速度
 */
float app_planner_get_speed(const app_planner_t *p);

/**
 * @brief  获取当前状态
 * @param  p 规划器指针
 * @retval 当前状态枚举
 */
app_planner_state_t app_planner_get_state(const app_planner_t *p);

/**
 * @brief  设置到位阈值
 * @param  p         规划器指针
 * @param  threshold 到位阈值(>0)
 */
void app_planner_set_done_threshold(app_planner_t *p,
                                     float threshold);

/**
 * @brief  重置规划器到 IDLE 状态
 * @param  p 规划器指针
 * @note   清零位置和速度, 保留参数(accel/限幅/阈值)
 */
void app_planner_reset(app_planner_t *p);

/**
 * @brief  紧急中止(立即停止)
 * @param  p 规划器指针
 * @note   速度归零, 位置保持当前值, 进入 DONE
 *         用于异常情况(过冲/外部停止命令)
 */
void app_planner_abort(app_planner_t *p);

#ifdef __cplusplus
}
#endif

#endif /* APP_PLANNER_H */
