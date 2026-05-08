/**
 * @file    bsp_mpu6050.h
 * @brief   MPU6050六轴传感器驱动接口
 * @note    基于bsp_soft_i2c实现，支持加速度计和陀螺仪数据读取
 *          MPU6050特性:
 *          - 3轴加速度计(±2/4/8/16g可配置)
 *          - 3轴陀螺仪(±250/500/1000/2000°/s可配置)
 *          - 16位ADC输出
 *          - 数字低通滤波器(DLPF)
 *          - I2C地址: 0x68(AD0=0) 或 0x69(AD0=1)
 *
 *          使用示例:
 *          bsp_mpu6050_config_t cfg = {
 *              .accel_full_scale = 0,  // ±2g
 *              .gyro_full_scale  = 0,  // ±250°/s
 *              .dlpf_cfg         = 3,  // 44Hz带宽
 *          };
 *          bsp_mpu6050_init(&cfg);
 */
#ifndef BSP_MPU6050_H
#define BSP_MPU6050_H

#ifdef __cplusplus
extern "C" {
#endif

/* ======================== 包含 ======================== */
#include "bsp_common.h"

/* ======================== 常量定义 ======================== */

/** MPU6050 WHO_AM_I寄存器默认值 */
#define BSP_MPU6050_WHO_AM_I_VAL   (0x68U)

/* ---- MPU6050寄存器地址 ---- */

/** 电源管理寄存器1 */
#define BSP_MPU6050_REG_PWR_MGMT_1     (0x6BU)
/** 电源管理寄存器2 */
#define BSP_MPU6050_REG_PWR_MGMT_2     (0x6CU)
/** 采样率分频寄存器 */
#define BSP_MPU6050_REG_SMPLRT_DIV      (0x19U)
/** 配置寄存器 */
#define BSP_MPU6050_REG_CONFIG          (0x1AU)
/** 陀螺仪配置寄存器 */
#define BSP_MPU6050_REG_GYRO_CONFIG     (0x1BU)
/** 加速度计配置寄存器 */
#define BSP_MPU6050_REG_ACCEL_CONFIG    (0x1CU)
/** FIFO使能寄存器 */
#define BSP_MPU6050_REG_FIFO_EN         (0x23U)
/** I2C主控制寄存器 */
#define BSP_MPU6050_REG_I2C_MST_CTRL    (0x24U)
/** I2C从机控制寄存器 */
#define BSP_MPU6050_REG_I2C_SLV0_ADDR   (0x25U)
/** 设备标识寄存器 */
#define BSP_MPU6050_REG_WHO_AM_I        (0x75U)

/** 加速度计X轴高字节寄存器 */
#define BSP_MPU6050_REG_ACCEL_XOUT_H    (0x3BU)
/** 加速度计X轴低字节寄存器 */
#define BSP_MPU6050_REG_ACCEL_XOUT_L    (0x3CU)
/** 加速度计Y轴高字节寄存器 */
#define BSP_MPU6050_REG_ACCEL_YOUT_H    (0x3DU)
/** 加速度计Y轴低字节寄存器 */
#define BSP_MPU6050_REG_ACCEL_YOUT_L    (0x3EU)
/** 加速度计Z轴高字节寄存器 */
#define BSP_MPU6050_REG_ACCEL_ZOUT_H    (0x3FU)
/** 加速度计Z轴低字节寄存器 */
#define BSP_MPU6050_REG_ACCEL_ZOUT_L    (0x40U)
/** 温度高字节寄存器 */
#define BSP_MPU6050_REG_TEMP_OUT_H      (0x41U)
/** 温度低字节寄存器 */
#define BSP_MPU6050_REG_TEMP_OUT_L      (0x42U)
/** 陀螺仪X轴高字节寄存器 */
#define BSP_MPU6050_REG_GYRO_XOUT_H     (0x43U)
/** 陀螺仪X轴低字节寄存器 */
#define BSP_MPU6050_REG_GYRO_XOUT_L     (0x44U)
/** 陀螺仪Y轴高字节寄存器 */
#define BSP_MPU6050_REG_GYRO_YOUT_H     (0x45U)
/** 陀螺仪Y轴低字节寄存器 */
#define BSP_MPU6050_REG_GYRO_YOUT_L     (0x46U)
/** 陀螺仪Z轴高字节寄存器 */
#define BSP_MPU6050_REG_GYRO_ZOUT_H     (0x47U)
/** 陀螺仪Z轴低字节寄存器 */
#define BSP_MPU6050_REG_GYRO_ZOUT_L     (0x48U)

/* ---- PWR_MGMT_1位定义 ---- */

/** 设备复位位 */
#define BSP_MPU6050_PWR_MGMT_1_RESET    (0x80U)
/** 睡眠模式位 */
#define BSP_MPU6050_PWR_MGMT_1_SLEEP    (0x40U)
/** 温度传感器使能位 */
#define BSP_MPU6050_PWR_MGMT_1_TEMP_DIS (0x08U)
/** 时钟源: 内部8MHz */
#define BSP_MPU6050_PWR_MGMT_1_CLKSEL_INT (0x00U)
/** 时钟源: PLL with X axis gyroscope reference */
#define BSP_MPU6050_PWR_MGMT_1_CLKSEL_PLL_XG (0x01U)

/* ---- 加速度计量程(ACCEL_CONFIG) ---- */

#define BSP_MPU6050_ACCEL_FS_2G     (0U)   /**< ±2g  */
#define BSP_MPU6050_ACCEL_FS_4G     (1U)   /**< ±4g  */
#define BSP_MPU6050_ACCEL_FS_8G     (2U)   /**< ±8g  */
#define BSP_MPU6050_ACCEL_FS_16G    (3U)   /**< ±16g */

/** 加速度计灵敏度(LSB/g) */
#define BSP_MPU6050_ACCEL_SENS_2G   (16384U)
#define BSP_MPU6050_ACCEL_SENS_4G   (8192U)
#define BSP_MPU6050_ACCEL_SENS_8G   (4096U)
#define BSP_MPU6050_ACCEL_SENS_16G (2048U)

/* ---- 陀螺仪量程(GYRO_CONFIG) ---- */

#define BSP_MPU6050_GYRO_FS_250     (0U)   /**< ±250°/s  */
#define BSP_MPU6050_GYRO_FS_500     (1U)   /**< ±500°/s  */
#define BSP_MPU6050_GYRO_FS_1000    (2U)   /**< ±1000°/s */
#define BSP_MPU6050_GYRO_FS_2000    (3U)   /**< ±2000°/s */

/** 陀螺仪灵敏度(LSB/(°/s)) */
#define BSP_MPU6050_GYRO_SENS_250   (131U)
#define BSP_MPU6050_GYRO_SENS_500   (65U)
#define BSP_MPU6050_GYRO_SENS_1000 (33U)
#define BSP_MPU6050_GYRO_SENS_2000 (17U)

/* ---- 数字低通滤波器配置(CONFIG) ---- */

#define BSP_MPU6050_DLPF_260HZ      (0U)   /**< 260Hz带宽,延迟0ms    */
#define BSP_MPU6050_DLPF_184HZ      (1U)   /**< 184Hz带宽,延迟2ms    */
#define BSP_MPU6050_DLPF_94HZ       (2U)   /**< 94Hz带宽,延迟3ms     */
#define BSP_MPU6050_DLPF_44HZ       (3U)   /**< 44Hz带宽,延迟4.9ms   */
#define BSP_MPU6050_DLPF_21HZ       (4U)   /**< 21Hz带宽,延迟8.5ms   */
#define BSP_MPU6050_DLPF_10HZ       (5U)   /**< 10Hz带宽,延迟13.8ms  */
#define BSP_MPU6050_DLPF_5HZ        (6U)   /**< 5Hz带宽,延迟19ms     */
#define BSP_MPU6050_DLPF_3600HZ     (7U)   /**< 3600Hz,延迟0.11ms   */

/** 温度传感器灵敏度(LSB/°C) */
#define BSP_MPU6050_TEMP_SENS       (340.0f)
#define BSP_MPU6050_TEMP_OFFSET     (36.53f)

/* ======================== 类型定义 ======================== */

/**
 * @brief MPU6050配置结构体
 */
typedef struct {
    uint8_t accel_full_scale;   /**< 加速度计量程:
                                     BSP_MPU6050_ACCEL_FS_2G/4G/8G/16G */
    uint8_t gyro_full_scale;    /**< 陀螺仪量程:
                                     BSP_MPU6050_GYRO_FS_250/500/1000/2000 */
    uint8_t dlpf_cfg;           /**< 数字低通滤波器配置:
                                     BSP_MPU6050_DLPF_xxx */
    uint8_t sample_rate_div;    /**< 采样率分频值(0-255):
                                     采样率 = 1kHz / (sample_rate_div + 1) */
} bsp_mpu6050_config_t;

/**
 * @brief MPU6050原始数据结构体
 * @note  包含加速度计、陀螺仪原始值和温度值
 */
typedef struct {
    int16_t accel_x;            /**< 加速度X轴原始值 */
    int16_t accel_y;            /**< 加速度Y轴原始值 */
    int16_t accel_z;            /**< 加速度Z轴原始值 */
    int16_t gyro_x;             /**< 陀螺仪X轴原始值 */
    int16_t gyro_y;             /**< 陀螺仪Y轴原始值 */
    int16_t gyro_z;             /**< 陀螺仪Z轴原始值 */
    int16_t temperature;        /**< 温度原始值 */
} bsp_mpu6050_raw_data_t;

/**
 * @brief MPU6050物理单位数据结构体
 * @note  包含转换后的物理单位值
 */
typedef struct {
    float accel_x_g;            /**< 加速度X轴(g) */
    float accel_y_g;            /**< 加速度Y轴(g) */
    float accel_z_g;            /**< 加速度Z轴(g) */
    float gyro_x_dps;           /**< 陀螺仪X轴(°/s) */
    float gyro_y_dps;           /**< 陀螺仪Y轴(°/s) */
    float gyro_z_dps;           /**< 陀螺仪Z轴(°/s) */
    float temperature_c;        /**< 温度(℃) */
} bsp_mpu6050_phys_data_t;

/* ======================== 函数接口 ======================== */

/**
 * @brief  初始化MPU6050
 * @param  cfg  MPU6050配置参数
 * @note   初始化流程:
 *         1. 初始化软件I2C
 *         2. 唤醒MPU6050
 *         3. 选择时钟源
 *         4. 配置加速度计量程
 *         5. 配置陀螺仪量程
 *         6. 配置数字低通滤波器
 *         7. 配置采样率
 *         8. 验证WHO_AM_I寄存器
 * @retval BSP_OK              初始化成功
 * @retval BSP_ERR_INVALID_PARAM 参数无效
 * @retval BSP_ERR_NAK         设备无应答(I2C通信失败)
 * @retval BSP_ERR_HW_FAULT     WHO_AM_I校验失败
 */
bsp_status_t bsp_mpu6050_init(const bsp_mpu6050_config_t *cfg);

/**
 * @brief  读取MPU6050原始数据
 * @param  data  存放原始数据的结构体指针
 * @note   连续读取14字节: ACCEL_XOUT_H ~ GYRO_ZOUT_L
 *         数据格式为大端序(MSB在前)
 * @retval BSP_OK              读取成功
 * @retval BSP_ERR_NULL_PTR    空指针参数
 * @retval BSP_ERR_NAK         I2C通信失败
 */
bsp_status_t bsp_mpu6050_read_raw(bsp_mpu6050_raw_data_t *data);

/**
 * @brief  读取MPU6050物理单位数据
 * @param  data  存放物理数据的结构体指针
 * @note   自动将原始值转换为物理单位:
 *         - 加速度: g (重力加速度)
 *         - 陀螺仪: °/s (角度/秒)
 *         - 温度: ℃
 * @retval BSP_OK              读取成功
 * @retval BSP_ERR_NULL_PTR    空指针参数
 * @retval BSP_ERR_NAK         I2C通信失败
 */
bsp_status_t bsp_mpu6050_read_phys(bsp_mpu6050_phys_data_t *data);

/**
 * @brief  读取MPU6050设备ID
 * @param  id   存放设备ID的指针
 * @retval BSP_OK              读取成功
 * @retval BSP_ERR_NULL_PTR    空指针参数
 * @retval BSP_ERR_NAK         I2C通信失败
 */
bsp_status_t bsp_mpu6050_read_id(uint8_t *id);

/**
 * @brief  设置MPU6050睡眠模式
 * @param  enable  true=进入睡眠, false=唤醒
 * @retval BSP_OK  设置成功
 * @retval BSP_ERR_NAK I2C通信失败
 */
bsp_status_t bsp_mpu6050_set_sleep(bool enable);

/**
 * @brief  设置MPU6050采样率分频
 * @param  divider  采样率分频值(0-255)
 * @note   采样率 = 1kHz / (divider + 1)
 *         例如: divider=9 → 100Hz采样率
 * @retval BSP_OK  设置成功
 * @retval BSP_ERR_NAK I2C通信失败
 */
bsp_status_t bsp_mpu6050_set_sample_rate(uint8_t divider);

/**
 * @brief  设置MPU6050加速度计量程
 * @param  fs   量程: BSP_MPU6050_ACCEL_FS_2G/4G/8G/16G
 * @retval BSP_OK  设置成功
 * @retval BSP_ERR_INVALID_PARAM 参数无效
 * @retval BSP_ERR_NAK I2C通信失败
 */
bsp_status_t bsp_mpu6050_set_accel_full_scale(uint8_t fs);

/**
 * @brief  设置MPU6050陀螺仪量程
 * @param  fs   量程: BSP_MPU6050_GYRO_FS_250/500/1000/2000
 * @retval BSP_OK  设置成功
 * @retval BSP_ERR_INVALID_PARAM 参数无效
 * @retval BSP_ERR_NAK I2C通信失败
 */
bsp_status_t bsp_mpu6050_set_gyro_full_scale(uint8_t fs);

/**
 * @brief  设置MPU6050数字低通滤波器
 * @param  cfg  DLPF配置: BSP_MPU6050_DLPF_xxx
 * @retval BSP_OK  设置成功
 * @retval BSP_ERR_INVALID_PARAM 参数无效(>7)
 * @retval BSP_ERR_NAK I2C通信失败
 */
bsp_status_t bsp_mpu6050_set_dlpf(uint8_t cfg);

/**
 * @brief  检查MPU6050是否已初始化
 * @retval true  已初始化
 * @retval false 未初始化
 */
bool bsp_mpu6050_is_inited(void);

#ifdef __cplusplus
}
#endif

#endif /* BSP_MPU6050_H */
