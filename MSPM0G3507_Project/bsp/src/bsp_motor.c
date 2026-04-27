/**
 * @file    bsp_motor.c
 * @brief   Motor driver implementation (PWM + GPIO H-bridge control)
 */

#include "bsp_motor.h"
#include "bsp_pin_config.h"
#include "bsp_config.h"
#include "hal_gpio.h"
#include "hal_pwm.h"

/* ---------- Motor pin map ---------- */
typedef struct {
    uint32_t pwm_inst;
    uint32_t pwm_channel;
    uint32_t in1_port;
    uint32_t in1_pin;
    uint32_t in2_port;
    uint32_t in2_pin;
} bsp_motor_pinmap_t;

static const bsp_motor_pinmap_t s_motor_map[BSP_MOTOR_COUNT] = {
    { BSP_MOTOR_A_PWM_INST, BSP_MOTOR_A_PWM_CHANNEL,
      BSP_MOTOR_A_IN1_PORT, BSP_MOTOR_A_IN1_PIN,
      BSP_MOTOR_A_IN2_PORT, BSP_MOTOR_A_IN2_PIN },
    { BSP_MOTOR_B_PWM_INST, BSP_MOTOR_B_PWM_CHANNEL,
      BSP_MOTOR_B_IN1_PORT, BSP_MOTOR_B_IN1_PIN,
      BSP_MOTOR_B_IN2_PORT, BSP_MOTOR_B_IN2_PIN },
};

/* ========== API ========== */

hal_status_t bsp_motor_init(void)
{
    hal_status_t status;

    for (uint32_t i = 0; i < BSP_MOTOR_COUNT; i++) {
        /* Initialize direction GPIO pins (IN1, IN2) as output, low */
        hal_gpio_config_t gpio_cfg = {
            .port        = s_motor_map[i].in1_port,
            .pin         = s_motor_map[i].in1_pin,
            .mode        = HAL_GPIO_MODE_OUTPUT,
            .pull        = HAL_GPIO_PULL_NONE,
            .irq_trigger = HAL_GPIO_IRQ_TRIGGER_NONE,
            .irq_priority= HAL_IRQ_PRIORITY_LOW,
        };
        status = hal_gpio_init(&gpio_cfg);
        if (status != HAL_OK) return status;
        hal_gpio_write_pin(s_motor_map[i].in1_port, s_motor_map[i].in1_pin, HAL_GPIO_PIN_LOW);

        gpio_cfg.port = s_motor_map[i].in2_port;
        gpio_cfg.pin  = s_motor_map[i].in2_pin;
        status = hal_gpio_init(&gpio_cfg);
        if (status != HAL_OK) return status;
        hal_gpio_write_pin(s_motor_map[i].in2_port, s_motor_map[i].in2_pin, HAL_GPIO_PIN_LOW);

        /* Initialize PWM for speed control, 0% duty (stopped) */
        hal_pwm_config_t pwm_cfg = {
            .instance     = s_motor_map[i].pwm_inst,
            .channel      = s_motor_map[i].pwm_channel,
            .freq_hz      = BSP_MOTOR_PWM_FREQ_HZ,
            .duty_percent = 0,
            .irq_priority = HAL_IRQ_PRIORITY_LOW,
        };
        status = hal_pwm_init(&pwm_cfg);
        if (status != HAL_OK) return status;
    }

    return HAL_OK;
}

hal_status_t bsp_motor_deinit(void)
{
    hal_status_t status;

    for (uint32_t i = 0; i < BSP_MOTOR_COUNT; i++) {
        hal_pwm_stop(s_motor_map[i].pwm_inst, s_motor_map[i].pwm_channel);
        status = hal_pwm_deinit(s_motor_map[i].pwm_inst, s_motor_map[i].pwm_channel);
        if (status != HAL_OK) return status;
        hal_gpio_write_pin(s_motor_map[i].in1_port, s_motor_map[i].in1_pin, HAL_GPIO_PIN_LOW);
        hal_gpio_write_pin(s_motor_map[i].in2_port, s_motor_map[i].in2_pin, HAL_GPIO_PIN_LOW);
    }

    return HAL_OK;
}

hal_status_t bsp_motor_set_speed(bsp_motor_t motor, uint32_t speed)
{
    if (motor >= BSP_MOTOR_COUNT) return HAL_INVALID;
    if (speed > 100) speed = 100;

    return hal_pwm_set_duty(s_motor_map[motor].pwm_inst,
                            s_motor_map[motor].pwm_channel, speed);
}

hal_status_t bsp_motor_set_direction(bsp_motor_t motor, bsp_motor_dir_t direction)
{
    if (motor >= BSP_MOTOR_COUNT) return HAL_INVALID;

    const bsp_motor_pinmap_t *m = &s_motor_map[motor];

    switch (direction) {
    case BSP_MOTOR_DIR_FORWARD:
        hal_gpio_write_pin(m->in1_port, m->in1_pin, HAL_GPIO_PIN_HIGH);
        hal_gpio_write_pin(m->in2_port, m->in2_pin, HAL_GPIO_PIN_LOW);
        break;
    case BSP_MOTOR_DIR_BACKWARD:
        hal_gpio_write_pin(m->in1_port, m->in1_pin, HAL_GPIO_PIN_LOW);
        hal_gpio_write_pin(m->in2_port, m->in2_pin, HAL_GPIO_PIN_HIGH);
        break;
    case BSP_MOTOR_DIR_STOP:
        hal_gpio_write_pin(m->in1_port, m->in1_pin, HAL_GPIO_PIN_LOW);
        hal_gpio_write_pin(m->in2_port, m->in2_pin, HAL_GPIO_PIN_LOW);
        break;
    case BSP_MOTOR_DIR_BRAKE:
        hal_gpio_write_pin(m->in1_port, m->in1_pin, HAL_GPIO_PIN_HIGH);
        hal_gpio_write_pin(m->in2_port, m->in2_pin, HAL_GPIO_PIN_HIGH);
        break;
    default:
        return HAL_INVALID;
    }

    return HAL_OK;
}

hal_status_t bsp_motor_run(bsp_motor_t motor, bsp_motor_dir_t direction, uint32_t speed)
{
    hal_status_t status;

    status = bsp_motor_set_direction(motor, direction);
    if (status != HAL_OK) return status;

    status = bsp_motor_set_speed(motor, speed);
    if (status != HAL_OK) return status;

    /* Start PWM if not already running */
    return hal_pwm_start(s_motor_map[motor].pwm_inst, s_motor_map[motor].pwm_channel);
}

hal_status_t bsp_motor_stop(bsp_motor_t motor)
{
    hal_status_t status;
    status = bsp_motor_set_speed(motor, 0);
    if (status != HAL_OK) return status;
    return bsp_motor_set_direction(motor, BSP_MOTOR_DIR_STOP);
}

hal_status_t bsp_motor_brake(bsp_motor_t motor)
{
    hal_status_t status;
    status = bsp_motor_set_speed(motor, 0);
    if (status != HAL_OK) return status;
    return bsp_motor_set_direction(motor, BSP_MOTOR_DIR_BRAKE);
}
