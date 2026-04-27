/**
 * @file    bsp_mpu6050.h
 * @brief   MPU6050 6-axis sensor interface (accelerometer + gyroscope + temperature)
 * @note    I2C communication, default address 0x68 (AD0=0)
 */

#ifndef BSP_MPU6050_H
#define BSP_MPU6050_H

#ifdef __cplusplus
extern "C" {
#endif

#include "hal_common.h"

/* ---------- Accelerometer full-scale range ---------- */
typedef enum {
    BSP_MPU6050_ACCEL_2G  = 0,   /* ±2g  (sensitivity: 16384 LSB/g) */
    BSP_MPU6050_ACCEL_4G  = 1,   /* ±4g  (sensitivity: 8192 LSB/g) */
    BSP_MPU6050_ACCEL_8G  = 2,   /* ±8g  (sensitivity: 4096 LSB/g) */
    BSP_MPU6050_ACCEL_16G = 3,   /* ±16g (sensitivity: 2048 LSB/g) */
} bsp_mpu6050_accel_range_t;

/* ---------- Gyroscope full-scale range ---------- */
typedef enum {
    BSP_MPU6050_GYRO_250  = 0,   /* ±250 °/s  (sensitivity: 131 LSB/°/s) */
    BSP_MPU6050_GYRO_500  = 1,   /* ±500 °/s  (sensitivity: 65.5 LSB/°/s) */
    BSP_MPU6050_GYRO_1000 = 2,   /* ±1000 °/s (sensitivity: 32.8 LSB/°/s) */
    BSP_MPU6050_GYRO_2000 = 3,   /* ±2000 °/s (sensitivity: 16.4 LSB/°/s) */
} bsp_mpu6050_gyro_range_t;

/* ---------- 3-axis data structure ---------- */
typedef struct {
    int16_t x;
    int16_t y;
    int16_t z;
} bsp_mpu6050_axis_t;

/* ========== API ========== */

/**
 * @brief  Initialize MPU6050 (wake up, configure ranges)
 */
hal_status_t bsp_mpu6050_init(void);

/**
 * @brief  Deinitialize MPU6050 (put to sleep)
 */
hal_status_t bsp_mpu6050_deinit(void);

/**
 * @brief  Read raw accelerometer data
 * @param  accel  Pointer to store X/Y/Z raw values (16-bit signed)
 */
hal_status_t bsp_mpu6050_read_accel(bsp_mpu6050_axis_t *accel);

/**
 * @brief  Read raw gyroscope data
 * @param  gyro  Pointer to store X/Y/Z raw values (16-bit signed)
 */
hal_status_t bsp_mpu6050_read_gyro(bsp_mpu6050_axis_t *gyro);

/**
 * @brief  Read temperature sensor value
 * @param  temp_c  Pointer to store temperature in °C
 */
hal_status_t bsp_mpu6050_read_temp(float *temp_c);

/**
 * @brief  Set accelerometer full-scale range
 */
hal_status_t bsp_mpu6050_set_accel_range(bsp_mpu6050_accel_range_t range);

/**
 * @brief  Set gyroscope full-scale range
 */
hal_status_t bsp_mpu6050_set_gyro_range(bsp_mpu6050_gyro_range_t range);

/**
 * @brief  Get current accelerometer sensitivity (LSB/g)
 * @return Sensitivity value, or 0 on error
 */
float bsp_mpu6050_get_accel_sensitivity(void);

/**
 * @brief  Get current gyroscope sensitivity (LSB/(°/s))
 * @return Sensitivity value, or 0 on error
 */
float bsp_mpu6050_get_gyro_sensitivity(void);

#ifdef __cplusplus
}
#endif

#endif /* BSP_MPU6050_H */
