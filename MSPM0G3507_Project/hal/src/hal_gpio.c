/**
 * @file    hal_gpio.c
 * @brief   GPIO HAL implementation for MSPM0G3507
 * @note    Wraps TI DriverLib DL_GPIO calls
 */

#include "hal_conf.h"

#ifdef HAL_GPIO_MODULE_ENABLED

#include "hal_gpio.h"
#include <string.h>

/* ---------- Maximum pins tracked ---------- */
#define HAL_GPIO_MAX_CALLBACKS  16

/* ---------- Callback storage ---------- */
typedef struct {
    uint32_t          port;
    uint32_t          pin;
    hal_gpio_irq_cb_t callback;
} hal_gpio_callback_entry_t;

static hal_gpio_callback_entry_t s_gpio_callbacks[HAL_GPIO_MAX_CALLBACKS];
static uint8_t s_gpio_cb_count = 0;

/* ========== API Implementation ========== */

hal_status_t hal_gpio_init(const hal_gpio_config_t *config)
{
    HAL_CHECK_PTR(config);

    DL_GPIO_InitTypeDef gpio_init = {0};

    /* Set pin direction */
    switch (config->mode) {
    case HAL_GPIO_MODE_INPUT:
        gpio_init.direction = DL_GPIO_DIRECTION_INPUT;
        break;
    case HAL_GPIO_MODE_OUTPUT:
        gpio_init.direction = DL_GPIO_DIRECTION_OUTPUT;
        break;
    case HAL_GPIO_MODE_ALTERNATE:
        gpio_init.direction = DL_GPIO_DIRECTION_ALTERNATE;
        break;
    case HAL_GPIO_MODE_ANALOG:
        gpio_init.direction = DL_GPIO_DIRECTION_ANALOG;
        break;
    default:
        return HAL_INVALID;
    }

    /* Set pull configuration */
    switch (config->pull) {
    case HAL_GPIO_PULL_NONE:
        gpio_init.pull = DL_GPIO_PULL_NO;
        break;
    case HAL_GPIO_PULL_UP:
        gpio_init.pull = DL_GPIO_PULL_UP;
        break;
    case HAL_GPIO_PULL_DOWN:
        gpio_init.pull = DL_GPIO_PULL_DOWN;
        break;
    default:
        return HAL_INVALID;
    }

    /* Configure the pin */
    DL_GPIO_init(config->port, config->pin, &gpio_init);

    /* Enable interrupt if requested */
    if (config->irq_trigger != HAL_GPIO_IRQ_TRIGGER_NONE) {
        DL_GPIO_enableInterrupt(config->port, config->pin);
        NVIC_SetPriority(GPIOA_INT_IRQn + (config->port - GPIO_PORT_A), config->irq_priority);
        NVIC_EnableIRQ(GPIOA_INT_IRQn + (config->port - GPIO_PORT_A));
    }

    return HAL_OK;
}

hal_status_t hal_gpio_deinit(uint32_t port, uint32_t pin)
{
    DL_GPIO_disableInterrupt(port, pin);
    DL_GPIO_reset(port, pin);

    /* Remove callback if registered */
    hal_gpio_unregister_callback(port, pin);

    return HAL_OK;
}

hal_gpio_pin_state_t hal_gpio_read_pin(uint32_t port, uint32_t pin)
{
    return (DL_GPIO_readPins(port, pin) != 0) ? HAL_GPIO_PIN_HIGH : HAL_GPIO_PIN_LOW;
}

hal_status_t hal_gpio_write_pin(uint32_t port, uint32_t pin, hal_gpio_pin_state_t state)
{
    if (state == HAL_GPIO_PIN_HIGH) {
        DL_GPIO_setPins(port, pin);
    } else {
        DL_GPIO_clearPins(port, pin);
    }
    return HAL_OK;
}

hal_status_t hal_gpio_toggle_pin(uint32_t port, uint32_t pin)
{
    DL_GPIO_togglePins(port, pin);
    return HAL_OK;
}

hal_status_t hal_gpio_register_callback(uint32_t port, uint32_t pin, hal_gpio_irq_cb_t cb)
{
    HAL_CHECK_PTR(cb);

    if (s_gpio_cb_count >= HAL_GPIO_MAX_CALLBACKS) {
        return HAL_ERROR;
    }

    /* Check if already registered, update if so */
    for (uint8_t i = 0; i < s_gpio_cb_count; i++) {
        if (s_gpio_callbacks[i].port == port && s_gpio_callbacks[i].pin == pin) {
            s_gpio_callbacks[i].callback = cb;
            return HAL_OK;
        }
    }

    /* Add new entry */
    s_gpio_callbacks[s_gpio_cb_count].port     = port;
    s_gpio_callbacks[s_gpio_cb_count].pin      = pin;
    s_gpio_callbacks[s_gpio_cb_count].callback = cb;
    s_gpio_cb_count++;

    return HAL_OK;
}

hal_status_t hal_gpio_unregister_callback(uint32_t port, uint32_t pin)
{
    for (uint8_t i = 0; i < s_gpio_cb_count; i++) {
        if (s_gpio_callbacks[i].port == port && s_gpio_callbacks[i].pin == pin) {
            /* Shift remaining entries */
            for (uint8_t j = i; j < s_gpio_cb_count - 1; j++) {
                s_gpio_callbacks[j] = s_gpio_callbacks[j + 1];
            }
            s_gpio_cb_count--;
            return HAL_OK;
        }
    }
    return HAL_INVALID;
}

/* ---------- IRQ dispatcher (called from GPIO ISR) ---------- */
static void hal_gpio_irq_dispatcher(uint32_t port)
{
    uint32_t pending = DL_GPIO_getEnabledInterruptStatus(port, 0xFFFFFFFF);

    for (uint8_t i = 0; i < s_gpio_cb_count; i++) {
        if (s_gpio_callbacks[i].port == port &&
            (pending & s_gpio_callbacks[i].pin) != 0) {
            DL_GPIO_clearInterruptStatus(port, s_gpio_callbacks[i].pin);
            if (s_gpio_callbacks[i].callback != NULL) {
                s_gpio_callbacks[i].callback(port, s_gpio_callbacks[i].pin);
            }
        }
    }
}

/* ---------- ISR wrappers ---------- */
void GPIOA_IRQHandler(void)
{
    hal_gpio_irq_dispatcher(GPIO_PORT_A);
}

void GPIOB_IRQHandler(void)
{
    hal_gpio_irq_dispatcher(GPIO_PORT_B);
}

#endif /* HAL_GPIO_MODULE_ENABLED */
