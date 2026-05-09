/**
 * @file    bsp_mpu6050.c
 * @brief   MPU6050六轴传感器驱动实现
 * @note    基于bsp_soft_i2c实现I2C通信
 *          数据读取流程:
 *          1. 发送起始条件
 *          2. 发送设备地址+写标志
 *          3. 发送寄存器起始地址(ACCEL_XOUT_H)
 *          4. 发送重复起始条件
 *          5. 发送设备地址+读标志
 *          6. 连续读取14字节数据
 *          7. 发送停止条件
 *
 *          数据格式为大端序(MSB在前),需转换为小端序
 */
#include "bsp_mpu6050.h"
#include "bsp_soft_i2c.h"
#include "osal_api.h"
#include "project_config.h"

/* ======================== 私有宏 ======================== */

/** 初始化延时(ms) */
#define MPU6050_INIT_DELAY_MS      (100U)

/** 读取数据寄存器起始地址 */
#define MPU6050_DATA_REG_START     (BSP_MPU6050_REG_ACCEL_XOUT_H)
/** 数据寄存器数量(加速度6+温度2+陀螺仪6 = 14字节) */
#define MPU6050_DATA_REG_COUNT     (14U)

/* ======================== 私有变量 ======================== */

/** 初始化标志 */
static bool s_is_inited = false;

/** 当前灵敏度系数(用于物理单位转换) */
static float s_accel_sensitivity = (float)BSP_MPU6050_ACCEL_SENS_2G;
static float s_gyro_sensitivity  = (float)BSP_MPU6050_GYRO_SENS_250;

/** 当前配置(使用project_config.h中的默认值) */
static bsp_mpu6050_config_t s_current_config = PRJ_MPU6050_CONFIG;

/* ======================== 私有函数 ======================== */

/**
 * @brief  将大端序字节转换为小端序16位整数
 * @param  high 高字节
 * @param  low  低字节
 * @retval 转换后的16位整数
 */
static inline int16_t bytes_to_int16(uint8_t high, uint8_t low)
{
    return (int16_t)((uint16_t)high << 8 | (uint16_t)low);
}

/**
 * @brief  获取加速度灵敏度
 * @param  fs  量程选择(0-3)
 * @retval 灵敏度值(LSB/g)
 */
static uint16_t get_accel_sensitivity(uint8_t fs)
{
    switch (fs) {
        case BSP_MPU6050_ACCEL_FS_2G:  return BSP_MPU6050_ACCEL_SENS_2G;
        case BSP_MPU6050_ACCEL_FS_4G:  return BSP_MPU6050_ACCEL_SENS_4G;
        case BSP_MPU6050_ACCEL_FS_8G:  return BSP_MPU6050_ACCEL_SENS_8G;
        case BSP_MPU6050_ACCEL_FS_16G: return BSP_MPU6050_ACCEL_SENS_16G;
        default:                       return BSP_MPU6050_ACCEL_SENS_2G;
    }
}

/**
 * @brief  获取陀螺仪灵敏度
 * @param  fs  量程选择(0-3)
 * @retval 灵敏度值(LSB/(°/s))
 */
static uint16_t get_gyro_sensitivity(uint8_t fs)
{
    switch (fs) {
        case BSP_MPU6050_GYRO_FS_250:  return BSP_MPU6050_GYRO_SENS_250;
        case BSP_MPU6050_GYRO_FS_500:  return BSP_MPU6050_GYRO_SENS_500;
        case BSP_MPU6050_GYRO_FS_1000: return BSP_MPU6050_GYRO_SENS_1000;
        case BSP_MPU6050_GYRO_FS_2000: return BSP_MPU6050_GYRO_SENS_2000;
        default:                        return BSP_MPU6050_GYRO_SENS_250;
    }
}

/**
 * @brief  写入单个寄存器
 * @param  reg   寄存器地址
 * @param  value 写入值
 * @retval BSP_OK       成功
 * @retval BSP_ERR_NAK  从机无应答
 */
static bsp_status_t write_reg(uint8_t reg, uint8_t value)
{
    return bsp_soft_i2c_write_reg(PRJ_MPU6050_I2C_ADDR, reg, value);
}

/**
 * @brief  读取单个寄存器
 * @param  reg   寄存器地址
 * @param  value 读取值存放地址
 * @retval BSP_OK       成功
 * @retval BSP_ERR_NAK  从机无应答
 */
static bsp_status_t read_reg(uint8_t reg, uint8_t *value)
{
    return bsp_soft_i2c_read_reg(PRJ_MPU6050_I2C_ADDR, reg, value);
}

/* ======================== 公共函数实现 ======================== */

bsp_status_t bsp_mpu6050_init(const bsp_mpu6050_config_t *cfg)
{
    bsp_status_t ret;
    uint8_t id;
    uint8_t reg_val;

    /* 参数校验 */
    BSP_ASSERT(cfg != NULL);

    /* 重复初始化保护: 若已初始化则先睡眠再重新配置 */
    if (s_is_inited) {
        (void)bsp_mpu6050_set_sleep(true);
        s_is_inited = false;
    }

    /* 初始化软件I2C */
    ret = bsp_soft_i2c_init();
    if (ret != BSP_OK) {
        return ret;
    }

    /* 保存配置 */
    s_current_config.accel_full_scale = cfg->accel_full_scale;
    s_current_config.gyro_full_scale  = cfg->gyro_full_scale;
    s_current_config.dlpf_cfg         = cfg->dlpf_cfg;
    s_current_config.sample_rate_div  = cfg->sample_rate_div;

    /* 更新灵敏度 */
    s_accel_sensitivity = (float)get_accel_sensitivity(cfg->accel_full_scale);
    s_gyro_sensitivity  = (float)get_gyro_sensitivity(cfg->gyro_full_scale);

    /* 复位MPU6050(处理异常初始状态) */
    ret = write_reg(BSP_MPU6050_REG_PWR_MGMT_1,
                    BSP_MPU6050_PWR_MGMT_1_RESET);
    if (ret != BSP_OK) {
        s_is_inited = false;
        return BSP_ERR_NAK;
    }
    osal_delay_ms(MPU6050_INIT_DELAY_MS);

    /* 唤醒MPU6050: 清除睡眠位, 选择内部时钟源 */
    ret = write_reg(BSP_MPU6050_REG_PWR_MGMT_1,
                    BSP_MPU6050_PWR_MGMT_1_CLKSEL_INT);
    if (ret != BSP_OK) {
        s_is_inited = false;
        return BSP_ERR_NAK;
    }

    /* 等待唤醒稳定 */
    osal_delay_ms(MPU6050_INIT_DELAY_MS);

    /* 验证WHO_AM_I寄存器(提前到配置之前，避免无效配置) */
    ret = read_reg(BSP_MPU6050_REG_WHO_AM_I, &id);
    if (ret != BSP_OK) {
        s_is_inited = false;
        return BSP_ERR_NAK;
    }
    if (id != BSP_MPU6050_WHO_AM_I_VAL) {
        s_is_inited = false;
        return BSP_ERR_HW_FAULT;
    }

    /* 配置加速度计量程 */
    reg_val = (cfg->accel_full_scale & 0x03U) << 3;
    ret = write_reg(BSP_MPU6050_REG_ACCEL_CONFIG, reg_val);
    if (ret != BSP_OK) {
        s_is_inited = false;
        return BSP_ERR_NAK;
    }

    /* 配置陀螺仪量程 */
    reg_val = (cfg->gyro_full_scale & 0x03U) << 3;
    ret = write_reg(BSP_MPU6050_REG_GYRO_CONFIG, reg_val);
    if (ret != BSP_OK) {
        s_is_inited = false;
        return BSP_ERR_NAK;
    }

    /* 配置数字低通滤波器 */
    reg_val = cfg->dlpf_cfg & 0x07U;
    ret = write_reg(BSP_MPU6050_REG_CONFIG, reg_val);
    if (ret != BSP_OK) {
        s_is_inited = false;
        return BSP_ERR_NAK;
    }

    /* 配置采样率分频(使用配置参数) */
    ret = bsp_mpu6050_set_sample_rate(cfg->sample_rate_div);
    if (ret != BSP_OK) {
        s_is_inited = false;
        return ret;
    }

    s_is_inited = true;
    return BSP_OK;
}

bsp_status_t bsp_mpu6050_read_raw(bsp_mpu6050_raw_data_t *data)
{
    bsp_status_t ret;
    uint8_t buf[MPU6050_DATA_REG_COUNT];

    /* 参数校验 */
    if (data == NULL) {
        return BSP_ERR_NULL_PTR;
    }

    if (!s_is_inited) {
        return BSP_ERR_NOT_INIT;
    }

    /* 连续读取14字节数据 */
    ret = bsp_soft_i2c_read_reg_buf(PRJ_MPU6050_I2C_ADDR,
                                    MPU6050_DATA_REG_START,
                                    buf,
                                    MPU6050_DATA_REG_COUNT);
    if (ret != BSP_OK) {
        return BSP_ERR_NAK;
    }

    /* 解析数据(大端序转小端序) */
    data->accel_x     = bytes_to_int16(buf[0],  buf[1]);
    data->accel_y     = bytes_to_int16(buf[2],  buf[3]);
    data->accel_z     = bytes_to_int16(buf[4],  buf[5]);
    data->temperature = bytes_to_int16(buf[6],  buf[7]);
    data->gyro_x      = bytes_to_int16(buf[8],  buf[9]);
    data->gyro_y      = bytes_to_int16(buf[10], buf[11]);
    data->gyro_z      = bytes_to_int16(buf[12], buf[13]);

    return BSP_OK;
}

bsp_status_t bsp_mpu6050_read_phys(bsp_mpu6050_phys_data_t *data)
{
    bsp_status_t ret;
    bsp_mpu6050_raw_data_t raw;

    /* 参数校验 */
    if (data == NULL) {
        return BSP_ERR_NULL_PTR;
    }

    /* 读取原始数据 */
    ret = bsp_mpu6050_read_raw(&raw);
    if (ret != BSP_OK) {
        return ret;
    }

    /* 转换为物理单位 */
    data->accel_x_g      = (float)raw.accel_x / s_accel_sensitivity;
    data->accel_y_g      = (float)raw.accel_y / s_accel_sensitivity;
    data->accel_z_g      = (float)raw.accel_z / s_accel_sensitivity;
    data->gyro_x_dps     = (float)raw.gyro_x  / s_gyro_sensitivity;
    data->gyro_y_dps     = (float)raw.gyro_y  / s_gyro_sensitivity;
    data->gyro_z_dps     = (float)raw.gyro_z  / s_gyro_sensitivity;
    data->temperature_c  = (float)raw.temperature / BSP_MPU6050_TEMP_SENS
                            + BSP_MPU6050_TEMP_OFFSET;

    return BSP_OK;
}

bsp_status_t bsp_mpu6050_read_id(uint8_t *id)
{
    /* 参数校验 */
    if (id == NULL) {
        return BSP_ERR_NULL_PTR;
    }

    return read_reg(BSP_MPU6050_REG_WHO_AM_I, id);
}

bsp_status_t bsp_mpu6050_set_sleep(bool enable)
{
    bsp_status_t ret;
    uint8_t reg_val;

    ret = read_reg(BSP_MPU6050_REG_PWR_MGMT_1, &reg_val);
    if (ret != BSP_OK) {
        return BSP_ERR_NAK;
    }

    if (enable) {
        reg_val |= BSP_MPU6050_PWR_MGMT_1_SLEEP;
    } else {
        reg_val &= ~BSP_MPU6050_PWR_MGMT_1_SLEEP;
    }

    return write_reg(BSP_MPU6050_REG_PWR_MGMT_1, reg_val);
}

bsp_status_t bsp_mpu6050_set_sample_rate(uint8_t divider)
{
    return write_reg(BSP_MPU6050_REG_SMPLRT_DIV, divider);
}

bsp_status_t bsp_mpu6050_set_accel_full_scale(uint8_t fs)
{
    bsp_status_t ret;
    uint8_t reg_val;

    /* 参数校验 */
    if (fs > BSP_MPU6050_ACCEL_FS_16G) {
        return BSP_ERR_INVALID_PARAM;
    }

    reg_val = (fs & 0x03U) << 3;
    ret = write_reg(BSP_MPU6050_REG_ACCEL_CONFIG, reg_val);
    if (ret != BSP_OK) {
        return BSP_ERR_NAK;
    }

    /* 更新灵敏度 */
    s_accel_sensitivity = (float)get_accel_sensitivity(fs);
    s_current_config.accel_full_scale = fs;

    return BSP_OK;
}

bsp_status_t bsp_mpu6050_set_gyro_full_scale(uint8_t fs)
{
    bsp_status_t ret;
    uint8_t reg_val;

    /* 参数校验 */
    if (fs > BSP_MPU6050_GYRO_FS_2000) {
        return BSP_ERR_INVALID_PARAM;
    }

    reg_val = (fs & 0x03U) << 3;
    ret = write_reg(BSP_MPU6050_REG_GYRO_CONFIG, reg_val);
    if (ret != BSP_OK) {
        return BSP_ERR_NAK;
    }

    /* 更新灵敏度 */
    s_gyro_sensitivity = (float)get_gyro_sensitivity(fs);
    s_current_config.gyro_full_scale = fs;

    return BSP_OK;
}

bsp_status_t bsp_mpu6050_set_dlpf(uint8_t cfg)
{
    bsp_status_t ret;
    uint8_t reg_val;

    /* 参数校验 */
    if (cfg > BSP_MPU6050_DLPF_3600HZ) {
        return BSP_ERR_INVALID_PARAM;
    }

    reg_val = cfg & 0x07U;
    ret = write_reg(BSP_MPU6050_REG_CONFIG, reg_val);
    if (ret != BSP_OK) {
        return BSP_ERR_NAK;
    }

    /* 更新当前配置 */
    s_current_config.dlpf_cfg = cfg;

    return BSP_OK;
}

bool bsp_mpu6050_is_inited(void)
{
    return s_is_inited;
}
