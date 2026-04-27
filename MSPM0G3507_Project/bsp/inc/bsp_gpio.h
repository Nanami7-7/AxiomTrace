/**
 * @file    bsp_gpio.h
 * @brief   BSP GPIO interface (LED, button, etc.)
 */

#ifndef BSP_GPIO_H
#define BSP_GPIO_H

#ifdef __cplusplus
extern "C" {
#endif

#include "hal_common.h"

/* ---------- LED enumeration ---------- */
typedef enum {
    BSP_LED_1 = 0,
    BSP_LED_2 = 1,
} bsp_led_t;

/* ---------- Button enumeration ---------- */
typedef enum {
    BSP_KEY_1 = 0,
    BSP_KEY_2 = 1,
} bsp_key_t;

/* ========== API ========== */

hal_status_t bsp_gpio_init(void);

/* LED operations */
hal_status_t bsp_led_on(bsp_led_t led);
hal_status_t bsp_led_off(bsp_led_t led);
hal_status_t bsp_led_toggle(bsp_led_t led);

/* Button operations */
uint8_t bsp_key_read(bsp_key_t key);   /* Returns 1 = pressed, 0 = released */

#ifdef __cplusplus
}
#endif

#endif /* BSP_GPIO_H */
