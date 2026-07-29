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

/* ======================== 共享上下文 ======================== */

/** 控制任务共享状态(控制任务写, 菜单任务读) */
typedef struct {
    int32_t rpm[BSP_MOTOR_COUNT];
    int32_t output[BSP_MOTOR_COUNT];
    float   pid_correction[BSP_MOTOR_COUNT];  /**< FF模式PID修正量 */
    float   current_ma[BSP_MOTOR_COUNT];      /**< Motor current (mA). */
    uint32_t bus_voltage_mv;                   /**< Bus voltage (mV). */
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
    uint32_t             overload_cnt[BSP_MOTOR_COUNT]; /**< Overcurrent duration ticks. */
} app_shared_ctx_t;

/**
 * @brief 电机状态快照结构。
 *
 * 保存电机速度、PID 参数和前馈状态，供 VOFA+ 等诊断接口读取。
 */
typedef struct {
    bool enabled;
    int32_t rpm;
    int32_t output;
    float target;
    float kp;
    float ki;
    float kd;
    bool ff_enabled;
    float ff_k;
    float ff_b;
} app_motor_state_snapshot_t;

/**
 * @brief 应用状态快照结构。
 *
 * 由 app_state_snapshot_read() 读取控制状态、电机状态和 IMU 数据。
 * 用于菜单、通信和诊断输出，不直接参与控制。
 */
typedef struct {
    app_control_status_t control;
    app_imu_data_t imu;
    app_motor_state_snapshot_t motor[BSP_MOTOR_COUNT];
    app_ctrl_mode_t mode;
} app_state_snapshot_t;

/**
 * @brief 读取应用层状态快照。
 * @param[in] ctx 应用共享上下文指针，不能为 NULL。
 * @param[out] snapshot 输出快照指针，不能为 NULL。
 * @retval true 读取成功。
 * @retval false 参数无效或读取失败。
 */
bool app_state_snapshot_read(const app_shared_ctx_t *ctx,
                             app_state_snapshot_t *snapshot);

/** ???????????????????????????? */
app_shared_ctx_t *app_protocol_get_context(void);


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


/** FreeRTOS 运行时故障状态：当前没有记录故障。 */
#define APP_RUNTIME_FAULT_NONE             (0U)
/** FreeRTOS 运行时故障状态：任务栈溢出。 */
#define APP_RUNTIME_FAULT_STACK_OVERFLOW   (1U)
/** FreeRTOS 运行时故障状态：动态内存分配失败。 */
#define APP_RUNTIME_FAULT_MALLOC_FAILED    (2U)

/**
 * @brief FreeRTOS 运行时资源和故障诊断快照。
 */
typedef struct {
    /** 控制任务栈历史最小剩余空间，单位为 StackType_t 个数。 */
    uint32_t control_stack_high_watermark_words;
    /** 菜单任务栈历史最小剩余空间，单位为 StackType_t 个数。 */
    uint32_t menu_stack_high_watermark_words;
    /** IMU 任务栈历史最小剩余空间，单位为 StackType_t 个数。 */
    uint32_t imu_stack_high_watermark_words;
    /** 当前 FreeRTOS heap 剩余字节数。 */
    uint32_t free_heap_bytes;
    /** FreeRTOS heap 历史最小剩余字节数。 */
    uint32_t minimum_ever_free_heap_bytes;
    /** 已记录的第一个运行时故障码。 */
    uint32_t fault_code;
} app_runtime_diag_t;

/**
 * @brief 读取当前 FreeRTOS 运行时诊断快照。
 * @param[out] out 输出诊断结构体，不能为 NULL。
 * @retval true 读取成功。
 * @retval false 参数无效。
 * @note 该接口只读取诊断数据，不阻塞，也不修改控制或 IMU 任务状态。
 */
bool app_runtime_diag_read(app_runtime_diag_t *out);

/**
 * @brief 记录不可恢复的 FreeRTOS 运行时故障。
 * @param fault_code 故障码，使用 APP_RUNTIME_FAULT_* 定义。
 * @note 该接口供异常钩子调用；禁止在其中打印、加锁、分配内存或阻塞等待。
 *       只保留首次记录的故障，避免故障现场被后续路径覆盖。
 */
void app_runtime_diag_record_fault(uint32_t fault_code);

#ifdef __cplusplus
}
#endif

#endif /* APP_MAIN_H */