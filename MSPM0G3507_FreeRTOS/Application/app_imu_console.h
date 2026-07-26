/**
 * @file    app_imu_console.h
 * @brief   IMU 串口状态查询与受控遥测输出接口
 * @details 本模块集中管理 IMU 控制台命令、输出格式、发送周期和 UART0
 *          遥测状态。菜单任务只负责转交命令，IMU 任务只负责提交最新样本，
 *          从而避免任务层直接拼接协议数据。
 */
#ifndef APP_IMU_CONSOLE_H
#define APP_IMU_CONSOLE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

/** IMU 串口遥测格式。 */
typedef enum {
    APP_IMU_CONSOLE_FORMAT_COMPACT = 0, /**< roll,pitch,yaw 三通道。 */
    APP_IMU_CONSOLE_FORMAT_FULL         /**< 17 通道完整诊断数据。 */
} app_imu_console_format_t;

/** 菜单命令处理结果。 */
typedef enum {
    APP_IMU_CONSOLE_CMD_NOT_HANDLED = 0, /**< 不是 IMU 命令。 */
    APP_IMU_CONSOLE_CMD_HANDLED,         /**< 已处理普通 IMU 命令。 */
    APP_IMU_CONSOLE_CMD_STREAM_STARTED,  /**< 已开启 IMU 连续输出。 */
    APP_IMU_CONSOLE_CMD_STREAM_STOPPED   /**< 已关闭 IMU 连续输出。 */
} app_imu_console_cmd_result_t;

/**
 * @brief IMU 控制台使用的完整只读样本。
 * @note  加速度单位为 g，角速度单位为 dps，姿态角单位为度。
 */
typedef struct {
    float accel_x_g;
    float accel_y_g;
    float accel_z_g;
    float gyro_x_dps;
    float gyro_y_dps;
    float gyro_z_dps;
    float pitch;
    float roll;
    float yaw;
    float kf_p00_x;
    float kf_p00_y;
    float kf_p00_z;
    float gyro_mag_dps;
    float acc_norm_err;
    float kf_p11_x;
    float temperature;
    float kf_bias_z;
    uint32_t timestamp_ms;
} app_imu_console_sample_t;

/** 初始化控制台状态；默认关闭连续输出。 */
void app_imu_console_init(void);

/**
 * @brief 提交最新 IMU 样本，并在达到发送周期时尝试输出一帧。
 * @param sample 最新有效样本，函数返回前完成复制，不保存调用者指针。
 * @note  应由 IMU 采集任务调用；UART0 忙时丢弃本次发送，不阻塞采样。
 */
void app_imu_console_update(const app_imu_console_sample_t *sample);

/**
 * @brief 解析并执行一条 IMU 菜单命令。
 * @param line 已去除 CR/LF 的命令行。
 * @return 命令处理结果；非 IMU 命令返回 APP_IMU_CONSOLE_CMD_NOT_HANDLED。
 */
app_imu_console_cmd_result_t app_imu_console_handle_command(const char *line);

/** 查询连续 IMU 输出是否已开启。 */
bool app_imu_console_is_streaming(void);

/**
 * @brief 强制关闭连续 IMU 输出。
 * @return true 表示调用前处于开启状态，false 表示原本已关闭。
 */
bool app_imu_console_stop(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_IMU_CONSOLE_H */
