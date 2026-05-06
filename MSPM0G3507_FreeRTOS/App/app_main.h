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
#include "bsp_motor.h"
#include "project_config.h"

/* ======================== 任务优先级 ======================== */

/** 控制任务优先级(最高,保证实时性) */
#define APP_TASK_PRIORITY_CONTROL   (5U)
/** 菜单任务优先级(低) */
#define APP_TASK_PRIORITY_MENU      (2U)

/* ======================== 任务栈大小 ======================== */

/** 控制任务栈大小(字, 含浮点运算) */
#define APP_TASK_STACK_CONTROL      (256U)
/** 菜单任务栈大小(字, 行缓冲+printf+sscanf+VOFA+12通道) */
#define APP_TASK_STACK_MENU         (512U)

/* ======================== 控制周期 ======================== */

/** 控制任务周期(ms) */
#define APP_CONTROL_PERIOD_MS       (5U)
/** 菜单轮询周期(ms) */
#define APP_MENU_POLL_PERIOD_MS     (100U)
/** RPM输出周期(ms, 运行模式下, 需满足: 12通道×12字节/周期 < 波特率×周期) */
#define APP_RPM_OUTPUT_PERIOD_MS    (30U)

/* ======================== 菜单配置 ======================== */

/** 行输入缓冲区大小(字节) */
#define MENU_LINE_BUF_SIZE          (64U)

/* ======================== PID默认参数 ======================== */

/** 速度环默认比例增益(空载MG310 RPM控制,保守值) */
#define APP_PID_DEFAULT_KP          (0.8f)
/** 速度环默认积分增益(空载MG310 RPM控制) */
#define APP_PID_DEFAULT_KI          (0.3f)
/** 速度环默认微分增益 */
#define APP_PID_DEFAULT_KD          (0.0f)

/* ======================== 共享上下文 ======================== */

/** 控制任务共享状态(控制任务写, 菜单任务读) */
typedef struct {
    int32_t rpm[BSP_MOTOR_COUNT];
    int32_t output[BSP_MOTOR_COUNT];
} app_control_status_t;

/** 应用层共享上下文(控制任务和菜单任务之间共享) */
typedef struct {
    app_pid_t            pid[BSP_MOTOR_COUNT];       /**< PID控制器实例 */
    bool                 motor_enabled[BSP_MOTOR_COUNT]; /**< 电机使能标志 */
    app_control_status_t status;                     /**< 控制任务状态 */
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