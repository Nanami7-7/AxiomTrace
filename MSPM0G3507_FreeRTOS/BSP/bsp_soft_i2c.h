/**
 * @file    bsp_soft_i2c.h
 * @brief   软件I2C驱动接口
 * @note    基于hal_gpio位操作实现，支持标准模式(100kHz)
 *         SCL/SDA引脚在bsp_soft_i2c_init()中初始化为开漏输出
 *         引脚映射来源于project_config.h中的PRJ_SOFT_I2C_xxx
 *
 *         接口说明:
 *         - start/stop: I2C起始/停止条件
 *         - write_byte/read_byte: 单字节传输(含ACK/NACK)
 *         - write_reg/read_reg: 寄存器读写(写:ADDR+REG+DATA,
 *           读:ADDR+REG+RESTART+ADDR_R+DATA)
 *         - write_reg_buf/read_reg_buf: 多字节寄存器读写
 */
#ifndef BSP_SOFT_I2C_H
#define BSP_SOFT_I2C_H

#ifdef __cplusplus
extern "C" {
#endif

/* ======================== 包含 ======================== */
#include "bsp_common.h"

/* ======================== 配置宏 ======================== */

/** I2C应答: 从机收到字节后拉低SDA */
#define I2C_ACK     (0U)
/** I2C无应答: 从机不拉低SDA */
#define I2C_NACK    (1U)

/* ======================== 函数接口 ======================== */

/**
 * @brief  初始化软件I2C
 * @note   配置SCL/SDA为开漏输出高电平(空闲状态)
 *         SCL/SDA需外部上拉电阻(4.7k~10k)
 * @retval BSP_OK 初始化成功
 */
bsp_status_t bsp_soft_i2c_init(void);

/**
 * @brief  发送I2C起始条件
 * @note   SCL高电平期间，SDA从高变低
 * @retval BSP_OK 成功
 */
bsp_status_t bsp_soft_i2c_start(void);

/**
 * @brief  发送I2C停止条件
 * @note   SCL高电平期间，SDA从低变高
 * @retval BSP_OK 成功
 */
bsp_status_t bsp_soft_i2c_stop(void);

/**
 * @brief  发送一个字节并读取ACK/NACK
 * @param  data 待发送字节
 * @retval I2C_ACK  从机应答
 * @retval I2C_NACK 从机无应答
 */
uint8_t bsp_soft_i2c_write_byte(uint8_t data);

/**
 * @brief  读取一个字节并发送ACK/NACK
 * @param  ack  I2C_ACK=继续读取, I2C_NACK=最后一字节
 * @retval 读取到的字节
 */
uint8_t bsp_soft_i2c_read_byte(uint8_t ack);

/**
 * @brief  向从机寄存器写入单字节
 * @param  dev_addr 从机7位地址(不含R/W位,如0x3C)
 * @param  reg      寄存器地址
 * @param  data     待写入数据
 * @retval BSP_OK       写入成功
 * @retval BSP_ERR_NAK  从机无应答
 */
bsp_status_t bsp_soft_i2c_write_reg(uint8_t dev_addr,
    uint8_t reg, uint8_t data);

/**
 * @brief  从从机寄存器读取单字节
 * @param  dev_addr 从机7位地址(不含R/W位)
 * @param  reg      寄存器地址
 * @param  data     读取数据存放地址
 * @retval BSP_OK       读取成功
 * @retval BSP_ERR_NAK  从机无应答
 */
bsp_status_t bsp_soft_i2c_read_reg(uint8_t dev_addr,
    uint8_t reg, uint8_t *data);

/**
 * @brief  向从机寄存器连续写入多字节
 * @param  dev_addr 从机7位地址(不含R/W位)
 * @param  reg      起始寄存器地址
 * @param  buf      待写入数据缓冲区
 * @param  len      写入长度(字节)
 * @retval BSP_OK       写入成功
 * @retval BSP_ERR_NAK  从机无应答
 */
bsp_status_t bsp_soft_i2c_write_reg_buf(uint8_t dev_addr,
    uint8_t reg, const uint8_t *buf, uint16_t len);

/**
 * @brief  从从机寄存器连续读取多字节
 * @param  dev_addr 从机7位地址(不含R/W位)
 * @param  reg      起始寄存器地址
 * @param  buf      读取数据存放缓冲区
 * @param  len      读取长度(字节)
 * @retval BSP_OK       读取成功
 * @retval BSP_ERR_NAK  从机无应答
 */
bsp_status_t bsp_soft_i2c_read_reg_buf(uint8_t dev_addr,
    uint8_t reg, uint8_t *buf, uint16_t len);

#ifdef __cplusplus
}
#endif

#endif /* BSP_SOFT_I2C_H */
