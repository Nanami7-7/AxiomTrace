/**
 * @file    hal_gpio.h
 * @brief   GPIO hardware abstraction layer interface
 */

#ifndef HAL_GPIO_H
#define HAL_GPIO_H

#ifdef __cplusplus
extern "C" {
#endif

#include "hal_common.h"

/* ---------- Pin state ---------- */
typedef enum {
    HAL_GPIO_PIN_LOW  = 0,
    HAL_GPIO_PIN_HIGH = 1,
} hal_gpio_pin_state_t;

/* ---------- Pin mode ---------- */
typedef enum {
    HAL_GPIO_MODE_INPUT    = 0,
    HAL_GPIO_MODE_OUTPUT   = 1,
    HAL_GPIO_MODE_ALTERNATE = 2,
    HAL_GPIO_MODE_ANALOG   = 3,
} hal_gpio_mode_t;

/* ---------- Pull configuration ---------- */
typedef enum {
    HAL_GPIO_PULL_NONE = 0,
    HAL_GPIO_PULL_UP   = 1,
    HAL_GPIO_PULL_DOWN = 2,
} hal_gpio_pull_t;

/* ---------- Interrupt trigger ---------- */
typedef enum {
    HAL_GPIO_IRQ_TRIGGER_NONE    = 0,
    HAL_GPIO_IRQ_TRIGGER_RISING  = 1,
    HAL_GPIO_IRQ_TRIGGER_FALLING = 2,
    HAL_GPIO_IRQ_TRIGGER_BOTH    = 3,
    HAL_GPIO_IRQ_TRIGGER_HIGH    = 4,
    HAL_GPIO_IRQ_TRIGGER_LOW     = 5,
} hal_gpio_irq_trigger_t;

/* ---------- GPIO configuration ---------- */
typedef struct {
    uint32_t              port;          /* GPIO_PORT_A / GPIO_PORT_B */
    uint32_t              pin;           /* DL_GPIO_PIN_0 ~ DL_GPIO_PIN_31 */
    hal_gpio_mode_t       mode;
    hal_gpio_pull_t       pull;
    hal_gpio_irq_trigger_t irq_trigger;  /* IRQ trigger type (NONE = no interrupt) */
    hal_irq_priority_t    irq_priority;
} hal_gpio_config_t;

/* ---------- GPIO callback ---------- */
typedef void (*hal_gpio_irq_cb_t)(uint32_t port, uint32_t pin);

/* ========== API ========== */

hal_status_t hal_gpio_init(const hal_gpio_config_t *config);
hal_status_t hal_gpio_deinit(uint32_t port, uint32_t pin);

hal_gpio_pin_state_t hal_gpio_read_pin(uint32_t port, uint32_t pin);
hal_status_t hal_gpio_write_pin(uint32_t port, uint32_t pin, hal_gpio_pin_state_t state);
hal_status_t hal_gpio_toggle_pin(uint32_t port, uint32_t pin);

hal_status_t hal_gpio_register_callback(uint32_t port, uint32_t pin, hal_gpio_irq_cb_t cb);
hal_status_t hal_gpio_unregister_callback(uint32_t port, uint32_t pin);

#ifdef __cplusplus
}
#endif

#endif /* HAL_GPIO_H */
