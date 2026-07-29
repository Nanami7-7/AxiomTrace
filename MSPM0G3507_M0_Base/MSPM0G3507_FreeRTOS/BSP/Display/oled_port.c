/**
 * @file    oled_port.c
 * @brief   OLED 软件 I2C 端口层。
 * @details 提供本模块的基础功能实现。
 * @note   使用时请遵循接口约束。
 */
#include "oled_port.h"
#include "ti_msp_dl_config.h"
#include <stddef.h>

/* ============================================================
 * I2C 鏃跺簭鍙傛暟
 * ============================================================ */
#define IIC_HALF_PERIOD_US      (5U)    /* OLED 软件 I2C 端口层。 */
#define IIC_DATA_SETUP_US       (1U)    /* 鏁版嵁寤虹珛鏃堕棿 1us */
#define IIC_ACK_TIMEOUT_RETRIES (10U)   /* ACK 绛夊緟閲嶈瘯娆℃暟 */

/* OLED 软件 I2C 端口层。 */
#define I2C_DELAY_US(us)    DL_Common_delayCycles((CPUCLK_FREQ / 1000000U) * (us))

/* ============================================================
 * GPIO 浣嶆ā鎷熷畯
 * 浣跨敤 ti_msp_dl_config.h 涓畾涔夌殑 IIC_PORT / IIC_SCL_PIN / IIC_SDA_PIN
 * ============================================================ */

/* OLED 软件 I2C 端口层。 */
#define SDA_OUT()  do {                                                    \
    DL_GPIO_initDigitalOutput(IIC_SDA_IOMUX);                              \
    DL_GPIO_setPins(IIC_PORT, IIC_SDA_PIN);                                \
    DL_GPIO_enableOutput(IIC_PORT, IIC_SDA_PIN);                           \
} while (0)

/* OLED 软件 I2C 端口层。 */
#define SDA_IN()   do {                                                    \
    DL_GPIO_initDigitalInput(IIC_SDA_IOMUX);                               \
} while (0)

/* OLED 软件 I2C 端口层。 */
#define SDA_GET()  ((DL_GPIO_readPins(IIC_PORT, IIC_SDA_PIN) != 0U) ? 1U : 0U)

/* 璁剧疆 SDA 鐢靛钩 */
#define SDA(x)     do {                                                    \
    if ((x) != 0U) { DL_GPIO_setPins(IIC_PORT, IIC_SDA_PIN); }             \
    else { DL_GPIO_clearPins(IIC_PORT, IIC_SDA_PIN); }                     \
} while (0)

/* 璁剧疆 SCL 鐢靛钩 */
#define SCL(x)     do {                                                    \
    if ((x) != 0U) { DL_GPIO_setPins(IIC_PORT, IIC_SCL_PIN); }             \
    else { DL_GPIO_clearPins(IIC_PORT, IIC_SCL_PIN); }                     \
} while (0)

/* OLED 软件 I2C 端口层。 */

/**
 * @brief   OLED 软件 I2C 端口层。
 */
static void oled_i2c_start(void)
{
    SDA_OUT();
    SCL(1);
    SDA(1);
    I2C_DELAY_US(IIC_HALF_PERIOD_US);
    SDA(0);
    I2C_DELAY_US(IIC_HALF_PERIOD_US);
    SCL(0);
}

/**
 * @brief   OLED 软件 I2C 端口层。
 */
static void oled_i2c_stop(void)
{
    SDA_OUT();
    SCL(0);
    SDA(0);
    I2C_DELAY_US(IIC_HALF_PERIOD_US);
    SCL(1);
    I2C_DELAY_US(IIC_HALF_PERIOD_US);
    SDA(1);
    I2C_DELAY_US(IIC_HALF_PERIOD_US);
}

/**
 * @brief   OLED 软件 I2C 端口层。
 * @param  ack 参数说明。
 */
static void oled_i2c_send_ack(uint8_t ack)
{
    SDA_OUT();
    SCL(0);
    SDA(ack);
    I2C_DELAY_US(IIC_HALF_PERIOD_US);
    SCL(1);
    I2C_DELAY_US(IIC_HALF_PERIOD_US);
    SCL(0);
    SDA(1);
}

/**
 * @brief   OLED 软件 I2C 端口层。
 * @return 返回处理结果。
 */
static uint8_t oled_i2c_wait_ack(void)
{
    uint8_t retry = IIC_ACK_TIMEOUT_RETRIES;

    SCL(0);
    SDA(1);
    SDA_IN();

    SCL(1);
    while ((SDA_GET() != 0U) && (retry > 0U)) {
        retry--;
        I2C_DELAY_US(IIC_HALF_PERIOD_US);
    }

    if (retry == 0U) {
        oled_i2c_stop();
        return 1U;
    }

    SCL(0);
    SDA_OUT();
    return 0U;
}

/**
 * @brief   OLED 软件 I2C 端口层。
 * @param  dat 参数说明。
 */
static void oled_i2c_send_byte(uint8_t dat)
{
    int8_t i;

    SDA_OUT();
    SCL(0);

    for (i = 7; i >= 0; i--) {
        SDA((uint8_t)((dat >> i) & 0x01U));
        I2C_DELAY_US(IIC_DATA_SETUP_US);
        SCL(1);
        I2C_DELAY_US(IIC_HALF_PERIOD_US);
        SCL(0);
        I2C_DELAY_US(IIC_HALF_PERIOD_US);
    }
}

/* ============================================================
 * 鍏叡鎺ュ彛瀹炵幇
 * ============================================================ */

static volatile uint8_t g_oled_addr = (uint8_t)OLED_I2C_ADDRESS;

/**
 * @brief 设置函数 OLED_Port_SetAddress，完成对应模块的功能处理。
 * @param addr7 函数参数 addr7。
 * @return 函数执行结果。
 */
void OLED_Port_SetAddress(uint8_t addr7)
{
    g_oled_addr = (uint8_t)(addr7 & 0x7FU);
}

/**
 * @brief 初始化函数 OLED_Port_Init，完成对应模块的功能处理。
 * @return 函数执行结果。
 */
void OLED_Port_Init(void)
{
    /* OLED 软件 I2C 端口层。 */
    DL_GPIO_initDigitalOutput(IIC_SCL_IOMUX);
    DL_GPIO_setPins(IIC_PORT, IIC_SCL_PIN);
    DL_GPIO_enableOutput(IIC_PORT, IIC_SCL_PIN);

    /* OLED 软件 I2C 端口层。 */
    DL_GPIO_initDigitalOutput(IIC_SDA_IOMUX);
    DL_GPIO_setPins(IIC_PORT, IIC_SDA_PIN);
    DL_GPIO_enableOutput(IIC_PORT, IIC_SDA_PIN);

    /* OLED 软件 I2C 端口层。 */
    SCL(1);
    SDA(1);
}

/**
 * @brief 写入函数 OLED_Port_Write，完成对应模块的功能处理。
 * @param data 函数参数 data。
 * @param length 函数参数 length。
 * @return 函数执行结果。
 */
bool OLED_Port_Write(const uint8_t *data, uint16_t length)
{
    uint16_t offset;

    if ((data == NULL) || (length == 0U)) {
        return false;
    }

    /* 璧峰鏉′欢 */
    oled_i2c_start();

    /* OLED 软件 I2C 端口层。 */
    oled_i2c_send_byte((uint8_t)(g_oled_addr << 1));
    if (oled_i2c_wait_ack() != 0U) {
        oled_i2c_stop();
        return false;
    }

    /* OLED 软件 I2C 端口层。 */
    for (offset = 0U; offset < length; offset++) {
        oled_i2c_send_byte(data[offset]);
        if (oled_i2c_wait_ack() != 0U) {
            oled_i2c_stop();
            return false;
        }
    }

    /* 鍋滄鏉′欢 */
    oled_i2c_stop();
    return true;
}