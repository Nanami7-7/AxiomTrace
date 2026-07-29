/**
 * @file    hal_common.h
 * @brief   Common types for the small MSPM0 hardware abstraction layer.
 *
 * This base project intentionally contains only generic GPIO/UART definitions.
 * Board-specific motor, encoder, IMU, ADC and BLE definitions do not belong here.
 */
#ifndef HAL_COMMON_H
#define HAL_COMMON_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
    HAL_OK                = 0,
    HAL_ERR_INVALID_PARAM = -1,
    HAL_ERR_BUSY          = -2,
    HAL_ERR_TIMEOUT       = -3,
    HAL_ERR_HW_FAULT      = -4,
    HAL_ERR_NOT_INIT      = -5,
    HAL_ERR_UNSUPPORTED   = -6
} hal_status_t;

typedef enum {
    HAL_GPIO_PORT_A = 0,
    HAL_GPIO_PORT_B,
    HAL_GPIO_PORT_COUNT
} hal_gpio_port_t;

typedef enum {
    HAL_GPIO_DIR_INPUT  = 0,
    HAL_GPIO_DIR_OUTPUT = 1
} hal_gpio_dir_t;

typedef enum {
    HAL_GPIO_PULL_NONE = 0,
    HAL_GPIO_PULL_UP,
    HAL_GPIO_PULL_DOWN
} hal_gpio_pull_t;

typedef struct {
    hal_gpio_port_t port;
    uint32_t        pin;
    uint32_t        iomux;
} hal_gpio_pin_config_t;

/* Kept generic for future board support; this base project uses no motor timer. */
typedef enum {
    HAL_TIMER_SYS_TICK = 0,
    HAL_TIMER_COUNT
} hal_timer_id_t;

typedef enum {
    HAL_TIMER_IRQ_NONE     = 0,
    HAL_TIMER_IRQ_CC0      = 1,
    HAL_TIMER_IRQ_CC1      = 2,
    HAL_TIMER_IRQ_LOAD     = 4,
    HAL_TIMER_IRQ_CC0_CC1  = 3
} hal_timer_irq_flag_t;

/* UART0 is the debug port; UART1..UART3 remain generic for board communication. */
typedef enum {
    HAL_UART_0 = 0,
    HAL_UART_DEBUG = HAL_UART_0,
    HAL_UART_1,
    HAL_UART_2,
    HAL_UART_3,
    HAL_UART_COUNT
} hal_uart_id_t;

typedef enum {
    HAL_UART_IRQ_NONE = 0,
    HAL_UART_IRQ_RX   = 1,
    HAL_UART_IRQ_TX   = 2
} hal_uart_irq_flag_t;

#define HAL_ARRAY_SIZE(arr)   (sizeof(arr) / sizeof((arr)[0]))
#define HAL_MIN(a, b)         ((a) < (b) ? (a) : (b))
#define HAL_MAX(a, b)         ((a) > (b) ? (a) : (b))

#ifdef DEBUG
#define HAL_ASSERT(cond)       \
    do {                       \
        if (!(cond)) {         \
            for (;;) {         \
            }                  \
        }                      \
    } while (0)
#else
#define HAL_ASSERT(cond) ((void)0)
#endif

#ifdef __cplusplus
}
#endif

#endif /* HAL_COMMON_H */
