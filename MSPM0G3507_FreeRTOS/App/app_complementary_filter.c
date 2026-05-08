/**
 * @file    app_complementary_filter.c
 * @brief   互补滤波器实现
 * @note    融合编码器和IMU数据实现姿态估计:
 *
 *          线速度融合:
 *          v_imu = v_prev + accel_x_g × 9.81 × dt
 *          v_fused = α × v_encoder + (1-α) × v_imu
 *
 *          航向角估计:
 *          heading += gyro_z_dps × dt (陀螺仪积分)
 *
 *          位移积分:
 *          x += vx × cos(heading) × dt
 *          y += vx × sin(heading) × dt
 */
#include "app_complementary_filter.h"
#include "project_config.h"
#include <math.h>

#ifndef M_PI
#define M_PI (3.14159265358979323846f)
#endif

/** 重力加速度常量(m/s²) */
#define GRAVITY_MS2 (9.80665f)

/** 角度→弧度转换系数 */
#define DEG2RAD     (M_PI / 180.0f)

/** 弧度→角度转换系数 */
#define RAD2DEG     (180.0f / M_PI)

/* ======================== 私有变量 ======================== */

/** 初始化标志 */
static bool s_cf_inited = false;

/** 滤波器配置 */
static app_cf_config_t s_cfg;

/** 融合后X轴线速度(m/s) */
static float s_vx = 0.0f;

/** 航向角(弧度) */
static float s_heading_rad = 0.0f;

/** X轴位移(m) */
static float s_x = 0.0f;

/** Y轴位移(m) */
static float s_y = 0.0f;

/* ======================== 公共函数实现 ======================== */

void app_cf_init(const app_cf_config_t *cfg)
{
    if (cfg != NULL) {
        s_cfg = *cfg;
    } else {
        /* 使用project_config.h中的默认值 */
        s_cfg.alpha          = PRJ_CF_ALPHA;
        s_cfg.wheel_radius_m = PRJ_CF_WHEEL_RADIUS_M;
        s_cfg.wheel_base_m   = PRJ_CF_WHEEL_BASE_M;
    }

    /* 参数范围保护 */
    if (s_cfg.alpha < 0.0f) { s_cfg.alpha = 0.0f; }
    if (s_cfg.alpha > 1.0f) { s_cfg.alpha = 1.0f; }
    if (s_cfg.wheel_radius_m <= 0.0f) { s_cfg.wheel_radius_m = 0.03f; }
    if (s_cfg.wheel_base_m <= 0.0f) { s_cfg.wheel_base_m = 0.15f; }

    s_vx         = 0.0f;
    s_heading_rad = 0.0f;
    s_x          = 0.0f;
    s_y          = 0.0f;

    s_cf_inited = true;
}

void app_cf_update(float encoder_vx_mps, float imu_accel_x_g,
                   float imu_gyro_z_dps, float dt_s)
{
    if (!s_cf_inited) {
        return;
    }

    /* 参数保护 */
    if (dt_s <= 0.0f || dt_s > 1.0f) {
        return;  /* 异常采样周期, 跳过 */
    }

    /* ---- 线速度融合 ---- */
    /* IMU加速度积分: v_imu = v_prev + accel_x × g × dt */
    float v_imu = s_vx + imu_accel_x_g * GRAVITY_MS2 * dt_s;

    /* 互补滤波: v_fused = α × v_encoder + (1-α) × v_imu */
    s_vx = s_cfg.alpha * encoder_vx_mps + (1.0f - s_cfg.alpha) * v_imu;

    /* ---- 航向角估计(陀螺仪积分) ---- */
    s_heading_rad += imu_gyro_z_dps * DEG2RAD * dt_s;

    /* 角度归一化到[-π, π] */
    while (s_heading_rad > M_PI) {
        s_heading_rad -= 2.0f * M_PI;
    }
    while (s_heading_rad < -M_PI) {
        s_heading_rad += 2.0f * M_PI;
    }

    /* ---- 位移积分 ---- */
    s_x += s_vx * cosf(s_heading_rad) * dt_s;
    s_y += s_vx * sinf(s_heading_rad) * dt_s;
}

float app_cf_get_velocity_x(void)
{
    return s_vx;
}

float app_cf_get_heading(void)
{
    return s_heading_rad * RAD2DEG;
}

float app_cf_get_x(void)
{
    return s_x;
}

float app_cf_get_y(void)
{
    return s_y;
}

void app_cf_get_output(app_cf_output_t *out)
{
    if (out == NULL) {
        return;
    }

    out->vx_mps      = s_vx;
    out->heading_deg  = s_heading_rad * RAD2DEG;
    out->x_m          = s_x;
    out->y_m          = s_y;
}

void app_cf_reset(void)
{
    s_vx         = 0.0f;
    s_heading_rad = 0.0f;
    s_x          = 0.0f;
    s_y          = 0.0f;
}

bool app_cf_is_inited(void)
{
    return s_cf_inited;
}