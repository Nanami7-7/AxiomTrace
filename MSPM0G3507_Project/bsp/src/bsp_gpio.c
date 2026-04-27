/**
 * @file    bsp_gpio.c
 * @brief   BSP GPIO implementation (LEDs and buttons)
 */

#include "bsp_gpio.h"
#include "bsp_pin_config.h"
#include "hal_gpio.h"

/* ---------- LED pin mapping ---------- */
typedef struct {
    uint32_t port;
    uint32_t pin;
    hal_gpio_pin_state_t active_level;
} bsp_led_map_t;

static const bsp_led_map_t s_led_map[] = {
    { BSP_LED1_PORT, BSP_LED1_PIN, BSP_LED1_ACTIVE },
    { BSP_LED2_PORT, BSP_LED2_PIN, BSP_LED2_ACTIVE },
};

/* ---------- Button pin mapping ---------- */
typedef struct {
    uint32_t port;
    uint32_t pin;
    hal_gpio_pin_state_t active_level;
} bsp_key_map_t;

static const bsp_key_map_t s_key_map[] = {
    { BSP_KEY1_PORT, BSP_KEY1_PIN, BSP_KEY1_ACTIVE },
    { BSP_KEY2_PORT, BSP_KEY2_PIN, BSP_KEY2_ACTIVE },
};

/* ========== API ========== */

hal_status_t bsp_gpio_init(void)
{
    hal_status_t status;

    /* Initialize LED pins as output */
    for (uint32_t i = 0; i < sizeof(s_led_map) / sizeof(s_led_map[0]); i++) {
        hal_gpio_config_t cfg = {
            .port        = s_led_map[i].port,
            .pin         = s_led_map[i].pin,
            .mode        = HAL_GPIO_MODE_OUTPUT,
            .pull        = HAL_GPIO_PULL_NONE,
            .irq_trigger = HAL_GPIO_IRQ_TRIGGER_NONE,
            .irq_priority= HAL_IRQ_PRIORITY_LOW,
        };
        status = hal_gpio_init(&cfg);
        if (status != HAL_OK) return status;

        /* Turn off LED initially */
        hal_gpio_write_pin(s_led_map[i].port, s_led_map[i].pin,
                           (s_led_map[i].active_level == HAL_GPIO_PIN_HIGH)
                               ? HAL_GPIO_PIN_LOW : HAL_GPIO_PIN_HIGH);
    }

    /* Initialize button pins as input with pull-up */
    for (uint32_t i = 0; i < sizeof(s_key_map) / sizeof(s_key_map[0]); i++) {
        hal_gpio_config_t cfg = {
            .port        = s_key_map[i].port,
            .pin         = s_key_map[i].pin,
            .mode        = HAL_GPIO_MODE_INPUT,
            .pull        = HAL_GPIO_PULL_UP,
            .irq_trigger = HAL_GPIO_IRQ_TRIGGER_FALLING,
            .irq_priority= HAL_IRQ_PRIORITY_MEDIUM,
        };
        status = hal_gpio_init(&cfg);
        if (status != HAL_OK) return status;
    }

    return HAL_OK;
}

hal_status_t bsp_led_on(bsp_led_t led)
{
    if (led >= sizeof(s_led_map) / sizeof(s_led_map[0])) {
        return HAL_INVALID;
    }
    return hal_gpio_write_pin(s_led_map[led].port, s_led_map[led].pin, s_led_map[led].active_level);
}

hal_status_t bsp_led_off(bsp_led_t led)
{
    if (led >= sizeof(s_led_map) / sizeof(s_led_map[0])) {
        return HAL_INVALID;
    }
    hal_gpio_pin_state_t off_level = (s_led_map[led].active_level == HAL_GPIO_PIN_HIGH)
                                      ? HAL_GPIO_PIN_LOW : HAL_GPIO_PIN_HIGH;
    return hal_gpio_write_pin(s_led_map[led].port, s_led_map[led].pin, off_level);
}

hal_status_t bsp_led_toggle(bsp_led_t led)
{
    if (led >= sizeof(s_led_map) / sizeof(s_led_map[0])) {
        return HAL_INVALID;
    }
    return hal_gpio_toggle_pin(s_led_map[led].port, s_led_map[led].pin);
}

uint8_t bsp_key_read(bsp_key_t key)
{
    if (key >= sizeof(s_key_map) / sizeof(s_key_map[0])) {
        return 0;
    }
    hal_gpio_pin_state_t state = hal_gpio_read_pin(s_key_map[key].port, s_key_map[key].pin);
    return (state == s_key_map[key].active_level) ? 1 : 0;
}
