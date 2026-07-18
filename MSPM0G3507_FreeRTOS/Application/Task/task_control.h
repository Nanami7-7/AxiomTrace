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

/**
 * @brief  控制任务函数
 * @note   5ms周期: 读取编码器→M法测速RPM→PID计算→设置电机duty
 * @param  param 任务参数(app_shared_ctx_t指针)
 */
void app_control_task(void *param);

#ifdef __cplusplus
}
#endif

#endif /* TASK_CONTROL_H */