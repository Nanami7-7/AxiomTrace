/**
 * @file    app_position_control.h
 * @brief   位置-速度串级控制接口(纯算法,无硬件依赖)
 * @note    在现有速度环(5ms,增量式PID)之上增加位置环/角度环(20ms).
 *
 *          三种模式(互斥):
 *          1. SPEED:    速度模式(现有), 用户直接设置目标RPM
 *          2. POSITION: 位移模式, 梯形规划+位置环, 控制直行距离
 *          3. ANGLE:    角度模式, 梯形规划+角度环, 控制转向角度
 *
 *          控制架构(POSITION模式):
 *          梯形规划器(20ms) → planned_speed
 *                              ↓
 *          位置环PID(20ms)  → speed_correction
 *                              ↓
 *          target_rpm = planned_speed + correction (限幅)
 *                              ↓
 *          速度环PID(5ms)   → duty
 *
 *          控制架构(ANGLE模式, 差速转向):
 *          梯形规划器(20ms) → planned_rate (角速度→轮速)
 *                              ↓
 *          角度环PID(20ms)  → yaw_correction (差速修正)
 *                              ↓
 *          left_target  = planned_rate - yaw_correction
 *          right_target = planned_rate + yaw_correction
 *                              ↓
 *          速度环PID(5ms) × 4 → duty
 *
 *          反馈源:
 *          - 位置反馈: 编码器累计脉冲(4轮平均)
 *          - 角度反馈: KF 估计的 yaw(度)
 *
 *          P0 模式切换保护:
 *          切换瞬间外环PID复位, 速度环setpoint保持当前RPM,
 *          经 APP_MODE_TRANSITION_MS 渐变到新目标.
 *
 *          到位判定:
 *          |error| < threshold 持续 N 个周期(200ms) → reached=true
 *          到位后电机停转, 保持位置模式等待下一条命令.
 */
#ifndef APP_POSITION_CONTROL_H
#define APP_POSITION_CONTROL_H

#ifdef __cplusplus
extern "C" {
#endif

/* ======================== 包含 ======================== */
#include <stdint.h>
#include <stdbool.h>
#include "app_pid.h"
#include "app_planner.h"

/* ======================== 常量定义 ======================== */

/** 电机数量(4轮) */
#define APP_POS_MOTOR_COUNT     (4U)

/** 车轮逻辑索引；顺序与BSP编码器LF/LB/RF/RB一致，不是DRV8870 A/B/C/D顺序。 */
#define APP_POS_MOTOR_LF        (0U)  /**< 左前 */
#define APP_POS_MOTOR_LB        (1U)  /**< 左后 */
#define APP_POS_MOTOR_RF        (2U)  /**< 右前 */
#define APP_POS_MOTOR_RB        (3U)  /**< 右后 */

/* ======================== 类型定义 ======================== */

/** 控制模式 */
typedef enum {
    APP_CTRL_MODE_SPEED = 0,    /**< 速度模式(现有,默认) */
    APP_CTRL_MODE_POSITION,     /**< 位移模式 */
    APP_CTRL_MODE_ANGLE,        /**< 角度模式 */
} app_ctrl_mode_t;

/** 模式切换阶段 */
typedef enum {
    APP_CTRL_PHASE_STABLE = 0,      /**< 稳定运行 */
    APP_CTRL_PHASE_TRANSITION,      /**< 模式切换过渡中 */
} app_ctrl_phase_t;

/** 位置控制输出(供 task_control 读取) */
typedef struct {
    float target_rpm[APP_POS_MOTOR_COUNT]; /**< 4路目标RPM */
    float planned_speed;                   /**< 规划器输出速度 */
    float planned_pos;                     /**< 规划器输出位置 */
    float pos_correction;                  /**< 位置环修正 */
    float yaw_correction;                  /**< 角度环修正 */
    bool  reached;                         /**< 到位标志 */
    app_ctrl_mode_t mode;                  /**< 当前模式 */
    app_planner_state_t planner_state;     /**< 规划器状态 */
} app_pos_output_t;

/**
 * @brief 位置-速度串级控制器
 */
typedef struct {
    /* ---- 模式管理 ---- */
    app_ctrl_mode_t  mode;             /**< 当前模式 */
    app_ctrl_mode_t  prev_mode;        /**< 切换前模式 */
    app_ctrl_phase_t phase;            /**< 切换阶段 */
    uint32_t         transition_tick;  /**< 切换起始tick */
    uint32_t         transition_ms;    /**< 过渡时长(ms) */
    float            transition_rpm;   /**< 切换时记录的RPM */

    /* ---- 梯形规划器 ---- */
    app_planner_t planner;

    /* ---- 位置环PID(POSITION模式) ---- */
    app_pid_t pos_pid;
    float     pos_correction;          /**< 位置环输出修正 */

    /* ---- 角度环PID(ANGLE模式) ---- */
    app_pid_t yaw_pid;
    float     yaw_correction;          /**< 角度环输出修正 */

    /* ---- 到位检测 ---- */
    bool     reached;                  /**< 到位标志 */
    uint32_t reached_count;            /**< 到位持续计数 */
    uint32_t reached_threshold_count;  /**< 到位所需周期数 */
    float    reached_threshold;        /**< 到位误差阈值 */

    /* ---- 启动基准 ---- */
    float pos_start;                   /**< 位置启动基准(脉冲) */
    float yaw_start;                   /**< 角度启动基准(度) */
    float cruise_speed;                /**< 当前巡航速度 */
    uint32_t pulses_per_rev;           /**< 每转脉冲数(RPM↔脉冲/s转换) */

    /* ---- 输出 ---- */
    app_pos_output_t output;
} app_position_ctrl_t;

/* ======================== 函数接口 ======================== */

/**
 * @brief  初始化位置控制器
 * @param  ctrl    控制器指针
 * @param  pos_kp  位置环 Kp
 * @param  pos_ki  位置环 Ki
 * @param  pos_kd  位置环 Kd
 * @param  yaw_kp  角度环 Kp
 * @param  yaw_ki  角度环 Ki
 * @param  yaw_kd  角度环 Kd
 * @param  accel   规划器加速度(RPM/s, 内部转脉冲/s²)
 * @param  max_rpm 最大目标RPM(速度限幅)
 * @param  pulses_per_rev 每转脉冲数(RPM↔脉冲/s转换)
 * @note   默认模式为 SPEED(不影响现有速度环)
 */
void app_posctrl_init(app_position_ctrl_t *ctrl,
                      float pos_kp, float pos_ki, float pos_kd,
                      float yaw_kp, float yaw_ki, float yaw_kd,
                      float accel, float max_rpm,
                      uint32_t pulses_per_rev);

/**
 * @brief  切换控制模式
 * @param  ctrl    控制器指针
 * @param  mode    目标模式
 * @param  current_rpm 当前4路RPM(用于过渡, NULL则用0)
 * @param  now_tick 当前系统tick(ms)
 * @note   触发模式切换过渡(渐变)
 *         SPEED→POSITION/ANGLE: 外环PID复位, 速度环保持
 *         POSITION/ANGLE→SPEED: 外环PID复位
 */
void app_posctrl_set_mode(app_position_ctrl_t *ctrl,
                           app_ctrl_mode_t mode,
                           const float current_rpm[],
                           uint32_t now_tick);

/**
 * @brief  启动位置控制(直行指定距离)
 * @param  ctrl          控制器指针
 * @param  target_pulses 目标位移(脉冲, 正=前进, 负=后退)
 * @param  cruise_speed  巡航速度(RPM, >0)
 * @param  current_pos   当前编码器脉冲(作为基准)
 * @note   仅在 POSITION 模式有效, 否则忽略
 */
void app_posctrl_start_position(app_position_ctrl_t *ctrl,
                                float target_pulses,
                                float cruise_speed,
                                float current_pos);

/**
 * @brief  启动角度控制(转向指定角度)
 * @param  ctrl          控制器指针
 * @param  target_angle  目标角度(度, 正=左转, 负=右转)
 * @param  cruise_speed  巡航速度(RPM, >0)
 * @param  current_yaw   当前KF yaw(度, 作为基准)
 * @note   仅在 ANGLE 模式有效, 否则忽略
 */
void app_posctrl_start_angle(app_position_ctrl_t *ctrl,
                              float target_angle,
                              float cruise_speed,
                              float current_yaw);

/**
 * @brief  位置控制器周期更新(20ms调用)
 * @param  ctrl          控制器指针
 * @param  encoder_avg   编码器平均脉冲(POSITION模式反馈)
 * @param  kf_yaw        KF估计yaw(ANGLE模式反馈)
 * @param  dt_s          时间步长(秒, 推荐0.02)
 * @param  now_tick      当前系统tick(ms)
 * @retval 输出指针(指向 ctrl->output, 4路目标RPM)
 * @note   SPEED模式不修改target_rpm(由用户直接设置速度环)
 *         POSITION/ANGLE模式输出target_rpm供速度环跟踪
 */
const app_pos_output_t *app_posctrl_update(app_position_ctrl_t *ctrl,
                                            float encoder_avg,
                                            float kf_yaw,
                                            float dt_s,
                                            uint32_t now_tick);

/**
 * @brief  查询是否到位
 * @param  ctrl 控制器指针
 * @retval true=已到位(POSITION/ANGLE模式)
 */
bool app_posctrl_is_reached(const app_position_ctrl_t *ctrl);

/**
 * @brief  获取当前模式
 * @param  ctrl 控制器指针
 * @retval 当前模式
 */
app_ctrl_mode_t app_posctrl_get_mode(const app_position_ctrl_t *ctrl);

/**
 * @brief  设置位置环PID参数(运行时调参)
 * @param  ctrl 控制器指针
 * @param  kp Ki kd
 */
void app_posctrl_set_pos_pid(app_position_ctrl_t *ctrl,
                              float kp, float ki, float kd);

/**
 * @brief  设置角度环PID参数(运行时调参)
 * @param  ctrl 控制器指针
 * @param  kp Ki kd
 */
void app_posctrl_set_yaw_pid(app_position_ctrl_t *ctrl,
                              float kp, float ki, float kd);

/**
 * @brief  设置规划器加速度
 * @param  ctrl  控制器指针
 * @param  accel 加速度(>0)
 */
void app_posctrl_set_accel(app_position_ctrl_t *ctrl, float accel);

/**
 * @brief  紧急停止(中止当前运动)
 * @param  ctrl 控制器指针
 * @note   规划器abort, 外环PID复位, 切回SPEED模式
 */
void app_posctrl_emergency_stop(app_position_ctrl_t *ctrl);

#ifdef __cplusplus
}
#endif

#endif /* APP_POSITION_CONTROL_H */
