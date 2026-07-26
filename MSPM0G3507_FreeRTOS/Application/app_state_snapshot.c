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
        (void)memcpy(snapshot->motor_enabled, ctx->motor_enabled,
                     sizeof(snapshot->motor_enabled));
    }

    return true;
}
