/**
 * @file    task_imu.c
 * @brief   IMU采集任务实现
 * @note    10ms周期(100Hz):
 *          调用mpu_dmp_get_data获取DMP解算数据(四元数→欧拉角)
 *          优先级: 4(介于control=5和menu=2之间)
 */
#include "task_imu.h"
#include "app_main.h"
#include "osal_api.h"
#include "project_config.h"
#include "inv_mpu.h"
#include <string.h>

void app_imu_task(void *param)
{
    app_shared_ctx_t *ctx = (app_shared_ctx_t *)param;
    float pitch, roll, yaw;

    if (ctx == NULL) {
        osal_task_delete(NULL);
        return;
    }

    for (;;) {
        if (mpu_dmp_get_data(&pitch, &roll, &yaw) == 0) {
            OSAL_CRITICAL_SECTION {
                ctx->imu.roll       = roll;
                ctx->imu.pitch      = pitch;
                ctx->imu.yaw        = yaw;
            }
        }

        osal_task_delay_ms(PRJ_IMU_TASK_PERIOD_MS);
    }
}