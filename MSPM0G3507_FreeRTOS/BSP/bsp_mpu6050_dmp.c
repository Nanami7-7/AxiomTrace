/**
 * @file    bsp_mpu6050_dmp.c
 * @brief   MPU6050 DMP驱动实现
 * @note    基于InvenSense eMPL开源库简化实现
 *          DMP固件通过I2C写入MPU6050的程序空间(bank写入)
 *          FIFO数据包格式(28字节):
 *          [0-3]   四元数Q0(大端序, 16位定点)
 *          [4-7]   四元数Q1
 *          [8-11]  四元数Q2
 *          [12-15] 四元数Q3
 *          [16-21] 陀螺仪XYZ(原始值)
 *          [22-27] 加速度计XYZ(原始值)
 */
#include "bsp_mpu6050_dmp.h"
#include "bsp_mpu6050.h"
#include "bsp_soft_i2c.h"
#include "osal_api.h"
#include "project_config.h"
#include "dmp_firmware.h"
#include <math.h>

/* ======================== 私有宏 ======================== */

/** DMP初始化延时(ms) */
#define DMP_INIT_DELAY_MS       (50U)
/** DMP使能后延时(ms) */
#define DMP_ENABLE_DELAY_MS     (50U)

/** MPU6050寄存器: DMP配置 */
#define REG_BANK_SEL            (0x6DU)
#define REG_MEM_START_ADDR      (0x6EU)
#define REG_MEM_R_W             (0x6FU)
#define REG_PRGM_START_H        (0x70U)
#define REG_PRGM_START_L        (0x71U)
#define REG_INT_ENABLE          (0x38U)
#define REG_FIFO_COUNT_H        (0x72U)
#define REG_FIFO_COUNT_L        (0x73U)
#define REG_FIFO_R_W            (0x74U)
#define REG_USER_CTRL           (0x6AU)
#define REG_SIGNAL_PATH_RESET   (0x68U)

/** USER_CTRL位定义 */
#define USER_CTRL_DMP_EN        (0x80U)
#define USER_CTRL_FIFO_EN       (0x40U)
#define USER_CTRL_DMP_RESET     (0x08U)
#define USER_CTRL_FIFO_RESET    (0x04U)

/** INT_ENABLE位定义 */
#define INT_ENABLE_DMP_INT      (0x02U)

/** DMP特性配置地址(固件内偏移) */
#define DMP_CFG_1_ADDR          (2910U)
#define DMP_CFG_2_ADDR          (2902U)

/* ======================== 私有变量 ======================== */

/** DMP初始化标志 */
static bool s_dmp_is_inited = false;

/** 当前时间戳(ms) */
static uint32_t s_timestamp_ms = 0;

/* ======================== 私有函数 ======================== */

/**
 * @brief  向MPU6050内存写入数据
 * @param  addr  内存起始地址(16位)
 * @param  data  数据缓冲区
 * @param  len   数据长度
 * @retval BSP_OK      成功
 * @retval BSP_ERR_NAK I2C通信失败
 */
static bsp_status_t dmp_write_mem(uint16_t addr, const uint8_t *data,
                                   uint16_t len)
{
    bsp_status_t ret;
    uint8_t addr_buf[2];

    /* 设置bank选择和内存起始地址 */
    addr_buf[0] = (uint8_t)(addr >> 8);    /* bank */
    addr_buf[1] = (uint8_t)(addr & 0xFF);  /* 起始地址 */

    ret = bsp_soft_i2c_write_reg(PRJ_MPU6050_I2C_ADDR,
                                  REG_BANK_SEL, addr_buf[0]);
    if (ret != BSP_OK) { return BSP_ERR_NAK; }

    ret = bsp_soft_i2c_write_reg(PRJ_MPU6050_I2C_ADDR,
                                  REG_MEM_START_ADDR, addr_buf[1]);
    if (ret != BSP_OK) { return BSP_ERR_NAK; }

    ret = bsp_soft_i2c_write_reg_buf(PRJ_MPU6050_I2C_ADDR,
                                      REG_MEM_R_W, data, len);
    if (ret != BSP_OK) { return BSP_ERR_NAK; }

    return BSP_OK;
}

/**
 * @brief  从MPU6050内存读取数据
 * @param  addr  内存起始地址(16位)
 * @param  data  读取缓冲区
 * @param  len   读取长度
 * @retval BSP_OK      成功
 * @retval BSP_ERR_NAK I2C通信失败
 */
static bsp_status_t dmp_read_mem(uint16_t addr, uint8_t *data,
                                  uint16_t len)
{
    bsp_status_t ret;
    uint8_t addr_buf[2];

    addr_buf[0] = (uint8_t)(addr >> 8);
    addr_buf[1] = (uint8_t)(addr & 0xFF);

    ret = bsp_soft_i2c_write_reg(PRJ_MPU6050_I2C_ADDR,
                                  REG_BANK_SEL, addr_buf[0]);
    if (ret != BSP_OK) { return BSP_ERR_NAK; }

    ret = bsp_soft_i2c_write_reg(PRJ_MPU6050_I2C_ADDR,
                                  REG_MEM_START_ADDR, addr_buf[1]);
    if (ret != BSP_OK) { return BSP_ERR_NAK; }

    ret = bsp_soft_i2c_read_reg_buf(PRJ_MPU6050_I2C_ADDR,
                                     REG_MEM_R_W, data, len);
    if (ret != BSP_OK) { return BSP_ERR_NAK; }

    return BSP_OK;
}

/**
 * @brief  加载DMP固件到MPU6050
 * @retval BSP_OK           成功
 * @retval BSP_ERR_NAK      I2C通信失败
 * @retval BSP_ERR_HW_FAULT 固件校验失败
 */
static bsp_status_t dmp_load_firmware(void)
{
    bsp_status_t ret;
    uint16_t written = 0;
    uint16_t chunk_size;
    uint16_t this_len;
    uint8_t verify_buf[DMP_FIRMWARE_BANK_SIZE];

    /* 按bank写入固件 */
    while (written < DMP_FIRMWARE_SIZE) {
        this_len = DMP_FIRMWARE_SIZE - written;
        if (this_len > DMP_FIRMWARE_BANK_SIZE) {
            this_len = DMP_FIRMWARE_BANK_SIZE;
        }

        /* 写入当前chunk */
        ret = dmp_write_mem(written, &dmp_firmware[written], this_len);
        if (ret != BSP_OK) {
            return BSP_ERR_NAK;
        }

        /* 读回验证 */
        chunk_size = this_len;
        if (chunk_size > 16) {
            chunk_size = 16;  /* 仅验证前16字节以节省时间 */
        }
        ret = dmp_read_mem(written, verify_buf, chunk_size);
        if (ret != BSP_OK) {
            return BSP_ERR_NAK;
        }

        /* 校验 */
        for (uint16_t i = 0; i < chunk_size; i++) {
            if (verify_buf[i] != dmp_firmware[written + i]) {
                return BSP_ERR_HW_FAULT;
            }
        }

        written += this_len;
    }

    return BSP_OK;
}

/**
 * @brief  设置DMP特性
 * @param  features  特性位掩码
 * @retval BSP_OK      成功
 * @retval BSP_ERR_NAK I2C通信失败
 */
static bsp_status_t dmp_set_features(uint16_t features)
{
    bsp_status_t ret;
    uint8_t buf[10];

    /* 写入特性配置到DMP配置地址 */
    buf[0] = (uint8_t)(features >> 8);
    buf[1] = (uint8_t)(features & 0xFF);

    ret = dmp_write_mem(DMP_CFG_1_ADDR, buf, 2);
    if (ret != BSP_OK) {
        return BSP_ERR_NAK;
    }

    return BSP_OK;
}

/**
 * @brief  设置DMP FIFO输出速率
 * @param  rate  分频值(0=最大速率, 1=1/2, ...)
 * @retval BSP_OK      成功
 * @retval BSP_ERR_NAK I2C通信失败
 */
static bsp_status_t dmp_set_fifo_rate(uint16_t rate)
{
    bsp_status_t ret;
    uint8_t regs[2];

    regs[0] = (uint8_t)(rate >> 8);
    regs[1] = (uint8_t)(rate & 0xFF);

    ret = dmp_write_mem(DMP_CFG_2_ADDR, regs, 2);
    if (ret != BSP_OK) {
        return BSP_ERR_NAK;
    }

    return BSP_OK;
}

/**
 * @brief  使能/禁用DMP
 * @param  enable  true=使能, false=禁用
 * @retval BSP_OK      成功
 * @retval BSP_ERR_NAK I2C通信失败
 */
static bsp_status_t dmp_enable(bool enable)
{
    bsp_status_t ret;
    uint8_t reg_val;

    ret = bsp_soft_i2c_read_reg(PRJ_MPU6050_I2C_ADDR,
                                 REG_USER_CTRL, &reg_val);
    if (ret != BSP_OK) {
        return BSP_ERR_NAK;
    }

    if (enable) {
        reg_val |= USER_CTRL_DMP_EN | USER_CTRL_FIFO_EN;
    } else {
        reg_val &= ~(USER_CTRL_DMP_EN | USER_CTRL_FIFO_EN);
    }

    ret = bsp_soft_i2c_write_reg(PRJ_MPU6050_I2C_ADDR,
                                  REG_USER_CTRL, reg_val);
    if (ret != BSP_OK) {
        return BSP_ERR_NAK;
    }

    return BSP_OK;
}

/**
 * @brief  从大端序字节转换为有符号16位整数
 */
static inline int16_t dmp_bytes_to_int16(uint8_t high, uint8_t low)
{
    return (int16_t)((uint16_t)high << 8 | (uint16_t)low);
}

/**
 * @brief  四元数转欧拉角
 * @param  q     四元数数组[q0,q1,q2,q3]
 * @param  roll  输出横滚角(度)
 * @param  pitch 输出俯仰角(度)
 * @param  yaw   输出偏航角(度)
 */
static void dmp_quaternion_to_euler(const float q[4],
                                     float *roll, float *pitch, float *yaw)
{
    float sinr_cosp, cosr_cosp;
    float sinp;
    float siny_cosp, cosy_cosp;

    /* Roll (x-axis rotation) */
    sinr_cosp = 2.0f * (q[0] * q[1] + q[2] * q[3]);
    cosr_cosp = 1.0f - 2.0f * (q[1] * q[1] + q[2] * q[2]);
    *roll = atan2f(sinr_cosp, cosr_cosp) * 57.2957795f;  /* 180/PI */

    /* Pitch (y-axis rotation) */
    sinp = 2.0f * (q[0] * q[2] - q[3] * q[1]);
    if (sinp >= 1.0f) {
        *pitch = 90.0f;
    } else if (sinp <= -1.0f) {
        *pitch = -90.0f;
    } else {
        *pitch = asinf(sinp) * 57.2957795f;
    }

    /* Yaw (z-axis rotation) */
    siny_cosp = 2.0f * (q[0] * q[3] + q[1] * q[2]);
    cosy_cosp = 1.0f - 2.0f * (q[2] * q[2] + q[3] * q[3]);
    *yaw = atan2f(siny_cosp, cosy_cosp) * 57.2957795f;
}

/* ======================== 公共函数实现 ======================== */

bsp_status_t bsp_mpu6050_dmp_init(void)
{
    bsp_status_t ret;
    uint8_t reg_val;

    /* 检查MPU6050是否已初始化 */
    if (!bsp_mpu6050_is_inited()) {
        return BSP_ERR_NOT_INIT;
    }

    /* 重置DMP和FIFO */
    reg_val = USER_CTRL_DMP_RESET | USER_CTRL_FIFO_RESET;
    ret = bsp_soft_i2c_write_reg(PRJ_MPU6050_I2C_ADDR,
                                  REG_USER_CTRL, reg_val);
    if (ret != BSP_OK) { return BSP_ERR_NAK; }

    osal_delay_ms(DMP_INIT_DELAY_MS);

    /* 加载DMP固件 */
    ret = dmp_load_firmware();
    if (ret != BSP_OK) {
        return ret;
    }

    /* 配置DMP特性: 6轴低功耗四元数 + 陀螺仪自动校准 */
    ret = dmp_set_features(DMP_FEATURE_6X_LP_QUAT | DMP_FEATURE_GYRO_CAL);
    if (ret != BSP_OK) { return BSP_ERR_NAK; }

    /* 设置FIFO输出速率分频(0=每个采样点都输出) */
    ret = dmp_set_fifo_rate(0);
    if (ret != BSP_OK) { return BSP_ERR_NAK; }

    /* 配置DMP中断: 使能DMP_INT */
    ret = bsp_soft_i2c_write_reg(PRJ_MPU6050_I2C_ADDR,
                                  REG_INT_ENABLE, INT_ENABLE_DMP_INT);
    if (ret != BSP_OK) { return BSP_ERR_NAK; }

    /* 使能DMP和FIFO */
    ret = dmp_enable(true);
    if (ret != BSP_OK) { return BSP_ERR_NAK; }

    osal_delay_ms(DMP_ENABLE_DELAY_MS);

    /* 重置FIFO以获取干净的第一帧数据 */
    ret = bsp_mpu6050_dmp_reset_fifo();
    if (ret != BSP_OK) { return BSP_ERR_NAK; }

    s_dmp_is_inited = true;
    s_timestamp_ms = 0;

    return BSP_OK;
}

bsp_status_t bsp_mpu6050_dmp_read(bsp_mpu6050_dmp_data_t *data)
{
    bsp_status_t ret;
    uint8_t buf[DMP_FIFO_PACKET_SIZE];
    uint8_t fifo_count_buf[2];
    uint16_t fifo_count;
    int32_t quat[4];
    float q[4];

    /* 参数校验 */
    if (data == NULL) {
        return BSP_ERR_NULL_PTR;
    }

    if (!s_dmp_is_inited) {
        return BSP_ERR_NOT_INIT;
    }

    /* 读取FIFO计数 */
    ret = bsp_soft_i2c_read_reg(PRJ_MPU6050_I2C_ADDR,
                                 REG_FIFO_COUNT_H, &fifo_count_buf[0]);
    if (ret != BSP_OK) { return BSP_ERR_NAK; }

    ret = bsp_soft_i2c_read_reg(PRJ_MPU6050_I2C_ADDR,
                                 REG_FIFO_COUNT_L, &fifo_count_buf[1]);
    if (ret != BSP_OK) { return BSP_ERR_NAK; }

    fifo_count = (uint16_t)((uint16_t)fifo_count_buf[0] << 8 |
                             fifo_count_buf[1]);

    /* 检查FIFO是否有足够数据 */
    if (fifo_count < DMP_FIFO_PACKET_SIZE) {
        return BSP_ERR_BUF_EMPTY;
    }

    /* 如果FIFO中有多个包，跳过旧数据只读最新的 */
    while (fifo_count > DMP_FIFO_PACKET_SIZE) {
        /* 读取并丢弃一个包 */
        uint8_t dummy[DMP_FIFO_PACKET_SIZE];
        ret = bsp_soft_i2c_read_reg_buf(PRJ_MPU6050_I2C_ADDR,
                                         REG_FIFO_R_W, dummy,
                                         DMP_FIFO_PACKET_SIZE);
        if (ret != BSP_OK) { return BSP_ERR_NAK; }
        fifo_count -= DMP_FIFO_PACKET_SIZE;
    }

    /* 读取最新的DMP数据包(28字节) */
    ret = bsp_soft_i2c_read_reg_buf(PRJ_MPU6050_I2C_ADDR,
                                     REG_FIFO_R_W, buf,
                                     DMP_FIFO_PACKET_SIZE);
    if (ret != BSP_OK) { return BSP_ERR_NAK; }

    /* 解析四元数(DMP输出为Q30定点格式, 需除以2^30转换为浮点) */
    quat[0] = ((int32_t)buf[0] << 24) | ((int32_t)buf[1] << 16) |
              ((int32_t)buf[2] << 8)  | buf[3];
    quat[1] = ((int32_t)buf[4] << 24) | ((int32_t)buf[5] << 16) |
              ((int32_t)buf[6] << 8)  | buf[7];
    quat[2] = ((int32_t)buf[8] << 24) | ((int32_t)buf[9] << 16) |
              ((int32_t)buf[10] << 8) | buf[11];
    quat[3] = ((int32_t)buf[12] << 24) | ((int32_t)buf[13] << 16) |
              ((int32_t)buf[14] << 8) | buf[15];

    /* Q30定点转浮点 */
    q[0] = (float)quat[0] / 1073741824.0f;  /* 2^30 */
    q[1] = (float)quat[1] / 1073741824.0f;
    q[2] = (float)quat[2] / 1073741824.0f;
    q[3] = (float)quat[3] / 1073741824.0f;

    /* 保存四元数 */
    data->q0 = q[0];
    data->q1 = q[1];
    data->q2 = q[2];
    data->q3 = q[3];

    /* 四元数转欧拉角 */
    dmp_quaternion_to_euler(q, &data->roll, &data->pitch, &data->yaw);

    /* 解析陀螺仪原始值(buf[16-21]) */
    int16_t gyro_raw_x = dmp_bytes_to_int16(buf[16], buf[17]);
    int16_t gyro_raw_y = dmp_bytes_to_int16(buf[18], buf[19]);
    int16_t gyro_raw_z = dmp_bytes_to_int16(buf[20], buf[21]);

    /* 解析加速度计原始值(buf[22-27]) */
    int16_t accel_raw_x = dmp_bytes_to_int16(buf[22], buf[23]);
    int16_t accel_raw_y = dmp_bytes_to_int16(buf[24], buf[25]);
    int16_t accel_raw_z = dmp_bytes_to_int16(buf[26], buf[27]);

    /* 根据当前量程转换为物理单位(使用project_config.h配置) */
    /* 加速度灵敏度: ±2g=16384, ±4g=8192, ±8g=4096, ±16g=2048 */
    float accel_sens;
    switch (PRJ_MPU6050_ACCEL_FS) {
        case 0: accel_sens = 16384.0f; break;
        case 1: accel_sens = 8192.0f;  break;
        case 2: accel_sens = 4096.0f;  break;
        case 3: accel_sens = 2048.0f;  break;
        default: accel_sens = 16384.0f; break;
    }
    data->accel_x_g = (float)accel_raw_x / accel_sens;
    data->accel_y_g = (float)accel_raw_y / accel_sens;
    data->accel_z_g = (float)accel_raw_z / accel_sens;

    /* 陀螺仪灵敏度: ±250=131, ±500=65, ±1000=33, ±2000=17 */
    float gyro_sens;
    switch (PRJ_MPU6050_GYRO_FS) {
        case 0: gyro_sens = 131.0f; break;
        case 1: gyro_sens = 65.0f;  break;
        case 2: gyro_sens = 33.0f;  break;
        case 3: gyro_sens = 17.0f;  break;
        default: gyro_sens = 131.0f; break;
    }
    data->gyro_x_dps = (float)gyro_raw_x / gyro_sens;
    data->gyro_y_dps = (float)gyro_raw_y / gyro_sens;
    data->gyro_z_dps = (float)gyro_raw_z / gyro_sens;

    /* 温度(使用基础驱动的公式) */
    uint8_t temp_buf[2];
    ret = bsp_soft_i2c_read_reg(PRJ_MPU6050_I2C_ADDR,
                                 BSP_MPU6050_REG_TEMP_OUT_H, &temp_buf[0]);
    if (ret != BSP_OK) { return BSP_ERR_NAK; }
    ret = bsp_soft_i2c_read_reg(PRJ_MPU6050_I2C_ADDR,
                                 BSP_MPU6050_REG_TEMP_OUT_L, &temp_buf[1]);
    if (ret != BSP_OK) { return BSP_ERR_NAK; }
    int16_t temp_raw = dmp_bytes_to_int16(temp_buf[0], temp_buf[1]);
    data->temperature_c = (float)temp_raw / 340.0f + 36.53f;

    /* 时间戳递增(基于调用周期) */
    s_timestamp_ms += 10;  /* 100Hz = 10ms */
    data->timestamp_ms = s_timestamp_ms;

    return BSP_OK;
}

bool bsp_mpu6050_dmp_data_ready(void)
{
    bsp_status_t ret;
    uint8_t int_status;

    if (!s_dmp_is_inited) {
        return false;
    }

    /* 读取INT_STATUS寄存器检查DMP_INT */
    ret = bsp_soft_i2c_read_reg(PRJ_MPU6050_I2C_ADDR,
                                 0x3A, &int_status);
    if (ret != BSP_OK) {
        return false;
    }

    return (int_status & DMP_INT_DMP_READY) != 0;
}

bool bsp_mpu6050_dmp_is_inited(void)
{
    return s_dmp_is_inited;
}

bsp_status_t bsp_mpu6050_dmp_reset_fifo(void)
{
    bsp_status_t ret;
    uint8_t reg_val;

    ret = bsp_soft_i2c_read_reg(PRJ_MPU6050_I2C_ADDR,
                                 REG_USER_CTRL, &reg_val);
    if (ret != BSP_OK) {
        return BSP_ERR_NAK;
    }

    /* 触发FIFO_RESET位(bit2) */
    reg_val |= USER_CTRL_FIFO_RESET;
    ret = bsp_soft_i2c_write_reg(PRJ_MPU6050_I2C_ADDR,
                                  REG_USER_CTRL, reg_val);
    if (ret != BSP_OK) {
        return BSP_ERR_NAK;
    }

    osal_delay_ms(5);

    return BSP_OK;
}