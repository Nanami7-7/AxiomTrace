/**
 * @file    task_control.h
 * @brief   控制任务接口
 * @note    5ms周期: 读取编码器→M法测速RPM→PID计算→设置电机duty
 */
#ifndef TASK_CONTROL_H
#define TASK_CONTROL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief  控制任务函数
 * @note   2ms周期(500Hz): 读取编码器→测速RPM→PID计算→设置电机命令
 * @param  param 任务参数(app_shared_ctx_t指针)
 */
void app_control_task(void *param);

/** PID调参阶段。 */
typedef enum {
    APP_PID_TUNE_PHASE_IDLE = 0,
    APP_PID_TUNE_PHASE_SETTLE,
    APP_PID_TUNE_PHASE_STEP,
    APP_PID_TUNE_PHASE_COMPLETE
} app_pid_tune_phase_t;

/** PID调参状态与专用遥测快照。 */
typedef struct {
    bool active;
    app_pid_tune_phase_t phase;
    uint32_t motor_id;
    uint32_t elapsed_ms;
    float requested_target_rpm;
    float applied_pid_target_rpm;
    float measured_rpm;
    float error_rpm;
    float controller_output_raw; /**< PID内部限幅前输出。 */
    float motor_command_applied; /**< 方向保护和硬限幅后的电机命令。 */
    float p_term;
    float i_term;
    float d_term;
    float kp;
    float ki;
    float kd;
} app_pid_tune_status_t;

/**
 * @brief 请求启动一次可重复的单电机PID阶跃测试。
 * @note  请求由500Hz控制任务执行；正常运行和循迹不会绕过平滑限制。
 */
bool app_control_pid_tune_start(uint32_t motor_id, float target_rpm);

/** @brief 请求立即停止PID调参并停车。 */
void app_control_pid_tune_stop(void);

/** @brief 读取PID调参状态和遥测快照。 */
void app_control_pid_tune_get_status(app_pid_tune_status_t *status);

/** @brief 判断调参正在运行或等待控制任务启动。 */
bool app_control_pid_tune_is_active(void);

#ifdef __cplusplus
}
#endif

#endif /* TASK_CONTROL_H */