/**
 * @file    app_chassis_assist.h
 * @brief   双轮底盘速度整形与IMU直行辅助（纯算法、头文件实现）
 * @note    设计目标是简单、易移植：
 *          1. 编码器仍负责左右轮速度内环；
 *          2. yaw和gyro_z只修正左右轮目标差，不替代编码器测速；
 *          3. 加速度只判断当前姿态角是否可信，不做长期积分测速；
 *          4. 所有参数集中在本文件顶部，实车只需修改宏。
 */
#ifndef APP_CHASSIS_ASSIST_H
#define APP_CHASSIS_ASSIST_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <math.h>

/* ======================== 易调参数 ======================== */

/** 直行IMU辅助总开关。调单轮速度PID时可临时改为0。 */
#ifndef APP_CHASSIS_ASSIST_ENABLE
#define APP_CHASSIS_ASSIST_ENABLE                (1U)
#endif

/** 速度目标最大变化率，防止目标阶跃直接冲击低速速度环。 */
#ifndef APP_CHASSIS_TARGET_SLEW_RPM_PER_S
#define APP_CHASSIS_TARGET_SLEW_RPM_PER_S       (400.0f)
#endif

/** 电机命令最大变化率，单位：命令值/秒；停机不经过此限速。 */
#ifndef APP_CHASSIS_OUTPUT_SLEW_PER_S
#define APP_CHASSIS_OUTPUT_SLEW_PER_S           (1200.0f)
#endif

/** 目标绝对值低于该值时不启用直行航向辅助。 */
#ifndef APP_CHASSIS_ASSIST_MIN_RPM
#define APP_CHASSIS_ASSIST_MIN_RPM              (25.0f)
#endif

/** 左右目标差小于该值才认为用户要求直行。 */
#ifndef APP_CHASSIS_ASSIST_TARGET_MATCH_RPM
#define APP_CHASSIS_ASSIST_TARGET_MATCH_RPM     (8.0f)
#endif

/** IMU数据允许的最大年龄。IMU任务当前为10 ms周期。 */
#ifndef APP_CHASSIS_ASSIST_IMU_MAX_AGE_MS
#define APP_CHASSIS_ASSIST_IMU_MAX_AGE_MS       (60U)
#endif

/** 陀螺仪Z轴符号。若实车修正方向相反，只把+1改成-1。 */
#ifndef APP_CHASSIS_ASSIST_GYRO_SIGN
#define APP_CHASSIS_ASSIST_GYRO_SIGN            (1.0f)
#endif

/** 航向角外环：每1度航向误差转换成多少度/秒的目标角速度。 */
#ifndef APP_CHASSIS_HEADING_KP_DPS_PER_DEG
#define APP_CHASSIS_HEADING_KP_DPS_PER_DEG      (1.2f)
#endif

/** 角速度P环：每1度/秒角速度误差转换成多少RPM左右差修正。 */
#ifndef APP_CHASSIS_YAW_RATE_KP_RPM_PER_DPS
#define APP_CHASSIS_YAW_RATE_KP_RPM_PER_DPS     (0.35f)
#endif

/** 航向角误差限幅，避免yaw异常时突然大转向。 */
#ifndef APP_CHASSIS_HEADING_ERROR_MAX_DEG
#define APP_CHASSIS_HEADING_ERROR_MAX_DEG       (12.0f)
#endif

/** 航向角外环输出限幅。 */
#ifndef APP_CHASSIS_TARGET_YAW_RATE_MAX_DPS
#define APP_CHASSIS_TARGET_YAW_RATE_MAX_DPS     (20.0f)
#endif

/** 左右轮目标RPM修正限幅。 */
#ifndef APP_CHASSIS_CORRECTION_MAX_RPM
#define APP_CHASSIS_CORRECTION_MAX_RPM          (18.0f)
#endif

/** 航向修正变化率限幅，防止陀螺仪噪声直接造成左右轮抖动。 */
#ifndef APP_CHASSIS_CORRECTION_SLEW_RPM_PER_S
#define APP_CHASSIS_CORRECTION_SLEW_RPM_PER_S   (120.0f)
#endif

/** gyro_z一阶低通系数，越小越平滑。 */
#ifndef APP_CHASSIS_GYRO_FILTER_ALPHA
#define APP_CHASSIS_GYRO_FILTER_ALPHA           (0.25f)
#endif

/** 加速度模长处于该范围时才使用yaw角慢外环；超出时仅使用gyro_z角速度环。 */
#ifndef APP_CHASSIS_ACCEL_NORM_MIN_G
#define APP_CHASSIS_ACCEL_NORM_MIN_G            (0.75f)
#endif
#ifndef APP_CHASSIS_ACCEL_NORM_MAX_G
#define APP_CHASSIS_ACCEL_NORM_MAX_G            (1.25f)
#endif

/* ======================== 状态与接口 ======================== */

typedef struct {
    bool active;                 /**< 是否已经锁定直行参考航向。 */
    float heading_ref_deg;       /**< 本次直行启动时的参考航向。 */
    float gyro_z_filtered_dps;   /**< 低通后的Z轴角速度。 */
    float correction_rpm;        /**< 当前左右轮差速修正。 */
} app_chassis_assist_t;

/** 通用斜坡函数：每次最多向目标移动max_step。 */
static inline float app_chassis_slew(float current,
                                     float target,
                                     float max_step)
{
    if (max_step <= 0.0f) {
        return target;
    }
    if (target > current + max_step) {
        return current + max_step;
    }
    if (target < current - max_step) {
        return current - max_step;
    }
    return target;
}

/** 将角度差归一化到[-180, 180]。 */
static inline float app_chassis_wrap_angle_deg(float angle_deg)
{
    while (angle_deg > 180.0f) {
        angle_deg -= 360.0f;
    }
    while (angle_deg < -180.0f) {
        angle_deg += 360.0f;
    }
    return angle_deg;
}

static inline float app_chassis_clamp(float value,
                                      float min_value,
                                      float max_value)
{
    if (value < min_value) {
        return min_value;
    }
    if (value > max_value) {
        return max_value;
    }
    return value;
}

/** 停止、转弯、IMU失效或切换模式时调用。 */
static inline void app_chassis_assist_reset(app_chassis_assist_t *assist)
{
    if (assist == NULL) {
        return;
    }
    assist->active = false;
    assist->heading_ref_deg = 0.0f;
    assist->gyro_z_filtered_dps = 0.0f;
    assist->correction_rpm = 0.0f;
}

/**
 * @brief 计算直行时的左右轮目标RPM修正量。
 * @return correction_rpm；调用方执行：left -= correction，right += correction。
 * @note  仅在左右目标近似相同、IMU新鲜且速度足够时生效。
 *        加速度只用于动态可信度判断，严禁在此长期积分成车速。
 */
static inline float app_chassis_assist_update(
    app_chassis_assist_t *assist,
    float left_target_rpm,
    float right_target_rpm,
    float yaw_deg,
    float gyro_z_dps,
    float accel_x_g,
    float accel_y_g,
    float accel_z_g,
    uint32_t imu_age_ms,
    float dt_s)
{
    const float average_target = 0.5f * (left_target_rpm + right_target_rpm);
    const bool same_direction = (left_target_rpm * right_target_rpm) > 0.0f;
    const bool straight_request =
        (APP_CHASSIS_ASSIST_ENABLE != 0U) &&
        same_direction &&
        (fabsf(average_target) >= APP_CHASSIS_ASSIST_MIN_RPM) &&
        (fabsf(left_target_rpm - right_target_rpm) <=
            APP_CHASSIS_ASSIST_TARGET_MATCH_RPM);
    const bool imu_valid =
        (imu_age_ms <= APP_CHASSIS_ASSIST_IMU_MAX_AGE_MS) &&
        isfinite(yaw_deg) && isfinite(gyro_z_dps) &&
        isfinite(accel_x_g) && isfinite(accel_y_g) && isfinite(accel_z_g) &&
        (dt_s > 0.0f) && (dt_s <= 0.1f);

    if ((assist == NULL) || !straight_request || !imu_valid) {
        app_chassis_assist_reset(assist);
        return 0.0f;
    }

    const float gyro_signed = APP_CHASSIS_ASSIST_GYRO_SIGN * gyro_z_dps;
    if (!assist->active) {
        assist->active = true;
        assist->heading_ref_deg = yaw_deg;
        assist->gyro_z_filtered_dps = gyro_signed;
        assist->correction_rpm = 0.0f;
    } else {
        assist->gyro_z_filtered_dps += APP_CHASSIS_GYRO_FILTER_ALPHA *
            (gyro_signed - assist->gyro_z_filtered_dps);
    }

    /* 强振动或大加速度时，暂时不用yaw角外环，但保留更直接的角速度抑制。 */
    const float accel_norm_g = sqrtf(accel_x_g * accel_x_g +
                                     accel_y_g * accel_y_g +
                                     accel_z_g * accel_z_g);
    float target_yaw_rate_dps = 0.0f;
    if ((accel_norm_g >= APP_CHASSIS_ACCEL_NORM_MIN_G) &&
        (accel_norm_g <= APP_CHASSIS_ACCEL_NORM_MAX_G)) {
        float heading_error = app_chassis_wrap_angle_deg(
            assist->heading_ref_deg - yaw_deg);
        heading_error = app_chassis_clamp(
            heading_error,
            -APP_CHASSIS_HEADING_ERROR_MAX_DEG,
             APP_CHASSIS_HEADING_ERROR_MAX_DEG);
        target_yaw_rate_dps = APP_CHASSIS_HEADING_KP_DPS_PER_DEG *
            heading_error;
        target_yaw_rate_dps = app_chassis_clamp(
            target_yaw_rate_dps,
            -APP_CHASSIS_TARGET_YAW_RATE_MAX_DPS,
             APP_CHASSIS_TARGET_YAW_RATE_MAX_DPS);
    }

    const float yaw_rate_error = target_yaw_rate_dps -
                                 assist->gyro_z_filtered_dps;
    float correction_target = APP_CHASSIS_YAW_RATE_KP_RPM_PER_DPS *
                              yaw_rate_error;
    correction_target = app_chassis_clamp(
        correction_target,
        -APP_CHASSIS_CORRECTION_MAX_RPM,
         APP_CHASSIS_CORRECTION_MAX_RPM);

    assist->correction_rpm = app_chassis_slew(
        assist->correction_rpm,
        correction_target,
        APP_CHASSIS_CORRECTION_SLEW_RPM_PER_S * dt_s);
    return assist->correction_rpm;
}

#endif /* APP_CHASSIS_ASSIST_H */
