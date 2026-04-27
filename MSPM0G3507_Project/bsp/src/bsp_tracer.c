/**
 * @file    bsp_tracer.c
 * @brief   Line tracer module implementation (multi-channel GPIO IR sensors)
 */

#include "bsp_tracer.h"
#include "bsp_pin_config.h"
#include "bsp_config.h"
#include "hal_gpio.h"

/* ---------- Channel pin map ---------- */
typedef struct {
    uint32_t port;
    uint32_t pin;
} bsp_tracer_pinmap_t;

/* Pin map table - uses preprocessor to support 4~8 channels based on BSP_TRACER_CHANNEL_COUNT */
#if BSP_TRACER_CHANNEL_COUNT >= 1
#define TRACER_CH0_ENTRY  { BSP_TRACER_CH0_PORT, BSP_TRACER_CH0_PIN },
#else
#define TRACER_CH0_ENTRY
#endif
#if BSP_TRACER_CHANNEL_COUNT >= 2
#define TRACER_CH1_ENTRY  { BSP_TRACER_CH1_PORT, BSP_TRACER_CH1_PIN },
#else
#define TRACER_CH1_ENTRY
#endif
#if BSP_TRACER_CHANNEL_COUNT >= 3
#define TRACER_CH2_ENTRY  { BSP_TRACER_CH2_PORT, BSP_TRACER_CH2_PIN },
#else
#define TRACER_CH2_ENTRY
#endif
#if BSP_TRACER_CHANNEL_COUNT >= 4
#define TRACER_CH3_ENTRY  { BSP_TRACER_CH3_PORT, BSP_TRACER_CH3_PIN },
#else
#define TRACER_CH3_ENTRY
#endif
#if BSP_TRACER_CHANNEL_COUNT >= 5
#define TRACER_CH4_ENTRY  { BSP_TRACER_CH4_PORT, BSP_TRACER_CH4_PIN },
#else
#define TRACER_CH4_ENTRY
#endif
#if BSP_TRACER_CHANNEL_COUNT >= 6
#define TRACER_CH5_ENTRY  { BSP_TRACER_CH5_PORT, BSP_TRACER_CH5_PIN },
#else
#define TRACER_CH5_ENTRY
#endif
#if BSP_TRACER_CHANNEL_COUNT >= 7
#define TRACER_CH6_ENTRY  { BSP_TRACER_CH6_PORT, BSP_TRACER_CH6_PIN },
#else
#define TRACER_CH6_ENTRY
#endif
#if BSP_TRACER_CHANNEL_COUNT >= 8
#define TRACER_CH7_ENTRY  { BSP_TRACER_CH7_PORT, BSP_TRACER_CH7_PIN },
#else
#define TRACER_CH7_ENTRY
#endif

static const bsp_tracer_pinmap_t s_tracer_map[BSP_TRACER_CHANNEL_COUNT] = {
    TRACER_CH0_ENTRY
    TRACER_CH1_ENTRY
    TRACER_CH2_ENTRY
    TRACER_CH3_ENTRY
    TRACER_CH4_ENTRY
    TRACER_CH5_ENTRY
    TRACER_CH6_ENTRY
    TRACER_CH7_ENTRY
};

/* ========== API ========== */

hal_status_t bsp_tracer_init(void)
{
    hal_status_t status;

    for (uint32_t i = 0; i < BSP_TRACER_CHANNEL_COUNT; i++) {
        hal_gpio_config_t cfg = {
            .port        = s_tracer_map[i].port,
            .pin         = s_tracer_map[i].pin,
            .mode        = HAL_GPIO_MODE_INPUT,
            .pull        = HAL_GPIO_PULL_UP,
            .irq_trigger = HAL_GPIO_IRQ_TRIGGER_NONE,
            .irq_priority= HAL_IRQ_PRIORITY_LOW,
        };
        status = hal_gpio_init(&cfg);
        if (status != HAL_OK) return status;
    }

    return HAL_OK;
}

hal_status_t bsp_tracer_deinit(void)
{
    hal_status_t status;

    for (uint32_t i = 0; i < BSP_TRACER_CHANNEL_COUNT; i++) {
        status = hal_gpio_deinit(s_tracer_map[i].port, s_tracer_map[i].pin);
        if (status != HAL_OK) return status;
    }

    return HAL_OK;
}

uint8_t bsp_tracer_read_channel(bsp_tracer_ch_t channel)
{
    if (channel >= BSP_TRACER_CHANNEL_COUNT) return 0;

    /* LOW = black line detected → return 1 */
    hal_gpio_pin_state_t state = hal_gpio_read_pin(s_tracer_map[channel].port,
                                                    s_tracer_map[channel].pin);
    return (state == HAL_GPIO_PIN_LOW) ? 1 : 0;
}

uint8_t bsp_tracer_read_all(void)
{
    uint8_t result = 0;

    for (uint32_t i = 0; i < BSP_TRACER_CHANNEL_COUNT; i++) {
        hal_gpio_pin_state_t state = hal_gpio_read_pin(s_tracer_map[i].port,
                                                        s_tracer_map[i].pin);
        /* LOW = black line → set bit */
        if (state == HAL_GPIO_PIN_LOW) {
            result |= (1 << i);
        }
    }

    return result;
}
