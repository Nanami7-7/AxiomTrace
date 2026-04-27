/**
 * @file    bsp_hcsr04.c
 * @brief   HC-SR04 ultrasonic distance sensor implementation
 * @note    Uses GPIO interrupt for Echo capture and Timer for pulse width measurement
 */

#include "bsp_hcsr04.h"
#include "bsp_pin_config.h"
#include "bsp_config.h"
#include "hal_gpio.h"
#include "hal_timer.h"
#include "osal_delay.h"
#include "osal_semaphore.h"

/* ---------- Speed of sound: 340 m/s = 0.034 cm/µs ---------- */
#define SOUND_SPEED_CM_PER_US  0.034f

/* ---------- Internal state ---------- */
static volatile uint32_t     s_echo_start_tick  = 0;
static volatile uint32_t     s_echo_pulse_us    = 0;
static volatile uint8_t      s_echo_received    = 0;
static bsp_hcsr04_callback_t s_async_callback   = NULL;

/* OSAL semaphore for blocking mode */
static osal_semaphore_handle_t s_echo_sem = NULL;

/* ---------- Echo GPIO interrupt callback ---------- */

static void hcsr04_echo_callback(uint32_t port, uint32_t pin)
{
    (void)port;
    (void)pin;

    hal_gpio_pin_state_t state = hal_gpio_read_pin(BSP_HCSR04_ECHO_PORT, BSP_HCSR04_ECHO_PIN);

    if (state == HAL_GPIO_PIN_HIGH) {
        /* Rising edge: start timing */
        s_echo_start_tick = hal_timer_get_counter(BSP_HCSR04_TIMER_INST);
        s_echo_received = 0;
    } else {
        /* Falling edge: stop timing */
        uint32_t end_tick = hal_timer_get_counter(BSP_HCSR04_TIMER_INST);
        if (end_tick >= s_echo_start_tick) {
            s_echo_pulse_us = end_tick - s_echo_start_tick;
        } else {
            /* Timer overflow handling */
            s_echo_pulse_us = end_tick + (0xFFFFFFFF - s_echo_start_tick);
        }
        s_echo_received = 1;

        /* Signal semaphore for blocking mode */
        if (s_echo_sem != NULL) {
            osal_semaphore_give_from_isr(s_echo_sem, NULL);
        }

        /* Call async callback if registered */
        if (s_async_callback != NULL) {
            uint32_t dist_mm = (uint32_t)(s_echo_pulse_us * SOUND_SPEED_CM_PER_US / 2.0f * 10.0f);
            s_async_callback(dist_mm);
        }
    }
}

/* ========== API ========== */

hal_status_t bsp_hcsr04_init(void)
{
    hal_status_t status;

    /* Initialize Trig pin as output (low) */
    hal_gpio_config_t trig_cfg = {
        .port        = BSP_HCSR04_TRIG_PORT,
        .pin         = BSP_HCSR04_TRIG_PIN,
        .mode        = HAL_GPIO_MODE_OUTPUT,
        .pull        = HAL_GPIO_PULL_NONE,
        .irq_trigger = HAL_GPIO_IRQ_TRIGGER_NONE,
        .irq_priority= HAL_IRQ_PRIORITY_LOW,
    };
    status = hal_gpio_init(&trig_cfg);
    if (status != HAL_OK) return status;
    hal_gpio_write_pin(BSP_HCSR04_TRIG_PORT, BSP_HCSR04_TRIG_PIN, HAL_GPIO_PIN_LOW);

    /* Initialize Echo pin as input with both-edge interrupt */
    hal_gpio_config_t echo_cfg = {
        .port        = BSP_HCSR04_ECHO_PORT,
        .pin         = BSP_HCSR04_ECHO_PIN,
        .mode        = HAL_GPIO_MODE_INPUT,
        .pull        = HAL_GPIO_PULL_DOWN,
        .irq_trigger = HAL_GPIO_IRQ_TRIGGER_BOTH,
        .irq_priority= HAL_IRQ_PRIORITY_HIGH,
    };
    status = hal_gpio_init(&echo_cfg);
    if (status != HAL_OK) return status;

    /* Register echo callback */
    status = hal_gpio_register_callback(BSP_HCSR04_ECHO_PORT, BSP_HCSR04_ECHO_PIN,
                                         hcsr04_echo_callback);
    if (status != HAL_OK) return status;

    /* Initialize Timer for pulse measurement (1 µs tick) */
    hal_timer_config_t timer_cfg = {
        .instance     = BSP_HCSR04_TIMER_INST,
        .mode         = HAL_TIMER_MODE_PERIODIC,
        .clk_src      = HAL_TIMER_CLK_BUSCLK,
        .clk_div      = BSP_BUSCLK_FREQ_HZ / 1000000UL,  /* 1 MHz = 1 µs tick */
        .period_us    = 0xFFFFFFFF,                       /* Maximum period */
        .irq_priority = HAL_IRQ_PRIORITY_MEDIUM,
    };
    status = hal_timer_init(&timer_cfg);
    if (status != HAL_OK) return status;

    hal_timer_start(BSP_HCSR04_TIMER_INST);

    /* Create binary semaphore for blocking mode */
    osal_semaphore_create_binary(&s_echo_sem);

    return HAL_OK;
}

hal_status_t bsp_hcsr04_deinit(void)
{
    hal_timer_stop(BSP_HCSR04_TIMER_INST);
    hal_timer_deinit(BSP_HCSR04_TIMER_INST);
    hal_gpio_unregister_callback(BSP_HCSR04_ECHO_PORT, BSP_HCSR04_ECHO_PIN);
    hal_gpio_deinit(BSP_HCSR04_TRIG_PORT, BSP_HCSR04_TRIG_PIN);
    hal_gpio_deinit(BSP_HCSR04_ECHO_PORT, BSP_HCSR04_ECHO_PIN);

    if (s_echo_sem != NULL) {
        osal_semaphore_delete(s_echo_sem);
        s_echo_sem = NULL;
    }

    return HAL_OK;
}

hal_status_t bsp_hcsr04_get_distance(uint32_t *distance_mm, uint32_t timeout_ms)
{
    if (distance_mm == NULL) return HAL_INVALID;

    s_echo_received = 0;

    /* Send trigger pulse: 10 µs HIGH */
    hal_gpio_write_pin(BSP_HCSR04_TRIG_PORT, BSP_HCSR04_TRIG_PIN, HAL_GPIO_PIN_HIGH);
    osal_delay_ms(1);   /* 1 ms delay (>> 10 µs, sufficient) */
    hal_gpio_write_pin(BSP_HCSR04_TRIG_PORT, BSP_HCSR04_TRIG_PIN, HAL_GPIO_PIN_LOW);

    /* Wait for echo (blocking with semaphore) */
    hal_status_t status = osal_semaphore_take(s_echo_sem, timeout_ms);
    if (status != HAL_OK) {
        return HAL_TIMEOUT;
    }

    if (!s_echo_received) return HAL_ERROR;

    /* distance = speed * time / 2, in mm */
    *distance_mm = (uint32_t)(s_echo_pulse_us * SOUND_SPEED_CM_PER_US / 2.0f * 10.0f);

    return HAL_OK;
}

hal_status_t bsp_hcsr04_get_distance_cm(uint32_t *distance_cm, uint32_t timeout_ms)
{
    uint32_t mm;
    hal_status_t status = bsp_hcsr04_get_distance(&mm, timeout_ms);
    if (status != HAL_OK) return status;

    *distance_cm = mm / 10;
    return HAL_OK;
}

hal_status_t bsp_hcsr04_start_measure(void)
{
    s_echo_received = 0;

    /* Send trigger pulse */
    hal_gpio_write_pin(BSP_HCSR04_TRIG_PORT, BSP_HCSR04_TRIG_PIN, HAL_GPIO_PIN_HIGH);
    osal_delay_ms(1);
    hal_gpio_write_pin(BSP_HCSR04_TRIG_PORT, BSP_HCSR04_TRIG_PIN, HAL_GPIO_PIN_LOW);

    return HAL_OK;
}

hal_status_t bsp_hcsr04_register_callback(bsp_hcsr04_callback_t callback)
{
    if (callback == NULL) return HAL_INVALID;
    s_async_callback = callback;
    return HAL_OK;
}
