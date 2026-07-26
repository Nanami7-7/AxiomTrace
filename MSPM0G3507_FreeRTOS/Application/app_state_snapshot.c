/**
 * @file    app_state_snapshot.c
 * @brief   应用层只读状态快照实现
 */
#include "app_state_snapshot.h"

#include <string.h>

#include "osal_api.h"

bool app_state_snapshot_read(const app_shared_ctx_t *ctx,
                             app_state_snapshot_t *snapshot)
{
    if (ctx == NULL || snapshot == NULL) {
        return false;
    }

    /* 只保护复制动作；不在临界区内执行格式化、通信或控制计算。 */
    OSAL_CRITICAL_SECTION {
        (void)memcpy(&snapshot->control, &ctx->status,
                     sizeof(snapshot->control));
        (void)memcpy(&snapshot->imu, &ctx->imu,
                     sizeof(snapshot->imu));
        for (uint32_t i = 0U; i < BSP_MOTOR_COUNT; i++) {
            snapshot->motor[i].enabled = ctx->motor_enabled[i];
            snapshot->motor[i].rpm = ctx->status.rpm[i];
            snapshot->motor[i].output = ctx->status.output[i];
            snapshot->motor[i].target = ctx->pid[i].setpoint;
            snapshot->motor[i].kp = ctx->pid[i].kp;
            snapshot->motor[i].ki = ctx->pid[i].ki;
            snapshot->motor[i].kd = ctx->pid[i].kd;
            snapshot->motor[i].ff_enabled = ctx->ff[i].enabled;
            snapshot->motor[i].ff_k = ctx->ff[i].k;
            snapshot->motor[i].ff_b = ctx->ff[i].b;
        }
        snapshot->mode = ctx->posctrl.mode;
    }

    return true;
}
