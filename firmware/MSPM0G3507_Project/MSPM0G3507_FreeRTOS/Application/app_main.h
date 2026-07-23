/**
 * @file    app_main.h
 * @brief   应用层主接口
 * @note    应用层入口: 创建FreeRTOS任务/裸机主循环
 *          硬件/RTOS无关,仅依赖BSP和OSAL接口
 *
 *          默认任务:
 *          - 控制任务(control_task): 电机PID闭环, 5ms周期
 *          - 菜单任务(menu_task): 串口交互菜单+LED心跳
 */
#ifndef APP_MAIN_H
#define APP_MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* ======================== 包含 ======================== */
#include <stdint.h>
#include <stdbool.h>
#include "app_pid.h"
#include "app_feedforward.h"
#include "app_position_control.h"
#include "bsp_motor.h"
#include "project_config.h"

/* ======================== 兼容别名 ======================== */
/* 任务优先级/栈大小/周期等配置已迁移到 project_config.h */
#define APP_TASK_PRIORITY_CONTROL   TASK_PRIO_CONTROL
#define APP_TASK_PRIORITY_IMU       TASK_PRIO_IMU
#define APP_TASK_PRIORITY_MENU      TASK_PRIO_MENU
#define APP_TASK_STACK_CONTROL      TASK_STACK_CONTROL
#define APP_TASK_STACK_IMU          TASK_STACK_IMU
#define APP_TASK_STACK_MENU         TASK_STACK_MENU
#define APP_CONTROL_PERIOD_MS       CONTROL_PERIOD_MS
#define APP_MENU_POLL_PERIOD_MS     MENU_POLL_PERIOD_MS
#define APP_RPM_OUTPUT_PERIOD_MS    RPM_OUTPUT_PERIOD_MS
#define APP_PID_DEFAULT_KP          PID_KP_DEFAULT
#define APP_PID_DEFAULT_KI          PID_KI_DEFAULT
#define APP_PID_DEFAULT_KD          PID_KD_DEFAULT
#define APP_FF_PID_DEFAULT_KP       FF_PID_KP_DEFAULT
#define APP_FF_PID_DEFAULT_KI       FF_PID_KI_DEFAULT
#define APP_FF_PID_DEFAULT_KD       FF_PID_KD_DEFAULT

/* 位置-速度控制参数兼容别名 (原在 project_config.h 中以 APP_ 前缀定义) */
#define APP_POS_PID_KP              POS_PID_KP
#define APP_POS_PID_KI              POS_PID_KI
#define APP_POS_PID_KD              POS_PID_KD
#define APP_YAW_PID_KP              YAW_PID_KP
#define APP_YAW_PID_KI              YAW_PID_KI
#define APP_YAW_PID_KD              YAW_PID_KD
#define APP_PLANNER_ACCEL           PLANNER_ACCEL
#define APP_PLANNER_MAX_RPM         PLANNER_MAX_RPM
#define APP_REACHED_THRESHOLD_POS   REACHED_THRESHOLD_POS
#define APP_REACHED_THRESHOLD_YAW   REACHED_THRESHOLD_YAW
#define APP_REACHED_COUNT           REACHED_COUNT
#define APP_MODE_TRANSITION_MS      MODE_TRANSITION_MS

/* ======================== 共享上下文 ======================== */

/** 控制任务共享状态(控制任务写, 菜单任务读) */
typedef struct {
    int32_t rpm[BSP_MOTOR_COUNT];
    int32_t output[BSP_MOTOR_COUNT];
    float   pid_correction[BSP_MOTOR_COUNT];  /**< FF模式PID修正量 */
} app_control_status_t;

/** IMU姿态数据(由IMU任务写入, 控制/菜单任务读取) */
typedef struct {
    float roll;              /**< 横滚角(度) */
    float pitch;             /**< 俯仰角(度) */
    float yaw;               /**< 偏航角(度) */
    float accel_x_g;         /**< X轴加速度(g) */
    float accel_y_g;         /**< Y轴加速度(g) */
    float accel_z_g;         /**< Z轴加速度(g) */
    float gyro_x_dps;        /**< X轴角速度(°/s) */
    float gyro_y_dps;        /**< Y轴角速度(°/s) */
    float gyro_z_dps;        /**< Z轴角速度(°/s) */
    float temperature;       /**< 温度(°C) */
    uint32_t timestamp_ms;   /**< 时间戳(ms) */
} app_imu_data_t;

/** 应用层共享上下文(控制任务和菜单任务之间共享) */
typedef struct app_shared_ctx_s {
    app_pid_t            pid[BSP_MOTOR_COUNT];       /**< PID控制器实例 */
    app_ff_params_t      ff[BSP_MOTOR_COUNT];        /**< 前馈参数实例 */
    bool                 motor_enabled[BSP_MOTOR_COUNT]; /**< 电机使能标志 */
    app_control_status_t status;                     /**< 控制任务状态 */
    app_imu_data_t       imu;                        /**< IMU姿态数据 */
    app_position_ctrl_t  posctrl;                    /**< 位置-速度串级控制器 */
} app_shared_ctx_t;

/* ======================== 函数接口 ======================== */

/**
 * @brief  应用层初始化
 * @note   创建FreeRTOS任务, 必须在调度器启动前调用
 *         初始化各BSP模块, 创建控制/菜单任务
 * @retval 0 成功, 非0 失败
 */
int32_t app_main_init(void);

/**
 * @brief  停止单个电机(禁用+PID重置+刹车)
 * @param  ctx        共享上下文指针
 * @param  motor_idx  电机索引(0~BSP_MOTOR_COUNT-1)
 */
void app_motor_stop(app_shared_ctx_t *ctx, uint32_t motor_idx);

/**
 * @brief  停止所有电机(禁用+PID重置+刹车)
 * @param  ctx  共享上下文指针
 */
void app_motor_stop_all(app_shared_ctx_t *ctx);

#ifdef __cplusplus
}
#endif

#endif /* APP_MAIN_H */
