/**
 * @file    bsp_board.c
 * @brief   Board-level initialization — calls all BSP module init functions
 */

#include "bsp_board.h"
#include "bsp_config.h"
#include "bsp_gpio.h"
#include "bsp_uart.h"
#include "bsp_spi.h"
#include "bsp_i2c.h"
#include "bsp_adc.h"

#if BSP_USE_MOTOR
#include "bsp_motor.h"
#endif

#if BSP_USE_SERVO
#include "bsp_servo.h"
#endif

#if BSP_USE_MPU6050
#include "bsp_mpu6050.h"
#endif

#if BSP_USE_OLED
#include "bsp_oled.h"
#endif

#if BSP_USE_TRACER
#include "bsp_tracer.h"
#endif

#if BSP_USE_HCSR04
#include "bsp_hcsr04.h"
#endif

hal_status_t bsp_board_init(void)
{
    hal_status_t status;

    /* 1. GPIO (LEDs, buttons) */
    status = bsp_gpio_init();
    if (status != HAL_OK) return status;

    /* 2. Debug UART */
    status = bsp_uart_init();
    if (status != HAL_OK) return status;

    /* 3. SPI bus */
    status = bsp_spi_init();
    if (status != HAL_OK) return status;

    /* 4. I2C bus (must init before MPU6050 and OLED) */
    status = bsp_i2c_init();
    if (status != HAL_OK) return status;

    /* 5. ADC */
    status = bsp_adc_init();
    if (status != HAL_OK) return status;

    /* 6. Motor driver (conditional) */
#if BSP_USE_MOTOR
    status = bsp_motor_init();
    if (status != HAL_OK) return status;
#endif

    /* 7. Servo driver (conditional) */
#if BSP_USE_SERVO
    status = bsp_servo_init();
    if (status != HAL_OK) return status;
#endif

    /* 8. MPU6050 sensor (conditional, requires I2C) */
#if BSP_USE_MPU6050
    status = bsp_mpu6050_init();
    if (status != HAL_OK) return status;
#endif

    /* 9. OLED display (conditional, requires I2C) */
#if BSP_USE_OLED
    status = bsp_oled_init();
    if (status != HAL_OK) return status;
#endif

    /* 10. Line tracer (conditional) */
#if BSP_USE_TRACER
    status = bsp_tracer_init();
    if (status != HAL_OK) return status;
#endif

    /* 11. HC-SR04 ultrasonic (conditional) */
#if BSP_USE_HCSR04
    status = bsp_hcsr04_init();
    if (status != HAL_OK) return status;
#endif

    return HAL_OK;
}
