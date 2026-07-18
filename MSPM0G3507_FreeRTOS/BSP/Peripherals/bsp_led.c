/**
 * @file    bsp_led.c
 * @brief   LED驱动实现
 * @note    LED为低电平有效(共阳极)，输出低=点亮，输出高=熄灭
 *          引脚: PRJ_LED_PORT/PRJ_LED_PIN (project_config.h)
 */
#include "bsp_led.h"
#include "hal_gpio.h"
#include "project_config.h"

/* ======================== 私有宏 ======================== */

/** LED低电平有效: on=低电平, off=高电平 */
#define LED_ON_LEVEL    false
#define LED_OFF_LEVEL   true

/* ======================== 函数实现 ======================== */

bsp_status_t bsp_led_init(void)
{
    /* SYSCFG_DL_init()已配置LED引脚为输出，仅设初始状态 */
    hal_gpio_write_pin(PRJ_LED_PORT, PRJ_LED_PIN, LED_OFF_LEVEL);
    return BSP_OK;
}

void bsp_led_on(void)
{
    hal_gpio_write_pin(PRJ_LED_PORT, PRJ_LED_PIN, LED_ON_LEVEL);
}

void bsp_led_off(void)
{
    hal_gpio_write_pin(PRJ_LED_PORT, PRJ_LED_PIN, LED_OFF_LEVEL);
}

void bsp_led_toggle(void)
{
    hal_gpio_toggle_pin(PRJ_LED_PORT, PRJ_LED_PIN);
}

bool bsp_led_is_on(void)
{
    /* 低电平=点亮 */
    return !hal_gpio_read_pin(PRJ_LED_PORT, PRJ_LED_PIN);
}
