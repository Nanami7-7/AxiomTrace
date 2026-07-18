/**
 * @file    bsp_led.h
 * @brief   LED驱动接口
 * @note    基于hal_gpio实现，LED为低电平有效(共阳极接法)
 *          引脚映射来源于project_config.h中的PRJ_LED_xxx
 */
#ifndef BSP_LED_H
#define BSP_LED_H

#ifdef __cplusplus
extern "C" {
#endif

/* ======================== 包含 ======================== */
#include "bsp_common.h"

/* ======================== 函数接口 ======================== */

/**
 * @brief  初始化LED
 * @note   LED引脚已由SYSCFG_DL_init()配置为输出，
 *         此函数仅设置初始状态(熄灭)
 * @retval BSP_OK 初始化成功
 */
bsp_status_t bsp_led_init(void);

/**
 * @brief  点亮LED
 */
void bsp_led_on(void);

/**
 * @brief  熄灭LED
 */
void bsp_led_off(void);

/**
 * @brief  翻转LED状态
 */
void bsp_led_toggle(void);

/**
 * @brief  获取LED当前状态
 * @retval true  LED点亮
 * @retval false LED熄灭
 */
bool bsp_led_is_on(void);

#ifdef __cplusplus
}
#endif

#endif /* BSP_LED_H */
