/**
 * @file    app_state_snapshot.h
 * @brief   应用层只读状态快照契约
 *
 * 本模块只定义跨任务读取应用状态的边界，不改变现有状态写入者，
 * 也不切换控制任务、菜单任务或 IMU 任务的现有调用路径。
 */
#ifndef APP_STATE_SNAPSHOT_H
#define APP_STATE_SNAPSHOT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

#include "app_main.h"

/**
 * @brief 单个电机的只读状态快照
 */
typedef struct {
    /** 电机使能标志。 */
    bool enabled;
    /** 控制任务发布的实时转速。 */
    int32_t rpm;
    /** 控制任务发布的输出值。 */
    int32_t output;
    /** 当前目标转速。 */
    float target;
    /** PID 参数快照。 */
    float kp;
    float ki;
    float kd;
    /** 前馈使能标志。 */
    bool ff_enabled;
    /** 前馈参数快照。 */
    float ff_k;
    float ff_b;
} app_motor_state_snapshot_t;

/**
 * @brief 应用层对外只读状态快照
 *
 * 快照只包含诊断和观测所需的值，不暴露 PID、前馈和位置控制器的
 * 可写内部对象。调用者可以在临界区外读取该副本，避免长时间持有
 * 共享上下文或直接访问控制对象。
 */
typedef struct {
    /** 控制任务发布的电机转速和输出状态。 */
    app_control_status_t control;
    /** IMU 任务发布的姿态、惯导数据和时间戳。 */
    app_imu_data_t imu;
    /** 各电机的参数、使能和实时状态。 */
    app_motor_state_snapshot_t motor[BSP_MOTOR_COUNT];
    /** 当前位置控制模式。 */
    app_ctrl_mode_t mode;
} app_state_snapshot_t;

/**
 * @brief 从应用共享上下文复制一致的只读快照
 * @param[in]  ctx      应用共享上下文；不能为 NULL
 * @param[out] snapshot 快照输出地址；不能为 NULL
 * @retval true 复制成功
 * @retval false 参数为空，未执行复制
 *
 * 函数只在复制期间进入临界区，返回后调用者不得通过快照
 * 修改应用运行状态。该适配层暂不替换现有消费者，便于后续逐个迁移和回退。
 */
bool app_state_snapshot_read(const app_shared_ctx_t *ctx,
                             app_state_snapshot_t *snapshot);

#ifdef __cplusplus
}
#endif

#endif /* APP_STATE_SNAPSHOT_H */
