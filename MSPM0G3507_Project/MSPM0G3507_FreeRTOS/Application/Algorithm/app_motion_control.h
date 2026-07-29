/**
 * @file    app_motion_control.h
 * @brief   速度环、位置环和角度环的统一应用层调用接口。
 * @details 本模块是运动控制门面：上层只设置目标，不直接操作 PWM、PID 或 GPIO。
 *          真正的闭环计算仍由现有控制任务、编码器和电机 BSP 完成。
 *
 * 速度模式：维持一个或多个电机的目标 RPM；
 * 位置模式：按轮径和编码器参数移动指定距离；
 * 角度模式：按当前 IMU yaw 执行相对/绝对角度控制。
 */
#ifndef APP_MOTION_CONTROL_H
#define APP_MOTION_CONTROL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

struct app_shared_ctx_s;

/** 当前工程的电机数量，顺序为 A/M1、B/M2、C/M3、D/M4。 */
#define APP_MOTION_MOTOR_COUNT (4U)

/** 电机编号，用于指定单路速度目标。 */
typedef enum {
    APP_MOTION_MOTOR_A = 0, /**< A/M1 电机。 */
    APP_MOTION_MOTOR_B,     /**< B/M2 电机。 */
    APP_MOTION_MOTOR_C,     /**< C/M3 电机。 */
    APP_MOTION_MOTOR_D      /**< D/M4 电机。 */
} app_motion_motor_id_t;

/** 运动控制门面的当前状态。 */
typedef enum {
    APP_MOTION_STATE_IDLE = 0, /**< 空闲，没有运动目标。 */
    APP_MOTION_STATE_SPEED,    /**< 速度环正在维持 RPM。 */
    APP_MOTION_STATE_POSITION, /**< 位置环正在移动指定距离。 */
    APP_MOTION_STATE_ANGLE,    /**< 角度环正在执行转向。 */
    APP_MOTION_STATE_FAULT     /**< 参数、电源或传感器异常。 */
} app_motion_state_t;

/**
 * @brief 初始化运动控制门面。
 * @param ctx 应用共享上下文，生命周期必须覆盖控制任务。
 * @return true 初始化成功；false 表示 ctx 为空。
 * @note 应在电机、编码器、PID 和 IMU 上下文准备好后调用一次。
 */
bool app_motion_control_init(struct app_shared_ctx_s *ctx);

/**
 * @brief 设置单个电机的目标转速。
 * @param motor A～D 电机编号。
 * @param rpm 目标转速，单位 RPM；正负号沿用当前安装方向约定。
 * @return true 已接受；false 表示参数、电源或初始化失败。
 * @note 未指定的其他电机目标会被置零，适合单电机测试。
 */
bool app_motion_speed_motor(app_motion_motor_id_t motor, float rpm);

/**
 * @brief 同时设置四个电机的持续目标速度。
 * @param rpm 长度为 4 的数组，顺序为 A、B、C、D。
 * @return true 已接受；false 表示数组为空或 RPM 超限。
 */
bool app_motion_speed_all(const float rpm[APP_MOTION_MOTOR_COUNT]);

/**
 * @brief 设置四个电机速度，并在超时后自动停车。
 * @param rpm 长度为 4 的目标数组，顺序为 A、B、C、D。
 * @param timeout_ms 超时时间，单位毫秒；0 表示不设置超时。
 * @return true 已接受；false 表示参数或电源失败。
 */
bool app_motion_speed_all_timed(const float rpm[APP_MOTION_MOTOR_COUNT],
                                uint32_t timeout_ms);

/**
 * @brief 启动车体位置控制。
 * @param distance_m 移动距离，单位米；正负表示前进/后退。
 * @param cruise_rpm 巡航转速，单位 RPM，必须为正值。
 * @param timeout_ms 最大执行时间，单位毫秒；0 表示不设置超时。
 * @return true 已启动；false 表示参数、编码器或电源异常。
 * @note 距离会按 project_config.h 中的轮径和编码器参数换算成脉冲数。
 */
bool app_motion_position_start(float distance_m,
                               float cruise_rpm,
                               uint32_t timeout_ms);

/**
 * @brief 按当前 yaw 启动相对角度控制。
 * @param delta_angle_deg 相对角度，单位度；正值沿当前工程左转约定。
 * @param cruise_rpm 转向巡航转速，单位 RPM，必须为正值。
 * @param timeout_ms 最大执行时间，单位毫秒；0 表示不设置超时。
 * @return true 已启动；false 表示 yaw、参数或电源异常。
 */
bool app_motion_angle_start_relative(float delta_angle_deg,
                                     float cruise_rpm,
                                     uint32_t timeout_ms);

/**
 * @brief 启动绝对 yaw 角度控制。
 * @param target_angle_deg 目标绝对角度，单位度。
 * @param cruise_rpm 转向巡航转速，单位 RPM，必须为正值。
 * @param timeout_ms 最大执行时间，单位毫秒；0 表示不设置超时。
 * @return true 已启动；false 表示 yaw 或参数异常。
 * @note 内部会转换成相对于当前 yaw 的角度差。
 */
bool app_motion_angle_start_absolute(float target_angle_deg,
                                     float cruise_rpm,
                                     uint32_t timeout_ms);

/** 立即停止所有电机，并取消当前速度/位置/角度目标。 */
void app_motion_stop(void);

/**
 * @brief 检查动作超时。
 * @param now_ms 控制任务提供的单调递增毫秒时间戳。
 * @note 由现有控制任务每周期调用，不创建第二套控制循环。
 */
void app_motion_control_process(uint32_t now_ms);

/** @return 当前运动状态，见 app_motion_state_t。 */
app_motion_state_t app_motion_get_state(void);

/** @return true 表示有运动动作正在执行。 */
bool app_motion_is_busy(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_MOTION_CONTROL_H */
