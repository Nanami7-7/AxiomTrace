/**
 * @file    app_line_track.c
 * @brief   四/五路红外循迹控制实现。
 *
 * 通道数量由 BSP_IR_CHANNEL_COUNT 切换。四路逻辑保持不变；五路板采用
 * 按传感器实际安装尺寸计算加权偏差，并以“中间三路全黑”作为启停横线判据（见
 * LINE_TRACK_CROSS_BLACK_MASK）。设计原则：
 * 1. app_line_track_update() 只负责读取传感器和计算左右轮目标RPM；
 * 2. app_line_track_step() 不直接操作电机，目标由控制任务交给现有速度PI；
 * 3. 不在本模块中增加看门狗、任务或阻塞等待；
 * 4. 上电默认停止，必须显式调用 app_line_track_start() 才会生成目标。
 */
#include "app_line_track.h"
#include "bsp_motor.h"
#include "project_config.h"
#if (LINE_TRACK_MODEL_CONTROL_ENABLE != 0U)
#include "app_speed_planner.h"
#endif
#include <math.h>
#include <stddef.h>
#include <string.h>

/* 工厂测试固件不启用循迹算法，避免引入额外浮点库代码。 */
#if defined(PRJ_DRV8870_FACTORY_TEST_ENABLE) && \
    (PRJ_DRV8870_FACTORY_TEST_ENABLE != 0)
#define APP_LINE_TRACK_BUILD_ENABLE 0
#else
#define APP_LINE_TRACK_BUILD_ENABLE 1
#endif

/* 如果工程没有定义电机通道掩码，默认允许四路输出。 */
#ifndef PRJ_MOTION_MOTOR_ACTIVE_MASK
#define PRJ_MOTION_MOTOR_ACTIVE_MASK   (0x0FU)
#endif


static volatile bool s_running;
static line_track_output_t s_last_output;

#if APP_LINE_TRACK_BUILD_ENABLE

static int s_turn_cnt;
static int s_saved_state;
static int s_last_valid_state;
static float s_last_line_error;
static float s_prev_control_error;
static float s_turn_output;
static uint32_t s_lost_cycles;
static int32_t s_left_rpm_feedback;
static int32_t s_right_rpm_feedback;
static uint32_t s_run_elapsed_ms;
static float s_imu_yaw_deg;
static float s_imu_gyro_z_dps;
static float s_imu_accel_x_g;
static float s_imu_accel_y_g;
static float s_imu_accel_z_g;
static uint32_t s_imu_timestamp_ms;
static uint32_t s_imu_age_ms;
static uint32_t s_imu_processed_timestamp_ms;
static uint16_t s_imu_straight_cycles;
static bool s_imu_heading_locked;
static float s_imu_heading_ref_deg;
static float s_imu_gyro_filtered_dps;
static float s_imu_correction_f;
static uint8_t s_filter_history[3];
static uint8_t s_filter_index;
static bool s_filter_initialized;
static const uint8_t s_ch_map[BSP_IR_CHANNEL_COUNT] = LINE_TRACK_CH_MAP;
static line_track_params_t s_params;

#if (LINE_TRACK_MODEL_CONTROL_ENABLE != 0U)
/* 模型只规划中心速度；左右差速保持快速通道，避免弯道响应被四轮规划器拖慢。 */
static app_speed_planner_t s_model_speed_planner;
static float s_model_turn_output_rpm;
static float s_model_gyro_filtered_rad_s;
static uint32_t s_model_gyro_timestamp_ms;
#endif

/** 将BSP读数转换成“1=黑线”的掩码，最高bit对应最左侧传感器。 */
static uint8_t line_track_make_black_mask(const uint8_t ir[BSP_IR_CHANNEL_COUNT])
{
    uint8_t mask = 0U;

    for (uint8_t i = 0U; i < BSP_IR_CHANNEL_COUNT; i++) {
        if (ir[s_ch_map[i]] != 0U) {
            mask |= (uint8_t)(1U << ((BSP_IR_CHANNEL_COUNT - 1U) - i));
        }
    }
    return mask;
}

/** 三次采样逐位多数表决；首次调用用当前值填满历史，避免启动瞬间误判。 */
static uint8_t line_track_filter_black_mask(uint8_t raw_mask)
{
    if (LINE_TRACK_FILTER_ENABLE == 0U) {
        return raw_mask;
    }

    if (!s_filter_initialized) {
        s_filter_history[0] = raw_mask;
        s_filter_history[1] = raw_mask;
        s_filter_history[2] = raw_mask;
        s_filter_index = 0U;
        s_filter_initialized = true;
    } else {
        s_filter_history[s_filter_index] = raw_mask;
        s_filter_index++;
        if (s_filter_index >= 3U) {
            s_filter_index = 0U;
        }
    }

    return (uint8_t)((s_filter_history[0] & s_filter_history[1]) |
                     (s_filter_history[0] & s_filter_history[2]) |
                     (s_filter_history[1] & s_filter_history[2]));
}

/**
 * 计算线相对车体中心的加权偏差。
 * 四路保持 +3、+1、-1、-3；五路使用按 48.9mm/96.5mm 换算后的
 * 实际位置权重。正值表示黑线在车体左侧，负值表示黑线在车体右侧。
 * 多路同时见线时取平均值，形成连续过渡。
 */
static float line_track_error_from_mask(uint8_t black_mask)
{
#if (BSP_IR_CHANNEL_COUNT == 4U)
    static const float weights[BSP_IR_CHANNEL_COUNT] = { 3.0f, 1.0f, -1.0f, -3.0f };
#else
    static const float weights[BSP_IR_CHANNEL_COUNT] = LINE_TRACK_WEIGHTS_5CH;
#endif
    float sum = 0.0f;
    uint32_t count = 0U;

    for (uint8_t i = 0U; i < BSP_IR_CHANNEL_COUNT; i++) {
        const uint8_t bit = (uint8_t)(1U << ((BSP_IR_CHANNEL_COUNT - 1U) - i));
        if ((black_mask & bit) != 0U) {
            /* 采用浮点物理位置，避免把96.5mm总宽误当成48.9mm等间距。 */
            sum += weights[i];
            count++;
        }
    }

    return (count == 0U) ? 0.0f : ((float)sum / (float)count);
}

/** 将归一化偏差（四路约-3~+3，五路约-2~+2）映射为连续转向量。 */
static float line_track_turn_from_error(float error)
{
    const float abs_error = fabsf(error);
    float angle;

    if (abs_error <= 1.0f) {
        angle = s_params.turn_min_angle * abs_error;
    } else if (abs_error <= 2.0f) {
        angle = s_params.turn_min_angle +
                (s_params.turn_mid_angle - s_params.turn_min_angle) *
                (abs_error - 1.0f);
    } else {
        float high_part = abs_error - 2.0f;
        if (high_part > 1.0f) {
            high_part = 1.0f;
        }
        angle = s_params.turn_mid_angle +
                (s_params.turn_max_angle - s_params.turn_mid_angle) *
                high_part;
    }

    return (error < 0.0f) ? -angle : angle;
}

static float line_track_slew_step(float current, float target, float max_step);

/** 限制旧算法相邻控制周期的转向突变量；0表示不限制。 */
static float line_track_slew(float current, float target)
{
    return line_track_slew_step(current, target, LINE_TRACK_TURN_SLEW_STEP);
}


static int32_t line_track_abs_i32(int32_t value)
{
    return (value < 0) ? -value : value;
}

/**
 * 只在红外居中直行时做左右轮同步P校正。
 * 参考工程采用左右轮独立增量PI；本工程低速速度环仍有抖动，因此这里只补偿
 * 两轮相对速度差，不追求绝对RPM，也不引入积分累积。
 */
static bool line_track_is_startup_window(void)
{
    return (LINE_TRACK_STARTUP_ASSIST_ENABLE != 0U) &&
           (s_run_elapsed_ms <= LINE_TRACK_STARTUP_SYNC_WINDOW_MS);
}

/** 起步阶段将基础速度从较低比例平滑提升，给编码器和IMU留下纠偏时间。 */
static float line_track_apply_startup_ramp(float base_rpm)
{
    float begin_ratio = LINE_TRACK_STARTUP_BEGIN_RATIO;

    if ((LINE_TRACK_STARTUP_ASSIST_ENABLE == 0U) ||
        (LINE_TRACK_STARTUP_RAMP_MS == 0U) ||
        (s_run_elapsed_ms >= LINE_TRACK_STARTUP_RAMP_MS)) {
        return base_rpm;
    }
    if (begin_ratio < 0.0f) {
        begin_ratio = 0.0f;
    } else if (begin_ratio > 1.0f) {
        begin_ratio = 1.0f;
    }

    const float progress = (float)s_run_elapsed_ms /
                           (float)LINE_TRACK_STARTUP_RAMP_MS;
    return base_rpm * (begin_ratio + (1.0f - begin_ratio) * progress);
}

/** 直线判断：起步横线也允许修正，离开起步阶段后横线不参与航向锁定。 */
static bool line_track_is_straight_like(const line_track_output_t *out,
                                        bool is_lost)
{
    const bool on_cross =
        ((out->black_mask & LINE_TRACK_CROSS_BLACK_MASK) ==
         LINE_TRACK_CROSS_BLACK_MASK);

    return !is_lost &&
           (fabsf(out->line_error) <= LINE_TRACK_STRAIGHT_SYNC_LINE_ERROR_MAX) &&
           (!on_cross || line_track_is_startup_window());
}

static float line_track_clamp_float(float value, float min_value, float max_value)
{
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

static float line_track_slew_step(float current, float target, float max_step)
{
    if (!isfinite(current) || !isfinite(target) || !isfinite(max_step) ||
        (max_step <= 0.0f)) {
        return target;
    }
    if (target > (current + max_step)) {
        return current + max_step;
    }
    if (target < (current - max_step)) {
        return current - max_step;
    }
    return target;
}

/** 清空本周期模型诊断量；旧算法路径仍可输出统一格式的零值字段。 */
static void line_track_clear_model_output(line_track_output_t *out)
{
    out->line_error_m = 0.0f;
    out->curvature_raw_m_inv = 0.0f;
    out->curvature_m_inv = 0.0f;
    out->base_request_rpm = 0.0f;
    out->base_planned_rpm = 0.0f;
    out->base_accel_rpm_s = 0.0f;
    out->yaw_rate_ref_dps = 0.0f;
    out->yaw_rate_measured_dps = 0.0f;
    out->yaw_rate_error_dps = 0.0f;
    out->turn_feedforward_rpm = 0.0f;
    out->turn_feedback_rpm = 0.0f;
    out->model_enabled = (LINE_TRACK_MODEL_CONTROL_ENABLE != 0U);
    out->model_valid = false;
    out->model_imu_rate_valid = false;
}

/**
 * 曲率模型：Pure Pursuit曲率 → 中心速度规划 → 运动学前馈 + gyro角速度P反馈。
 * 返回false时调用者继续使用原非线性P/PD算法，保证参数错误和IMU失效都可安全回退。
 */
static bool line_track_try_model(line_track_output_t *out,
                                 float line_error,
                                 float legacy_base_rpm,
                                 float legacy_turn_rpm,
                                 bool is_lost,
                                 bool force_right_angle)
{
    line_track_clear_model_output(out);

#if (LINE_TRACK_MODEL_CONTROL_ENABLE != 0U)
    const float pi_f = 3.14159265358979323846f;
    const float rad_to_deg = 57.29577951308232f;
    const float deg_to_rad = 0.017453292519943295f;
    const float dt_s = (float)PRJ_CONTROL_PERIOD_MS * 0.001f;
    const float sensor_forward_m = LINE_TRACK_MODEL_SENSOR_FORWARD_M;
    const float wheel_diameter_m = LINE_TRACK_MODEL_WHEEL_DIAMETER_M;
    const float wheel_base_m = LINE_TRACK_MODEL_EFFECTIVE_WHEEL_BASE_M;
    const float max_curvature = LINE_TRACK_MODEL_MAX_CURVATURE_M_INV;
    const float max_lateral_accel = LINE_TRACK_MODEL_MAX_LATERAL_ACCEL_M_S2;
    const float wheel_circumference_m = pi_f * wheel_diameter_m;
    float error_m;
    float curvature_raw;
    float curvature;
    float base_request_rpm;
    float base_planned_rpm;
    float base_accel_rpm_s = 0.0f;
    float linear_speed_m_s;
    float yaw_rate_ref_rad_s;
    float yaw_rate_measured_rad_s = 0.0f;
    float yaw_rate_error_rad_s;
    float turn_feedforward_rpm;
    float turn_feedback_rpm = 0.0f;
    float turn_target_rpm;
    float min_speed_ratio;
    float max_turn_rpm;
    bool imu_rate_valid = false;

    /* 丢线和直角弯继续使用原有快速恢复逻辑，不让模型覆盖安全行为。 */
    if (is_lost || force_right_angle) {
        if (app_speed_planner_is_valid(&s_model_speed_planner)) {
            app_speed_planner_reset(&s_model_speed_planner,
                line_track_clamp_float(legacy_base_rpm, 0.0f, PRJ_PLANNER_MAX_RPM));
        }
        s_model_turn_output_rpm = legacy_turn_rpm;
        return false;
    }

    if (!isfinite(line_error) || !isfinite(s_params.base_speed) ||
        !isfinite(sensor_forward_m) || !isfinite(wheel_diameter_m) ||
        !isfinite(wheel_base_m) || !isfinite(max_curvature) ||
        !isfinite(max_lateral_accel) || !isfinite(dt_s) ||
        (sensor_forward_m <= 0.001f) || (wheel_diameter_m <= 0.001f) ||
        (wheel_base_m <= 0.001f) || (max_curvature <= 0.0f) ||
        (max_lateral_accel <= 0.0f) || (dt_s <= 0.0f)) {
        return false;
    }

    error_m = LINE_TRACK_MODEL_ERROR_SIGN *
              LINE_TRACK_MODEL_ERROR_UNIT_M * line_error;
    curvature_raw = (2.0f * error_m) /
        (sensor_forward_m * sensor_forward_m + error_m * error_m);
    curvature = line_track_clamp_float(curvature_raw,
                                      -max_curvature,
                                       max_curvature);

    base_request_rpm = line_track_clamp_float(
        s_params.base_speed, 0.0f, PRJ_PLANNER_MAX_RPM);
    min_speed_ratio = line_track_clamp_float(
        LINE_TRACK_MODEL_MIN_SPEED_RATIO, 0.0f, 1.0f);

    /* 由横向加速度 a=v²|kappa| 计算弯道中心速度上限。 */
    if (fabsf(curvature) > 0.0001f) {
        const float curve_speed_m_s = sqrtf(max_lateral_accel /
                                             fabsf(curvature));
        const float curve_speed_rpm = 60.0f * curve_speed_m_s /
                                      wheel_circumference_m;
        const float min_curve_rpm = base_request_rpm * min_speed_ratio;
        base_request_rpm = line_track_clamp_float(
            curve_speed_rpm, min_curve_rpm, base_request_rpm);
    }

#if (LINE_TRACK_MODEL_SPEED_SCURVE_ENABLE != 0U)
    if (!app_speed_planner_is_valid(&s_model_speed_planner)) {
        return false;
    }
    base_planned_rpm = app_speed_planner_update(
        &s_model_speed_planner, base_request_rpm, dt_s);
    if (!app_speed_planner_is_valid(&s_model_speed_planner) ||
        !isfinite(base_planned_rpm)) {
        return false;
    }

    /* 弯道限速是硬上限：加速走S型，突然进弯时允许立即下压，防止规划惯性导致丢线。 */
    if (base_planned_rpm > base_request_rpm) {
        base_planned_rpm = base_request_rpm;
        app_speed_planner_reset(&s_model_speed_planner, base_planned_rpm);
    }
    base_accel_rpm_s = app_speed_planner_get_accel(&s_model_speed_planner);
#else
    base_planned_rpm = base_request_rpm;
#endif

    linear_speed_m_s = wheel_circumference_m * base_planned_rpm / 60.0f;
    yaw_rate_ref_rad_s = linear_speed_m_s * curvature;

#if (LINE_TRACK_MODEL_GYRO_RATE_ENABLE != 0U)
    imu_rate_valid = (s_imu_timestamp_ms != 0U) &&
                     (s_imu_age_ms <= LINE_TRACK_IMU_MAX_AGE_MS) &&
                     isfinite(s_imu_gyro_z_dps);
    if (imu_rate_valid) {
        const float gyro_rad_s = LINE_TRACK_MODEL_GYRO_SIGN *
                                 s_imu_gyro_z_dps * deg_to_rad;
        if (s_model_gyro_timestamp_ms != s_imu_timestamp_ms) {
            const float alpha = line_track_clamp_float(
                LINE_TRACK_MODEL_GYRO_FILTER_ALPHA, 0.0f, 1.0f);
            if (s_model_gyro_timestamp_ms == 0U) {
                s_model_gyro_filtered_rad_s = gyro_rad_s;
            } else {
                s_model_gyro_filtered_rad_s += alpha *
                    (gyro_rad_s - s_model_gyro_filtered_rad_s);
            }
            s_model_gyro_timestamp_ms = s_imu_timestamp_ms;
        }
        yaw_rate_measured_rad_s = s_model_gyro_filtered_rad_s;
        if (base_planned_rpm > 1.0f) {
            turn_feedback_rpm = LINE_TRACK_MODEL_YAW_RATE_KP_RPM_PER_RAD_S *
                (yaw_rate_ref_rad_s - yaw_rate_measured_rad_s);
            turn_feedback_rpm = line_track_clamp_float(
                turn_feedback_rpm,
                -LINE_TRACK_MODEL_YAW_RATE_FB_MAX_RPM,
                 LINE_TRACK_MODEL_YAW_RATE_FB_MAX_RPM);
        }
    } else {
        s_model_gyro_timestamp_ms = 0U;
        s_model_gyro_filtered_rad_s = 0.0f;
    }
#endif

    /* 差速底盘运动学：turn=(right-left)/2。正值代表右轮更快，即左转。 */
    turn_feedforward_rpm = (60.0f / wheel_circumference_m) *
                           (0.5f * wheel_base_m) * yaw_rate_ref_rad_s;
    turn_target_rpm = turn_feedforward_rpm + turn_feedback_rpm;
    max_turn_rpm = (s_params.turn_max_angle > 0.0f) ?
                   s_params.turn_max_angle : LINE_TRACK_TURN_MAX_ANGLE;
    turn_target_rpm = line_track_clamp_float(
        turn_target_rpm, -max_turn_rpm, max_turn_rpm);
    s_model_turn_output_rpm = line_track_slew_step(
        s_model_turn_output_rpm,
        turn_target_rpm,
        LINE_TRACK_MODEL_TURN_SLEW_RPM_PER_S * dt_s);

    if (!isfinite(error_m) || !isfinite(curvature_raw) ||
        !isfinite(curvature) || !isfinite(base_planned_rpm) ||
        !isfinite(yaw_rate_ref_rad_s) || !isfinite(turn_feedforward_rpm) ||
        !isfinite(turn_feedback_rpm) || !isfinite(s_model_turn_output_rpm)) {
        return false;
    }

    out->left_target_rpm = base_planned_rpm - s_model_turn_output_rpm;
    out->right_target_rpm = base_planned_rpm + s_model_turn_output_rpm;
    out->base_target_rpm = base_planned_rpm;
    out->turn_diff_rpm = s_model_turn_output_rpm;
    out->imu_correction_rpm = turn_feedback_rpm;
    out->line_error_m = error_m;
    out->curvature_raw_m_inv = curvature_raw;
    out->curvature_m_inv = curvature;
    out->base_request_rpm = base_request_rpm;
    out->base_planned_rpm = base_planned_rpm;
    out->base_accel_rpm_s = base_accel_rpm_s;
    out->yaw_rate_ref_dps = yaw_rate_ref_rad_s * rad_to_deg;
    out->yaw_rate_measured_dps = yaw_rate_measured_rad_s * rad_to_deg;
    out->yaw_rate_error_dps =
        (yaw_rate_ref_rad_s - yaw_rate_measured_rad_s) * rad_to_deg;
    out->turn_feedforward_rpm = turn_feedforward_rpm;
    out->turn_feedback_rpm = turn_feedback_rpm;
    out->model_valid = true;
    out->model_imu_rate_valid = imu_rate_valid;
    return true;
#else
    (void)line_error;
    (void)legacy_base_rpm;
    (void)legacy_turn_rpm;
    (void)is_lost;
    (void)force_right_angle;
    return false;
#endif
}

/**
 * 红外居中时按左右RPM差校正。起步阶段使用更强增益；若一轮已动而另一轮
 * 仍接近0，则主动压低快轮并抬高慢轮，避免等到车身明显偏转后才补救。
 */
static void line_track_apply_straight_sync(line_track_output_t *out,
                                           bool is_lost)
{
    int32_t left_rpm;
    int32_t right_rpm;
    float correction_rpm;
    float kp;
    float max_correction_rpm;
    const bool startup = line_track_is_startup_window();

    out->sync_correction_rpm = 0.0f;
    if ((LINE_TRACK_STRAIGHT_SYNC_ENABLE == 0U) ||
        !line_track_is_straight_like(out, is_lost)) {
        return;
    }

    left_rpm = line_track_abs_i32(s_left_rpm_feedback);
    right_rpm = line_track_abs_i32(s_right_rpm_feedback);
    if ((left_rpm < LINE_TRACK_STRAIGHT_SYNC_MIN_RPM) &&
        (right_rpm < LINE_TRACK_STRAIGHT_SYNC_MIN_RPM)) {
        return;
    }

    kp = startup ? LINE_TRACK_STARTUP_SYNC_KP : LINE_TRACK_STRAIGHT_SYNC_KP;
    max_correction_rpm = startup ? LINE_TRACK_STARTUP_SYNC_MAX_RPM :
                                   LINE_TRACK_STRAIGHT_SYNC_MAX_RPM;
    correction_rpm = kp * (float)(left_rpm - right_rpm);

    if (startup &&
        (left_rpm >= LINE_TRACK_STARTUP_MOVING_RPM) &&
        (right_rpm <= LINE_TRACK_STARTUP_STALLED_RPM) &&
        (correction_rpm < LINE_TRACK_STARTUP_MISMATCH_RPM)) {
        correction_rpm = LINE_TRACK_STARTUP_MISMATCH_RPM;
    } else if (startup &&
               (right_rpm >= LINE_TRACK_STARTUP_MOVING_RPM) &&
               (left_rpm <= LINE_TRACK_STARTUP_STALLED_RPM) &&
               (correction_rpm > -LINE_TRACK_STARTUP_MISMATCH_RPM)) {
        correction_rpm = -LINE_TRACK_STARTUP_MISMATCH_RPM;
    }

    correction_rpm = line_track_clamp_float(
        correction_rpm, -max_correction_rpm, max_correction_rpm);
    out->left_target_rpm -= correction_rpm;
    out->right_target_rpm += correction_rpm;
    out->sync_correction_rpm = correction_rpm;
}

static float line_track_wrap_angle_deg(float angle_deg)
{
    while (angle_deg > 180.0f) angle_deg -= 360.0f;
    while (angle_deg < -180.0f) angle_deg += 360.0f;
    return angle_deg;
}


static void line_track_reset_imu_assist(void)
{
    s_imu_straight_cycles = 0U;
    s_imu_heading_locked = false;
    s_imu_heading_ref_deg = 0.0f;
    s_imu_gyro_filtered_dps = 0.0f;
    s_imu_correction_f = 0.0f;
    s_imu_processed_timestamp_ms = 0U;
}

/** 直线段使用yaw保持航向、gyro_z快速抑制起步偏航；弯道和丢线时立即退出。 */
static void line_track_apply_imu_assist(line_track_output_t *out,
                                        bool is_lost)
{
    float signed_yaw;
    float signed_gyro;
    float correction_target;

    out->imu_correction_rpm = 0.0f;
    if ((LINE_TRACK_IMU_ASSIST_ENABLE == 0U) ||
        !line_track_is_straight_like(out, is_lost) ||
        (s_imu_timestamp_ms == 0U) ||
        (s_imu_age_ms > LINE_TRACK_IMU_MAX_AGE_MS) ||
        !isfinite(s_imu_yaw_deg) || !isfinite(s_imu_gyro_z_dps) ||
        !isfinite(s_imu_accel_x_g) || !isfinite(s_imu_accel_y_g) ||
        !isfinite(s_imu_accel_z_g)) {
        line_track_reset_imu_assist();
        return;
    }

    if (s_imu_straight_cycles < UINT16_MAX) {
        s_imu_straight_cycles++;
    }
    if (s_imu_straight_cycles < LINE_TRACK_IMU_STRAIGHT_CONFIRM_CYCLES) {
        return;
    }

    signed_yaw = LINE_TRACK_IMU_YAW_SIGN * s_imu_yaw_deg;
    signed_gyro = LINE_TRACK_IMU_YAW_SIGN * s_imu_gyro_z_dps;
    if (!s_imu_heading_locked) {
        s_imu_heading_locked = true;
        s_imu_heading_ref_deg = signed_yaw;
        s_imu_gyro_filtered_dps = signed_gyro;
        s_imu_processed_timestamp_ms = 0U;
    }

    /* IMU任务约100Hz，同一时间戳只计算一次，2ms循迹周期保持上次修正。 */
    if (s_imu_timestamp_ms != s_imu_processed_timestamp_ms) {
        float target_yaw_term = 0.0f;
        const float accel_norm_g = sqrtf(
            s_imu_accel_x_g * s_imu_accel_x_g +
            s_imu_accel_y_g * s_imu_accel_y_g +
            s_imu_accel_z_g * s_imu_accel_z_g);

        s_imu_gyro_filtered_dps += LINE_TRACK_IMU_GYRO_FILTER_ALPHA *
            (signed_gyro - s_imu_gyro_filtered_dps);

        /* 强振动时暂时不用yaw慢环，但保留gyro_z快速阻尼。 */
        if ((accel_norm_g >= LINE_TRACK_IMU_ACCEL_NORM_MIN_G) &&
            (accel_norm_g <= LINE_TRACK_IMU_ACCEL_NORM_MAX_G)) {
            const float heading_error = line_track_clamp_float(
                line_track_wrap_angle_deg(s_imu_heading_ref_deg - signed_yaw),
                -LINE_TRACK_IMU_HEADING_ERROR_MAX_DEG,
                 LINE_TRACK_IMU_HEADING_ERROR_MAX_DEG);
            target_yaw_term = LINE_TRACK_IMU_YAW_KP_RPM_PER_DEG *
                              heading_error;
        }

        correction_target = target_yaw_term -
            LINE_TRACK_IMU_GYRO_KD_RPM_PER_DPS *
            s_imu_gyro_filtered_dps;
        correction_target = line_track_clamp_float(
            correction_target,
            -LINE_TRACK_IMU_TRIM_MAX_RPM,
             LINE_TRACK_IMU_TRIM_MAX_RPM);

        if (correction_target > s_imu_correction_f +
                                LINE_TRACK_IMU_TRIM_SLEW_RPM_PER_SAMPLE) {
            s_imu_correction_f += LINE_TRACK_IMU_TRIM_SLEW_RPM_PER_SAMPLE;
        } else if (correction_target < s_imu_correction_f -
                                       LINE_TRACK_IMU_TRIM_SLEW_RPM_PER_SAMPLE) {
            s_imu_correction_f -= LINE_TRACK_IMU_TRIM_SLEW_RPM_PER_SAMPLE;
        } else {
            s_imu_correction_f = correction_target;
        }
        s_imu_processed_timestamp_ms = s_imu_timestamp_ms;
    }

    out->left_target_rpm -= s_imu_correction_f;
    out->right_target_rpm += s_imu_correction_f;
    out->imu_correction_rpm = s_imu_correction_f;
}


void app_line_track_init(void)
{
    memset(&s_last_output, 0, sizeof(s_last_output));
    s_running = false;
#if (LINE_TRACK_MODEL_CONTROL_ENABLE != 0U)
    app_speed_planner_init(&s_model_speed_planner,
                           LINE_TRACK_MODEL_MAX_ACCEL_RPM_S,
                           LINE_TRACK_MODEL_MAX_JERK_RPM_S2);
#endif
    app_line_track_reset();
    app_line_track_restore_default_params();
}

void app_line_track_start(void)
{
    /* 启动不自动使能电机功率，只允许控制任务开始输出命令。 */
    app_line_track_reset();
    s_running = true;
}

void app_line_track_stop(void)
{
    s_running = false;
    app_line_track_reset();
}

bool app_line_track_is_running(void)
{
    return s_running;
}

bool app_line_track_step(void)
{
    if (!s_running) {
        return false;
    }

    /* 只生成目标RPM，电机输出统一由task_control中的速度PI完成。 */
    app_line_track_update(&s_last_output);
    return true;
}

void app_line_track_stop_motors(void)
{
    bsp_motor_stop_all();
}

void app_line_track_reset(void)
{
    s_turn_cnt = 0;
    s_saved_state = LINE_TRACK_STATE_CROSS;
    s_last_valid_state = LINE_TRACK_STATE_STRAIGHT;
    s_last_line_error = 0.0f;
    s_prev_control_error = 0.0f;
    s_turn_output = 0.0f;
    s_lost_cycles = 0U;
    s_left_rpm_feedback = 0;
    s_right_rpm_feedback = 0;
    s_run_elapsed_ms = 0U;
    s_imu_yaw_deg = 0.0f;
    s_imu_gyro_z_dps = 0.0f;
    s_imu_accel_x_g = 0.0f;
    s_imu_accel_y_g = 0.0f;
    s_imu_accel_z_g = 0.0f;
    s_imu_timestamp_ms = 0U;
    s_imu_age_ms = UINT32_MAX;
    line_track_reset_imu_assist();
#if (LINE_TRACK_MODEL_CONTROL_ENABLE != 0U)
    if (app_speed_planner_is_valid(&s_model_speed_planner)) {
        app_speed_planner_reset(&s_model_speed_planner, 0.0f);
    }
    s_model_turn_output_rpm = 0.0f;
    s_model_gyro_filtered_rad_s = 0.0f;
    s_model_gyro_timestamp_ms = 0U;
#endif
    memset(s_filter_history, 0, sizeof(s_filter_history));
    s_filter_index = 0U;
    s_filter_initialized = false;
    memset(&s_last_output, 0, sizeof(s_last_output));
}

void app_line_track_set_speed_feedback(int32_t left_rpm, int32_t right_rpm)
{
    s_left_rpm_feedback = left_rpm;
    s_right_rpm_feedback = right_rpm;
}

void app_line_track_set_imu_feedback(float yaw_deg,
                                     float gyro_z_dps,
                                     float accel_x_g,
                                     float accel_y_g,
                                     float accel_z_g,
                                     uint32_t sample_timestamp_ms,
                                     uint32_t age_ms)
{
    s_imu_yaw_deg = yaw_deg;
    s_imu_gyro_z_dps = gyro_z_dps;
    s_imu_accel_x_g = accel_x_g;
    s_imu_accel_y_g = accel_y_g;
    s_imu_accel_z_g = accel_z_g;
    s_imu_timestamp_ms = sample_timestamp_ms;
    s_imu_age_ms = age_ms;
}

void app_line_track_restore_default_params(void)
{
    s_params.turn90_angle = LINE_TRACK_TURN90_ANGLE;
    s_params.turn_max_angle = LINE_TRACK_TURN_MAX_ANGLE;
    s_params.turn_mid_angle = LINE_TRACK_TURN_MID_ANGLE;
    s_params.turn_min_angle = LINE_TRACK_TURN_MIN_ANGLE;
    s_params.base_speed = LINE_TRACK_BASE_SPEED;
    s_params.forward_limit = LINE_TRACK_FORWARD_LIMIT;
}

line_track_params_t *app_line_track_get_params(void)
{
    return &s_params;
}

void app_line_track_update(line_track_output_t *out)
{
    uint8_t ir[BSP_IR_CHANNEL_COUNT];
    uint8_t black_mask;
    int sensor_state;
    float line_error = 0.0f;
    float target_turn = 0.0f;
    float base_rpm;
    bool is_lost;
    bool force_right_angle = false;
    bool model_applied;

    if (out == NULL) {
        return;
    }

    if (s_run_elapsed_ms <= (UINT32_MAX - PRJ_CONTROL_PERIOD_MS)) {
        s_run_elapsed_ms += PRJ_CONTROL_PERIOD_MS;
    }

    BSP_IR_Read(ir);
    (void)memcpy(out->ir_raw, ir, sizeof(out->ir_raw));

    /* 对外保留旧状态字语义，同时内部统一使用1=黑线，避免算法反复取反。 */
    black_mask = line_track_filter_black_mask(line_track_make_black_mask(ir));
    sensor_state = (int)((~black_mask) & LINE_TRACK_MASK_ALL);
    out->black_mask = black_mask;
    out->sensor_bits = (uint8_t)sensor_state;
    is_lost = (black_mask == 0U);
    if (!is_lost) {
        s_lost_cycles = 0U;
    }

    /*
     * 赛题是连续半圆弧，默认不启用直角保持。
     * 该兼容分支只供以后真正的直角赛道使用。
     */
    if (LINE_TRACK_RIGHT_ANGLE_ENABLE != 0U) {
        if (((sensor_state == LINE_TRACK_STATE_LEFT_90_A) ||
             (sensor_state == LINE_TRACK_STATE_RIGHT_90_A) ||
             (sensor_state == LINE_TRACK_STATE_LEFT_90_B) ||
             (sensor_state == LINE_TRACK_STATE_RIGHT_90_B)) &&
            (s_turn_cnt == 0)) {
            s_saved_state = sensor_state;
            s_turn_cnt = 1;
        }

        if (s_turn_cnt > 0) {
            if (s_turn_cnt < (int)LINE_TRACK_TURN90_HOLD_CYCLES) {
                target_turn = 0.0f;
                force_right_angle = true;
            } else if ((s_turn_cnt < (int)LINE_TRACK_TURN90_MAX_CYCLES) &&
                       (sensor_state != LINE_TRACK_STATE_LEFT_BIG) &&
                       (sensor_state != LINE_TRACK_STATE_RIGHT_BIG)) {
                target_turn = ((s_saved_state == LINE_TRACK_STATE_LEFT_90_A) ||
                               (s_saved_state == LINE_TRACK_STATE_LEFT_90_B)) ?
                              s_params.turn90_angle : -s_params.turn90_angle;
                force_right_angle = true;
            } else {
                s_turn_cnt = 0;
                s_saved_state = LINE_TRACK_STATE_CROSS;
            }

            if (s_turn_cnt > 0) {
                s_turn_cnt++;
            }
        }
    } else {
        s_turn_cnt = 0;
        s_saved_state = LINE_TRACK_STATE_CROSS;
    }
    out->turn90_active = (s_turn_cnt > 0);

    if (!force_right_angle) {
        if ((black_mask & LINE_TRACK_CROSS_BLACK_MASK) == LINE_TRACK_CROSS_BLACK_MASK) {
            /* 启停横线：保持直行，停车由单圈管理器确认后统一执行。 */
            sensor_state = LINE_TRACK_STATE_CROSS;
            line_error = 0.0f;
            target_turn = 0.0f;
            s_last_valid_state = sensor_state;
            s_last_line_error = 0.0f;
            s_prev_control_error = 0.0f;
        } else if (is_lost) {
            float search_angle = s_params.turn_mid_angle;
            uint32_t ramp_cycles = LINE_TRACK_LOST_SEARCH_RAMP_MS /
                                   PRJ_CONTROL_PERIOD_MS;

            if (ramp_cycles == 0U) {
                ramp_cycles = 1U;
            }
            if (s_lost_cycles < UINT32_MAX) {
                s_lost_cycles++;
            }

            /*
             * 保存最近一次非丢线状态：左偏向左搜，右偏向右搜，直行则低速直行。
             * 搜索转向在约120ms内从中档平滑增大到大档，重新见线后立即退出。
             */
            if (s_lost_cycles < ramp_cycles) {
                float ratio = (float)s_lost_cycles / (float)ramp_cycles;
                search_angle += (s_params.turn_max_angle - search_angle) * ratio;
            } else {
                search_angle = s_params.turn_max_angle;
            }

            if (s_last_line_error > 0.05f) {
                target_turn = search_angle;
            } else if (s_last_line_error < -0.05f) {
                target_turn = -search_angle;
            } else {
                target_turn = 0.0f;
            }
            line_error = s_last_line_error;
            s_prev_control_error = line_error;
        } else {
            /*
             * 位置式PD外环：非线性P负责主要转向，D只抑制误差快速变化。
             * 数字红外直接使用相邻采样差值，避免除以2ms后放大跳变噪声。
             */
            float error_step;
            s_lost_cycles = 0U;
            line_error = line_track_error_from_mask(black_mask);
            error_step = line_error - s_prev_control_error;
            target_turn = line_track_turn_from_error(line_error) +
                          LINE_TRACK_KD_RPM_PER_STEP * error_step;
            target_turn = line_track_clamp_float(
                target_turn, -s_params.turn_max_angle,
                s_params.turn_max_angle);
            s_prev_control_error = line_error;

            /* 包括直行的0偏差也要保存，避免丢线时沿用很久以前的转弯方向。 */
            s_last_valid_state = sensor_state;
            s_last_line_error = line_error;
        }
    }

    s_turn_output = line_track_slew(s_turn_output, target_turn);

    if (is_lost) {
        float lost_ratio = LINE_TRACK_LOST_SPEED_RATIO;
        if (lost_ratio < 0.0f) {
            lost_ratio = 0.0f;
        } else if (lost_ratio > 1.0f) {
            lost_ratio = 1.0f;
        }
        base_rpm = s_params.base_speed * lost_ratio;
        if (fabsf(s_last_line_error) <= 0.05f) {
            /* 没有可参考的搜索方向时进一步降速，等待重新捕获黑线。 */
            base_rpm *= 0.5f;
        }
    } else {
        float limit = s_params.forward_limit;
        float curve_ratio = LINE_TRACK_CURVE_MIN_SPEED_RATIO;
        float turn_ratio;

        if (limit <= 0.0f) {
            limit = (s_params.turn_max_angle > 0.0f) ?
                    s_params.turn_max_angle : 1.0f;
        }
        turn_ratio = fabsf(s_turn_output) / limit;
        if (turn_ratio > 1.0f) {
            turn_ratio = 1.0f;
        }
        if (curve_ratio < 0.0f) {
            curve_ratio = 0.0f;
        } else if (curve_ratio > 1.0f) {
            curve_ratio = 1.0f;
        }

        /* 圆弧中保留最低速度，减少对滚球系统不利的反复加减速。 */
        base_rpm = s_params.base_speed *
                        (1.0f - ((1.0f - curve_ratio) * turn_ratio));
    }

    if (base_rpm < 0.0f) {
        base_rpm = 0.0f;
    }
    base_rpm = line_track_apply_startup_ramp(base_rpm);

    out->line_error = line_error;
    out->current_state = sensor_state;
    out->last_valid_state = s_last_valid_state;
    out->lost_cycles = (s_lost_cycles > UINT16_MAX) ?
                       UINT16_MAX : (uint16_t)s_lost_cycles;

    model_applied = line_track_try_model(out,
                                         line_error,
                                         base_rpm,
                                         s_turn_output,
                                         is_lost,
                                         force_right_angle);
    if (!model_applied) {
        out->left_target_rpm = base_rpm - s_turn_output;
        out->right_target_rpm = base_rpm + s_turn_output;
        out->base_target_rpm = base_rpm;
        out->turn_diff_rpm = s_turn_output;

        /* 旧算法的两个辅助默认关闭，只修正目标RPM，不直接碰PWM。 */
        line_track_apply_straight_sync(out, is_lost);
        line_track_apply_imu_assist(out, is_lost);
    } else {
        /* 模型已包含gyro角速度反馈，不能再叠加旧yaw保持和双轮同步修正。 */
        out->sync_correction_rpm = 0.0f;
    }

    /*
     * 最终目标是进入速度PI的安全边界。未来从BLE修改运行参数时，
     * 即使写入NaN/Inf也不允许传到PID或电机；本周期置零并清除规划状态。
     */
    if (!isfinite(out->left_target_rpm) ||
        !isfinite(out->right_target_rpm)) {
        out->left_target_rpm = 0.0f;
        out->right_target_rpm = 0.0f;
        out->base_target_rpm = 0.0f;
        out->turn_diff_rpm = 0.0f;
        out->sync_correction_rpm = 0.0f;
        out->imu_correction_rpm = 0.0f;
        out->model_valid = false;
        s_turn_output = 0.0f;
#if (LINE_TRACK_MODEL_CONTROL_ENABLE != 0U)
        if (app_speed_planner_is_valid(&s_model_speed_planner)) {
            app_speed_planner_reset(&s_model_speed_planner, 0.0f);
        }
        s_model_turn_output_rpm = 0.0f;
#endif
    }

    /* 始终保留IMU原始诊断量，供UART0/UART1或后续BLE日志复用。 */
    {
        const float signed_yaw = LINE_TRACK_IMU_YAW_SIGN * s_imu_yaw_deg;
        const float signed_ref = s_imu_heading_ref_deg;
        out->imu_yaw_deg = s_imu_yaw_deg;
        out->imu_heading_ref_deg = s_imu_heading_locked ? signed_ref : signed_yaw;
        out->imu_heading_error_deg = s_imu_heading_locked ?
            line_track_wrap_angle_deg(signed_ref - signed_yaw) : 0.0f;
        out->imu_gyro_z_dps = s_imu_gyro_z_dps;
        out->imu_gyro_filtered_dps = s_imu_gyro_filtered_dps;
        out->imu_accel_norm_g = sqrtf(
            s_imu_accel_x_g * s_imu_accel_x_g +
            s_imu_accel_y_g * s_imu_accel_y_g +
            s_imu_accel_z_g * s_imu_accel_z_g);
        out->imu_age_ms = s_imu_age_ms;
        out->imu_straight_cycles = s_imu_straight_cycles;
        out->imu_valid = (s_imu_timestamp_ms != 0U) &&
                         (s_imu_age_ms <= LINE_TRACK_IMU_MAX_AGE_MS) &&
                         isfinite(s_imu_yaw_deg) &&
                         isfinite(s_imu_gyro_z_dps) &&
                         isfinite(out->imu_accel_norm_g);
        out->imu_heading_locked = s_imu_heading_locked;
    }

#if (LINE_TRACK_ALLOW_REVERSE == 0U)
    /* 普通圆弧循迹不需要单轮反转，首次调试可明显降低机械冲击。 */
    if (out->left_target_rpm < 0.0f) {
        out->left_target_rpm = 0.0f;
    }
    if (out->right_target_rpm < 0.0f) {
        out->right_target_rpm = 0.0f;
    }
#endif

    /* 目标RPM受项目统一速度上限约束，最终PWM还会再经过电机命令硬限幅。 */
    out->left_target_rpm = line_track_clamp_float(
        out->left_target_rpm, -PRJ_PLANNER_MAX_RPM, PRJ_PLANNER_MAX_RPM);
    out->right_target_rpm = line_track_clamp_float(
        out->right_target_rpm, -PRJ_PLANNER_MAX_RPM, PRJ_PLANNER_MAX_RPM);
    out->turn_diff_rpm = 0.5f *
        (out->right_target_rpm - out->left_target_rpm);
}

const line_track_output_t *app_line_track_get_output(void)
{
    return &s_last_output;
}

bool app_line_track_get_debug(line_track_debug_t *debug)
{
    if (debug == NULL) {
        return false;
    }
    *debug = s_last_output;
    return true;
}

const char *app_line_track_state_name(int state)
{
    switch (state) {
    case LINE_TRACK_STATE_CROSS:       return "CROSS";
    case LINE_TRACK_STATE_LEFT_90_A:   return "L90A";
    case LINE_TRACK_STATE_LEFT_90_B:   return "L90B";
    case LINE_TRACK_STATE_RIGHT_90_A:  return "R90A";
    case LINE_TRACK_STATE_RIGHT_90_B:  return "R90B";
    case LINE_TRACK_STATE_LEFT_BIG:    return "L_BIG";
    case LINE_TRACK_STATE_RIGHT_BIG:   return "R_BIG";
    case LINE_TRACK_STATE_LEFT_SMALL:  return "L_MIN";
    case LINE_TRACK_STATE_RIGHT_SMALL: return "R_MIN";
    case LINE_TRACK_STATE_STRAIGHT:    return "STR";
    case LINE_TRACK_STATE_LOST:        return "LOST";
    case 2:                            return "MIX_2";
    case 4:                            return "MIX_4";
    case 5:                            return "MIX_5";
    case 6:                            return "MIX_6";
    case 10:                           return "MIX_10";
    default:                           return "UNK";
    }
}

#else

/* 未启用循迹时保留完整空实现，保证不同 Keil 目标可以共用头文件。 */
void app_line_track_init(void)
{
    memset(&s_last_output, 0, sizeof(s_last_output));
    s_running = false;
}

void app_line_track_start(void)
{
    s_running = false;
}

void app_line_track_stop(void)
{
    s_running = false;
}

bool app_line_track_is_running(void)
{
    return false;
}

bool app_line_track_step(void)
{
    return false;
}

void app_line_track_reset(void)
{
    memset(&s_last_output, 0, sizeof(s_last_output));
}

void app_line_track_set_speed_feedback(int32_t left_rpm, int32_t right_rpm)
{
    (void)left_rpm;
    (void)right_rpm;
}

void app_line_track_set_imu_feedback(float yaw_deg,
                                     float gyro_z_dps,
                                     float accel_x_g,
                                     float accel_y_g,
                                     float accel_z_g,
                                     uint32_t sample_timestamp_ms,
                                     uint32_t age_ms)
{
    (void)yaw_deg;
    (void)gyro_z_dps;
    (void)accel_x_g;
    (void)accel_y_g;
    (void)accel_z_g;
    (void)sample_timestamp_ms;
    (void)age_ms;
}

void app_line_track_update(line_track_output_t *out)
{
    if (out != NULL) {
        memset(out, 0, sizeof(*out));
    }
}

const line_track_output_t *app_line_track_get_output(void)
{
    return &s_last_output;
}

bool app_line_track_get_debug(line_track_debug_t *debug)
{
    if (debug == NULL) {
        return false;
    }
    *debug = s_last_output;
    return true;
}

void app_line_track_stop_motors(void)
{
}

line_track_params_t *app_line_track_get_params(void)
{
    return NULL;
}

void app_line_track_restore_default_params(void)
{
}

const char *app_line_track_state_name(int state)
{
    (void)state;
    return "OFF";
}

#endif /* APP_LINE_TRACK_BUILD_ENABLE */
