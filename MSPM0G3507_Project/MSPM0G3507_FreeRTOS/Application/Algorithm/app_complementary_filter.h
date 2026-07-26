/**
 * @file    app_complementary_filter.h
 * @brief   互补滤波器接口
 * @note    融合编码器和IMU数据实现姿态估计:
 *          - 线速度: v_fused = α × v_encoder + (1-α) × (v_prev + accel × g × dt)
 *          - 航向角: heading += gyro_z × dt (陀螺仪积分)
 *          App层算法模块，不直接依赖硬件
 */
#ifndef APP_COMPLEMENTARY_FILTER_H
#define APP_COMPLEMENTARY_FILTER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

/* ======================== 类型定义 ======================== */

/**
 * @brief 互补滤波器配置结构体
 */
typedef struct {
    float alpha;            /**< 滤波系数(0~1)
                                 0=全信任IMU, 1=全信任编码器
                                 典型值: 0.95~0.98 */
    float wheel_radius_m;   /**< 轮子半径(m), 用于RPM→线速度转换 */
    float wheel_base_m;     /**< 轮距(m, 左右轮距离), 用于航向计算 */
} app_cf_config_t;

/**
 * @brief 互补滤波器输出数据
 */
typedef struct {
    float vx_mps;           /**< X轴融合后线速度(m/s) */
    float heading_deg;      /**< 航向角(度, ±180) */
    float x_m;              /**< X轴位移估计(m) */
    float y_m;              /**< Y轴位移估计(m) */
} app_cf_output_t;

/* ======================== 函数接口 ======================== */

/**
 * @brief  初始化互补滤波器
 * @param  cfg  配置参数指针
 * @note   使用默认值: alpha=0.98, wheel_radius=0.03m, wheel_base=0.15m
 *         可通过cfg传入自定义值
 */
void app_cf_init(const app_cf_config_t *cfg);

/**
 * @brief  更新互补滤波器
 * @param  encoder_vx_mps  编码器计算的X轴线速度(m/s)
 *                         正值=前进
 * @param  imu_accel_x_g  IMU X轴加速度(g)
 * @param  imu_yaw_deg    IMU DMP偏航角(°)，直接来自四元数融合
 * @param  dt_s           采样周期(秒)
 * @note   每个控制周期调用一次，融合编码器和IMU数据
 */
void app_cf_update(float encoder_vx_mps, float imu_accel_x_g,
                   float imu_yaw_deg, float dt_s);

/**
 * @brief  获取融合后X轴线速度
 * @retval 线速度(m/s), 正值=前进
 */
float app_cf_get_velocity_x(void);

/**
 * @brief  获取融合后航向角
 * @retval 航向角(度, ±180)
 */
float app_cf_get_heading(void);

/**
 * @brief  获取融合后X轴位移
 * @retval X轴位移(m)
 */
float app_cf_get_x(void);

/**
 * @brief  获取融合后Y轴位移
 * @retval Y轴位移(m)
 */
float app_cf_get_y(void);

/**
 * @brief  获取完整输出
 * @param  out  输出结构体指针
 */
void app_cf_get_output(app_cf_output_t *out);

/**
 * @brief  重置互补滤波器状态
 * @note   用于紧急停止或系统复位
 */
void app_cf_reset(void);

/**
 * @brief  检查互补滤波器是否已初始化
 * @retval true  已初始化
 * @retval false 未初始化
 */
bool app_cf_is_inited(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_COMPLEMENTARY_FILTER_H */