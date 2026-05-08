/**
 * @file    task_imu.h
 * @brief   IMU采集任务接口
 * @note    10ms周期: 读取DMP四元数/姿态→写入共享上下文
 */
#ifndef TASK_IMU_H
#define TASK_IMU_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/**
 * @brief  IMU采集任务函数
 * @note   10ms周期: 检查DMP就绪→读取姿态数据→写入共享上下文
 * @param  param 任务参数(app_shared_ctx_t指针)
 */
void app_imu_task(void *param);

#ifdef __cplusplus
}
#endif

#endif /* TASK_IMU_H */