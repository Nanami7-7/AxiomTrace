/**
 * @file    bsp_mpu6050_dmp.h
 * @brief   MPU6050 DMP驱动接口
 * @note    基于InvenSense DMP v6.12固件，实现:
 *          - DMP固件加载(写入MPU6050程序空间)
 *          - 四元数/欧拉角解算
 *          - 6轴低功耗模式
 *          - 陀螺仪自动校准
 *
 *          依赖: bsp_mpu6050(基础驱动) + bsp_soft_i2c(I2C通信)
 *          DMP输出频率: 100Hz(由采样率分频器控制)
 *
 *          使用示例:
 *          bsp_mpu6050_dmp_init();
 *          while(1) {
 *              if (bsp_mpu6050_dmp_data_ready()) {
 *                  bsp_mpu6050_dmp_data_t data;
 *                  bsp_mpu6050_dmp_read(&data);
 *                  // data.roll, data.pitch, data.yaw
 *              }
 *          }
 */
#ifndef BSP_MPU6050_DMP_H
#define BSP_MPU6050_DMP_H

#ifdef __cplusplus
extern "C" {
#endif

/* ======================== 包含 ======================== */
#include "bsp_common.h"

/* ======================== DMP寄存器/配置 ======================== */

/** DMP FIFO数据包大小(字节) */
#define DMP_FIFO_PACKET_SIZE    (28U)

/** DMP特性: 6轴低功耗四元数 */
#define DMP_FEATURE_6X_LP_QUAT  (0x0080U)
/** DMP特性: 陀螺仪自动校准 */
#define DMP_FEATURE_GYRO_CAL    (0x0004U)

/** DMP中断: 数据就绪 */
#define DMP_INT_DATA_RDY        (0x01U)
/** DMP中断: DMP就绪 */
#define DMP_INT_DMP_READY       (0x02U)

/* ======================== 类型定义 ======================== */

/**
 * @brief DMP解算输出数据结构体
 */
typedef struct {
    float q0, q1, q2, q3;              /**< 四元数 */
    float roll;                          /**< 横滚角(度, ±180) */
    float pitch;                         /**< 俯仰角(度, ±90) */
    float yaw;                           /**< 偏航角(度, ±180) */
    float accel_x_g;                     /**< 加速度X轴(g) */
    float accel_y_g;                     /**< 加速度Y轴(g) */
    float accel_z_g;                     /**< 加速度Z轴(g) */
    float gyro_x_dps;                    /**< 陀螺仪X轴(°/s) */
    float gyro_y_dps;                    /**< 陀螺仪Y轴(°/s) */
    float gyro_z_dps;                    /**< 陀螺仪Z轴(°/s) */
    float temperature_c;                 /**< 温度(℃) */
    uint32_t timestamp_ms;               /**< 时间戳(ms) */
} bsp_mpu6050_dmp_data_t;

/* ======================== 函数接口 ======================== */

/**
 * @brief  初始化DMP
 * @note   流程: 加载固件→配置特性→使能DMP→配置中断
 *         必须在bsp_mpu6050_init()之后调用
 * @retval BSP_OK           初始化成功
 * @retval BSP_ERR_NOT_INIT MPU6050未初始化
 * @retval BSP_ERR_NAK      I2C通信失败
 * @retval BSP_ERR_HW_FAULT 固件校验失败
 */
bsp_status_t bsp_mpu6050_dmp_init(void);

/**
 * @brief  读取DMP解算数据
 * @param  data  输出数据结构体指针
 * @note   从DMP FIFO读取28字节数据包，解析四元数和传感器数据
 *         自动转换为欧拉角和物理单位
 * @retval BSP_OK           读取成功
 * @retval BSP_ERR_NULL_PTR 空指针
 * @retval BSP_ERR_NOT_INIT DMP未初始化
 * @retval BSP_ERR_NAK      I2C通信失败
 * @retval BSP_ERR_BUF_EMPTY FIFO无新数据
 */
bsp_status_t bsp_mpu6050_dmp_read(bsp_mpu6050_dmp_data_t *data);

/**
 * @brief  检查DMP FIFO是否有新数据
 * @retval true  有新数据可读
 * @retval false 无新数据或DMP未初始化
 */
bool bsp_mpu6050_dmp_data_ready(void);

/**
 * @brief  检查DMP是否已初始化
 * @retval true  已初始化
 * @retval false 未初始化
 */
bool bsp_mpu6050_dmp_is_inited(void);

/**
 * @brief  重置DMP FIFO
 * @note   清空FIFO中的所有数据
 * @retval BSP_OK      重置成功
 * @retval BSP_ERR_NAK I2C通信失败
 */
bsp_status_t bsp_mpu6050_dmp_reset_fifo(void);

#ifdef __cplusplus
}
#endif

#endif /* BSP_MPU6050_DMP_H */