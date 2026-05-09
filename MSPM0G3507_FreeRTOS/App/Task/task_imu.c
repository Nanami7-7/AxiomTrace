/**
 * @file    task_imu.c
 * @brief   IMU采集任务实现
 * @note    10ms周期(100Hz):
 *          1. 检查DMP FIFO就绪
 *          2. 读取DMP解算数据(四元数+欧拉角+传感器数据)
 *          3. 临界区保护写入共享上下文
 *          优先级: 4(介于control=5和menu=2之间)
 */
#include "task_imu.h"
#include "app_main.h"
#include "bsp_mpu6050_dmp.h"
#include "osal_api.h"
#include "project_config.h"
#include <string.h>

/* ======================== 任务函数实现 ======================== */

void app_imu_task(void *param)
{
    app_shared_ctx_t *ctx = (app_shared_ctx_t *)param;
    bsp_mpu6050_dmp_data_t dmp_data;
    bsp_status_t ret;

    /* 参数校验 */
    if (ctx == NULL) {
        osal_task_delete(NULL);
        return;
    }

    for (;;) {
        /* 检查DMP是否有新数据 */
        if (bsp_mpu6050_dmp_data_ready()) {
            ret = bsp_mpu6050_dmp_read(&dmp_data);

            if (ret == BSP_OK) {
                /* 临界区保护: 更新共享上下文中的IMU数据 */
                OSAL_CRITICAL_SECTION {
                    ctx->imu.roll       = dmp_data.roll;
                    ctx->imu.pitch      = dmp_data.pitch;
                    ctx->imu.yaw        = dmp_data.yaw;
                    ctx->imu.accel_x_g  = dmp_data.accel_x_g;
                    ctx->imu.gyro_z_dps = dmp_data.gyro_z_dps;
                    ctx->imu.timestamp_ms = dmp_data.timestamp_ms;
                }
            }
            /* BSP_ERR_BUF_EMPTY时静默跳过, 其他错误可在此添加错误处理 */
        }

        /* 等待下一个周期(10ms = 100Hz) */
        osal_task_delay_ms(PRJ_IMU_TASK_PERIOD_MS);
    }
}