/**
 * @file    task_imu.h
 * @brief   IMU采集任务接口 (LSM6DSR)
 * @note    10ms周期: 使用LSM6DSR BSP获取姿态/加速度/角速度/温度
 *          写入共享上下文，支持多种滤波器
 */
#ifndef TASK_IMU_H
#define TASK_IMU_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* 前向声明 filter_t (避免头文件循环依赖) */
struct filter;

/**
 * @brief  IMU采集任务函数
 * @note   10ms周期: 
 *         1. 初始化LSM6DSR (首次运行)
 *         2. 调用bsp_lsm6dsr_update_ctx()获取数据
 *         3. 写入共享上下文 (姿态/加速度/角速度/温度)
 * @param  param 任务参数(app_shared_ctx_t指针)
 */
void app_imu_task(void *param);

/**
 * @brief  获取当前活动滤波器实例指针
 * @return filter_t* 滤波器指针, NULL=未初始化
 * @note   用于参数扫描测试 (kftune) 在运行时动态修改滤波器参数
 *         调用方不应释放返回的指针
 */
struct filter* app_imu_get_filter(void);

/**
 * @brief  重置当前滤波器状态
 * @note   清空状态和协方差, 用于参数切换后重新收敛
 *         参数保持不变, 仅重置状态
 */
void app_imu_reset_filter(void);

#ifdef __cplusplus
}
#endif

#endif /* TASK_IMU_H */
