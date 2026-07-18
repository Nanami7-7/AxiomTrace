/**
 * @file    soft_i2c_bridge.h
 * @brief   LSM6DSR 软件 I2C 桥接层 — 基于 GPIO 位模拟
 *
 * @note    本文件已停用: 当前工程改用 SPI 桥接(spi_bridge.c, lsm6dsr_io_spi)
 *          全部内容用 #if 0 包裹, 不参与编译, 仅保留作参考
 *          恢复方法: 删除外层 #if 0/#endif 或将本文件加入工程编译
 *
 * 历史背景:
 *   效仿 MPU6050 的软件 I2C 实现，使用 PA0(SCL)/PA1(SDA) 引脚。
 *   将 LSM6DSR 驱动层的 lsm6dsr_io_t 接口桥接到软件 I2C 位模拟。
 *
 *   硬件配置：
 *     - I2C 引脚：SCL=PA0, SDA=PA1（复用 MPU6050 引脚）
 *     - I2C 地址：0x6A (SDO/SA0 接 GND)
 *     - 时钟速率：约 100kHz（软件模拟）
 *
 *   注意事项：
 *     - 使用 ti_msp_dl_config.h 中定义的 IIC_PORT/IIC_SCL_PIN/IIC_SDA_PIN
 *     - LSM6DSR I2C 地址为 7-bit 左移 1 位格式 (0xD4)
 */

#ifndef SOFT_I2C_BRIDGE_H
#define SOFT_I2C_BRIDGE_H

#if 0
/* ============================================================
 * 原声明内容 (停用保留, 不参与编译)
 * ============================================================ */

#include "lsm6dsr.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief LSM6DSR I2C I/O 实例 (全局，供 bsp_lsm6dsr.c 使用)
 */
extern lsm6dsr_io_t lsm6dsr_io;

/**
 * @brief 初始化 LSM6DSR I/O 实例
 * @note  必须在使用 lsm6dsr_io 之前调用
 */
void lsm6dsr_io_init(void);

/**
 * @brief 初始化软件 I2C 引脚
 * @note  配置 PA0/PA1 为 GPIO 输出模式，上拉
 */
void soft_i2c_init(void);

/**
 * @brief 获取 LSM6DSR I/O 实例
 * @return lsm6dsr_io_t 指针，供 bsp_lsm6dsr 使用
 */
lsm6dsr_io_t *soft_i2c_get_io(void);

/**
 * @brief I2C 多字节读取 (软件 I2C 实现)
 * @param ctx  未使用 (软件 I2C 不需要实例指针)
 * @param reg  起始寄存器地址
 * @param buf  读取数据缓冲区
 * @param len  读取字节数
 * @return 0=成功, 负数=失败 (-1=NULL, -2=NACK(reg), -3=NACK(addr))
 */
int8_t soft_i2c_read(void *ctx, uint8_t reg, uint8_t *buf, uint16_t len);

/**
 * @brief I2C 多字节写入 (软件 I2C 实现)
 * @param ctx  未使用 (软件 I2C 不需要实例指针)
 * @param reg  起始寄存器地址
 * @param buf  写入数据缓冲区
 * @param len  写入字节数
 * @return 0=成功, 负数=失败
 */
int8_t soft_i2c_write(void *ctx, uint8_t reg, const uint8_t *buf, uint16_t len);

#ifdef __cplusplus
}
#endif

#endif /* 0 (停用保留) */

#endif /* SOFT_I2C_BRIDGE_H */
