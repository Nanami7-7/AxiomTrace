/**
 * @file    bsp_soft_i2c.c
 * @brief   软件I2C驱动实现
 * @note    基于hal_gpio + osal_delay_us实现I2C时序
 *         时序参考I2C标准模式(100kHz):
 *         - SCL高电平时间 >= 4.0us
 *         - SCL低电平时间 >= 4.7us
 *         - 起始条件保持时间 >= 4.0us
 *         - 停止条件建立时间 >= 4.0us
 *
 *         80MHz下osal_delay_us(5)约5us，满足标准模式时序
 *         若需快速模式(400kHz)，需缩短延时并验证信号完整性
 */
#include "bsp_soft_i2c.h"
#include "hal_gpio.h"
#include "osal_api.h"
#include "project_config.h"

/* ======================== 私有宏 ======================== */

/**
 * I2C位操作延时(微秒)
 * 标准模式: 半周期5us → SCL频率约100kHz
 * 快速模式: 半周期2us → SCL频率约250kHz
 */
#define I2C_DELAY_US       5U

/**
 * I2C起始/停止条件延时(微秒)
 * 标准模式要求>=4.0us
 */
#define I2C_START_STOP_US  5U

/* ---- SCL/SDA引脚操作宏 ---- */

/** SCL输出高电平 */
#define SCL_HIGH()  \
    hal_gpio_set_pin(PRJ_SOFT_I2C_SCL_PORT, PRJ_SOFT_I2C_SCL_PIN)

/** SCL输出低电平 */
#define SCL_LOW()   \
    hal_gpio_clear_pin(PRJ_SOFT_I2C_SCL_PORT, PRJ_SOFT_I2C_SCL_PIN)

/** SDA输出高电平(释放SDA线,由外部上拉拉高) */
#define SDA_HIGH()  \
    hal_gpio_set_pin(PRJ_SOFT_I2C_SDA_PORT, PRJ_SOFT_I2C_SDA_PIN)

/** SDA输出低电平 */
#define SDA_LOW()   \
    hal_gpio_clear_pin(PRJ_SOFT_I2C_SDA_PORT, PRJ_SOFT_I2C_SDA_PIN)

/** 读取SDA线电平 */
#define SDA_READ()  \
    hal_gpio_read_pin(PRJ_SOFT_I2C_SDA_PORT, PRJ_SOFT_I2C_SDA_PIN)

/* ======================== 私有变量 ======================== */

/** SCL引脚配置(用于init) */
static const hal_gpio_pin_config_t s_scl_cfg = {
    .port  = PRJ_SOFT_I2C_SCL_PORT,
    .pin   = PRJ_SOFT_I2C_SCL_PIN,
    .iomux = PRJ_SOFT_I2C_SCL_IOMUX,
};

/** SDA引脚配置(用于init和方向切换) */
static const hal_gpio_pin_config_t s_sda_cfg = {
    .port  = PRJ_SOFT_I2C_SDA_PORT,
    .pin   = PRJ_SOFT_I2C_SDA_PIN,
    .iomux = PRJ_SOFT_I2C_SDA_IOMUX,
};

/** 初始化标志 */
static bool s_is_inited = false;

/* ======================== 私有函数 ======================== */

/**
 * @brief  SDA方向切换为输入(读取ACK或数据)
 */
static void sda_set_input(void)
{
    hal_gpio_set_direction(&s_sda_cfg, HAL_GPIO_DIR_INPUT);
    /* 释放SDA输出，让外部上拉或从机驱动 */
    SDA_HIGH();
}

/**
 * @brief  SDA方向切换为输出(发送地址/数据)
 */
static void sda_set_output(void)
{
    hal_gpio_set_direction(&s_sda_cfg, HAL_GPIO_DIR_OUTPUT);
}

/**
 * @brief  等待SCL高电平并读取SDA(从机可能拉低SCL做时钟拉伸)
 * @note   简化实现:不处理时钟拉伸，直接延时
 */
static void i2c_wait_scl_high(void)
{
    SCL_HIGH();
    osal_delay_us(I2C_DELAY_US);
}

/* ======================== 公共函数实现 ======================== */

bsp_status_t bsp_soft_i2c_init(void)
{
    if (s_is_inited) {
        return BSP_OK;
    }

    /* 初始化SCL为输出高电平(空闲) */
    hal_gpio_init_output(&s_scl_cfg);
    SCL_HIGH();

    /* 初始化SDA为输出高电平(空闲) */
    hal_gpio_init_output(&s_sda_cfg);
    SDA_HIGH();

    s_is_inited = true;
    return BSP_OK;
}

bsp_status_t bsp_soft_i2c_start(void)
{
    /* 起始条件: SCL高电平期间，SDA从高变低 */
    sda_set_output();
    SDA_HIGH();
    SCL_HIGH();
    osal_delay_us(I2C_START_STOP_US);

    SDA_LOW();
    osal_delay_us(I2C_START_STOP_US);

    SCL_LOW();
    osal_delay_us(I2C_DELAY_US);

    return BSP_OK;
}

bsp_status_t bsp_soft_i2c_stop(void)
{
    /* 停止条件: SCL高电平期间，SDA从低变高 */
    sda_set_output();
    SDA_LOW();
    SCL_LOW();
    osal_delay_us(I2C_DELAY_US);

    SCL_HIGH();
    osal_delay_us(I2C_START_STOP_US);

    SDA_HIGH();
    osal_delay_us(I2C_START_STOP_US);

    return BSP_OK;
}

uint8_t bsp_soft_i2c_write_byte(uint8_t data)
{
    uint8_t ack;

    sda_set_output();

    /* 发送8位数据, MSB first */
    for (uint8_t i = 0U; i < 8U; i++) {
        SCL_LOW();
        osal_delay_us(I2C_DELAY_US);

        if (data & 0x80U) {
            SDA_HIGH();
        } else {
            SDA_LOW();
        }
        data <<= 1;

        i2c_wait_scl_high();
        SCL_LOW();
    }

    /* 读取ACK: 第9个时钟周期 */
    sda_set_input();
    SCL_LOW();
    osal_delay_us(I2C_DELAY_US);

    i2c_wait_scl_high();
    ack = SDA_READ() ? I2C_NACK : I2C_ACK;

    SCL_LOW();
    osal_delay_us(I2C_DELAY_US);

    return ack;
}

uint8_t bsp_soft_i2c_read_byte(uint8_t ack)
{
    uint8_t data = 0U;

    sda_set_input();

    /* 读取8位数据, MSB first */
    for (uint8_t i = 0U; i < 8U; i++) {
        SCL_LOW();
        osal_delay_us(I2C_DELAY_US);

        i2c_wait_scl_high();

        data <<= 1;
        if (SDA_READ()) {
            data |= 0x01U;
        }

        SCL_LOW();
    }

    /* 发送ACK/NACK: 第9个时钟周期 */
    sda_set_output();
    SCL_LOW();
    osal_delay_us(I2C_DELAY_US);

    if (I2C_ACK == ack) {
        SDA_LOW();   /* ACK: 拉低SDA */
    } else {
        SDA_HIGH();  /* NACK: 释放SDA */
    }

    i2c_wait_scl_high();
    SCL_LOW();
    osal_delay_us(I2C_DELAY_US);

    return data;
}

bsp_status_t bsp_soft_i2c_write_reg(uint8_t dev_addr,
    uint8_t reg, uint8_t data)
{
    bsp_soft_i2c_start();

    /* 发送设备地址+写标志 */
    if (I2C_NACK == bsp_soft_i2c_write_byte(dev_addr << 1)) {
        bsp_soft_i2c_stop();
        return BSP_ERR_NAK;
    }

    /* 发送寄存器地址 */
    if (I2C_NACK == bsp_soft_i2c_write_byte(reg)) {
        bsp_soft_i2c_stop();
        return BSP_ERR_NAK;
    }

    /* 发送数据 */
    if (I2C_NACK == bsp_soft_i2c_write_byte(data)) {
        bsp_soft_i2c_stop();
        return BSP_ERR_NAK;
    }

    bsp_soft_i2c_stop();
    return BSP_OK;
}

bsp_status_t bsp_soft_i2c_read_reg(uint8_t dev_addr,
    uint8_t reg, uint8_t *data)
{
    if (NULL == data) {
        return BSP_ERR_NULL_PTR;
    }

    bsp_soft_i2c_start();

    /* 发送设备地址+写标志(写寄存器地址阶段) */
    if (I2C_NACK == bsp_soft_i2c_write_byte(dev_addr << 1)) {
        bsp_soft_i2c_stop();
        return BSP_ERR_NAK;
    }

    /* 发送寄存器地址 */
    if (I2C_NACK == bsp_soft_i2c_write_byte(reg)) {
        bsp_soft_i2c_stop();
        return BSP_ERR_NAK;
    }

    /* 重复起始条件(切换到读方向) */
    bsp_soft_i2c_start();

    /* 发送设备地址+读标志 */
    if (I2C_NACK == bsp_soft_i2c_write_byte(
            (dev_addr << 1) | 0x01U)) {
        bsp_soft_i2c_stop();
        return BSP_ERR_NAK;
    }

    /* 读取数据(最后一字节发NACK) */
    *data = bsp_soft_i2c_read_byte(I2C_NACK);

    bsp_soft_i2c_stop();
    return BSP_OK;
}

bsp_status_t bsp_soft_i2c_write_reg_buf(uint8_t dev_addr,
    uint8_t reg, const uint8_t *buf, uint16_t len)
{
    if ((NULL == buf) && (len > 0U)) {
        return BSP_ERR_NULL_PTR;
    }

    bsp_soft_i2c_start();

    /* 发送设备地址+写标志 */
    if (I2C_NACK == bsp_soft_i2c_write_byte(dev_addr << 1)) {
        bsp_soft_i2c_stop();
        return BSP_ERR_NAK;
    }

    /* 发送寄存器地址 */
    if (I2C_NACK == bsp_soft_i2c_write_byte(reg)) {
        bsp_soft_i2c_stop();
        return BSP_ERR_NAK;
    }

    /* 连续写入数据 */
    for (uint16_t i = 0U; i < len; i++) {
        if (I2C_NACK == bsp_soft_i2c_write_byte(buf[i])) {
            bsp_soft_i2c_stop();
            return BSP_ERR_NAK;
        }
    }

    bsp_soft_i2c_stop();
    return BSP_OK;
}

bsp_status_t bsp_soft_i2c_read_reg_buf(uint8_t dev_addr,
    uint8_t reg, uint8_t *buf, uint16_t len)
{
    if (NULL == buf) {
        return BSP_ERR_NULL_PTR;
    }

    if (0U == len) {
        return BSP_OK;
    }

    bsp_soft_i2c_start();

    /* 发送设备地址+写标志(写寄存器地址阶段) */
    if (I2C_NACK == bsp_soft_i2c_write_byte(dev_addr << 1)) {
        bsp_soft_i2c_stop();
        return BSP_ERR_NAK;
    }

    /* 发送寄存器地址 */
    if (I2C_NACK == bsp_soft_i2c_write_byte(reg)) {
        bsp_soft_i2c_stop();
        return BSP_ERR_NAK;
    }

    /* 重复起始条件 */
    bsp_soft_i2c_start();

    /* 发送设备地址+读标志 */
    if (I2C_NACK == bsp_soft_i2c_write_byte(
            (dev_addr << 1) | 0x01U)) {
        bsp_soft_i2c_stop();
        return BSP_ERR_NAK;
    }

    /* 连续读取数据 */
    for (uint16_t i = 0U; i < len; i++) {
        if (i < (len - 1U)) {
            buf[i] = bsp_soft_i2c_read_byte(I2C_ACK);
        } else {
            buf[i] = bsp_soft_i2c_read_byte(I2C_NACK);
        }
    }

    bsp_soft_i2c_stop();
    return BSP_OK;
}
