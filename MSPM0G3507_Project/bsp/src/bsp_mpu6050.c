/**
 * @file    bsp_mpu6050.c
 * @brief   MPU6050 6-axis sensor implementation (I2C communication)
 */

#include "bsp_mpu6050.h"
#include "bsp_pin_config.h"
#include "bsp_config.h"
#include "bsp_i2c.h"
#include <string.h>

/* ========== MPU6050 Register Map ========== */
#define MPU6050_REG_SMPLRT_DIV     0x19
#define MPU6050_REG_CONFIG         0x1A
#define MPU6050_REG_GYRO_CONFIG    0x1B
#define MPU6050_REG_ACCEL_CONFIG   0x1C
#define MPU6050_REG_INT_ENABLE     0x38
#define MPU6050_REG_INT_STATUS     0x3A
#define MPU6050_REG_ACCEL_XOUT_H   0x3B
#define MPU6050_REG_ACCEL_XOUT_L   0x3C
#define MPU6050_REG_ACCEL_YOUT_H   0x3D
#define MPU6050_REG_ACCEL_YOUT_L   0x3E
#define MPU6050_REG_ACCEL_ZOUT_H   0x3F
#define MPU6050_REG_ACCEL_ZOUT_L   0x40
#define MPU6050_REG_TEMP_OUT_H     0x41
#define MPU6050_REG_TEMP_OUT_L     0x42
#define MPU6050_REG_GYRO_XOUT_H    0x43
#define MPU6050_REG_GYRO_XOUT_L    0x44
#define MPU6050_REG_GYRO_YOUT_H    0x45
#define MPU6050_REG_GYRO_YOUT_L    0x46
#define MPU6050_REG_GYRO_ZOUT_H    0x47
#define MPU6050_REG_GYRO_ZOUT_L    0x48
#define MPU6050_REG_USER_CTRL      0x6A
#define MPU6050_REG_PWR_MGMT_1     0x6B
#define MPU6050_REG_PWR_MGMT_2     0x6C
#define MPU6050_REG_WHO_AM_I       0x75

#define MPU6050_WHO_AM_I_VAL       0x68

/* ---------- Current ranges (for sensitivity calculation) ---------- */
static bsp_mpu6050_accel_range_t s_accel_range = BSP_MPU6050_ACCEL_2G;
static bsp_mpu6050_gyro_range_t  s_gyro_range  = BSP_MPU6050_GYRO_250;

/* ---------- I2C helpers ---------- */

static hal_status_t mpu6050_write_reg(uint8_t reg, uint8_t val)
{
    uint8_t buf[2] = { reg, val };
    return bsp_i2c0_write(BSP_MPU6050_I2C_ADDR, buf, 2, BSP_MPU6050_TIMEOUT_MS);
}

static hal_status_t mpu6050_read_reg(uint8_t reg, uint8_t *buf, uint16_t len)
{
    return bsp_i2c0_write_read(BSP_MPU6050_I2C_ADDR,
                                &reg, 1, buf, len, BSP_MPU6050_TIMEOUT_MS);
}

/* ========== API ========== */

hal_status_t bsp_mpu6050_init(void)
{
    hal_status_t status;
    uint8_t who_am_i;

    /* Verify device identity */
    status = mpu6050_read_reg(MPU6050_REG_WHO_AM_I, &who_am_i, 1);
    if (status != HAL_OK) return status;
    if (who_am_i != MPU6050_WHO_AM_I_VAL) return HAL_ERROR;

    /* Wake up: clear SLEEP bit, use PLL with X gyro reference */
    status = mpu6050_write_reg(MPU6050_REG_PWR_MGMT_1, 0x01);
    if (status != HAL_OK) return status;

    /* Sample rate divider: 1 kHz / (1 + 7) = 125 Hz */
    status = mpu6050_write_reg(MPU6050_REG_SMPLRT_DIV, 0x07);
    if (status != HAL_OK) return status;

    /* Config: DLPF ~44 Hz bandwidth, 5ms delay */
    status = mpu6050_write_reg(MPU6050_REG_CONFIG, 0x03);
    if (status != HAL_OK) return status;

    /* Set accelerometer range */
    status = bsp_mpu6050_set_accel_range((bsp_mpu6050_accel_range_t)BSP_MPU6050_ACCEL_RANGE);
    if (status != HAL_OK) return status;

    /* Set gyroscope range */
    status = bsp_mpu6050_set_gyro_range((bsp_mpu6050_gyro_range_t)BSP_MPU6050_GYRO_RANGE);
    if (status != HAL_OK) return status;

    return HAL_OK;
}

hal_status_t bsp_mpu6050_deinit(void)
{
    /* Put MPU6050 to sleep */
    return mpu6050_write_reg(MPU6050_REG_PWR_MGMT_1, 0x40);
}

hal_status_t bsp_mpu6050_read_accel(bsp_mpu6050_axis_t *accel)
{
    uint8_t buf[6];
    hal_status_t status;

    status = mpu6050_read_reg(MPU6050_REG_ACCEL_XOUT_H, buf, 6);
    if (status != HAL_OK) return status;

    accel->x = (int16_t)((buf[0] << 8) | buf[1]);
    accel->y = (int16_t)((buf[2] << 8) | buf[3]);
    accel->z = (int16_t)((buf[4] << 8) | buf[5]);

    return HAL_OK;
}

hal_status_t bsp_mpu6050_read_gyro(bsp_mpu6050_axis_t *gyro)
{
    uint8_t buf[6];
    hal_status_t status;

    status = mpu6050_read_reg(MPU6050_REG_GYRO_XOUT_H, buf, 6);
    if (status != HAL_OK) return status;

    gyro->x = (int16_t)((buf[0] << 8) | buf[1]);
    gyro->y = (int16_t)((buf[2] << 8) | buf[3]);
    gyro->z = (int16_t)((buf[4] << 8) | buf[5]);

    return HAL_OK;
}

hal_status_t bsp_mpu6050_read_temp(float *temp_c)
{
    uint8_t buf[2];
    hal_status_t status;
    int16_t raw;

    status = mpu6050_read_reg(MPU6050_REG_TEMP_OUT_H, buf, 2);
    if (status != HAL_OK) return status;

    raw = (int16_t)((buf[0] << 8) | buf[1]);
    *temp_c = (float)raw / 340.0f + 36.53f;

    return HAL_OK;
}

hal_status_t bsp_mpu6050_set_accel_range(bsp_mpu6050_accel_range_t range)
{
    if (range > BSP_MPU6050_ACCEL_16G) return HAL_INVALID;

    hal_status_t status = mpu6050_write_reg(MPU6050_REG_ACCEL_CONFIG, (uint8_t)(range << 3));
    if (status == HAL_OK) {
        s_accel_range = range;
    }
    return status;
}

hal_status_t bsp_mpu6050_set_gyro_range(bsp_mpu6050_gyro_range_t range)
{
    if (range > BSP_MPU6050_GYRO_2000) return HAL_INVALID;

    hal_status_t status = mpu6050_write_reg(MPU6050_REG_GYRO_CONFIG, (uint8_t)(range << 3));
    if (status == HAL_OK) {
        s_gyro_range = range;
    }
    return status;
}

float bsp_mpu6050_get_accel_sensitivity(void)
{
    /* LSB per g for each range */
    static const float accel_sens[] = { 16384.0f, 8192.0f, 4096.0f, 2048.0f };
    if (s_accel_range > BSP_MPU6050_ACCEL_16G) return 0.0f;
    return accel_sens[s_accel_range];
}

float bsp_mpu6050_get_gyro_sensitivity(void)
{
    /* LSB per °/s for each range */
    static const float gyro_sens[] = { 131.0f, 65.5f, 32.8f, 16.4f };
    if (s_gyro_range > BSP_MPU6050_GYRO_2000) return 0.0f;
    return gyro_sens[s_gyro_range];
}
