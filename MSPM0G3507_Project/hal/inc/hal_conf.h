/**
 * @file    hal_conf.h
 * @brief   HAL module enable/disable configuration
 * @note    Uncomment to enable, comment out to disable unused peripherals
 *          Disabled modules will not be compiled, saving Flash
 */

#ifndef HAL_CONF_H
#define HAL_CONF_H

/* ========== GPIO ========== */
#define HAL_GPIO_MODULE_ENABLED

/* ========== UART ========== */
#define HAL_UART_MODULE_ENABLED

/* ========== SPI ========== */
#define HAL_SPI_MODULE_ENABLED

/* ========== I2C ========== */
#define HAL_I2C_MODULE_ENABLED

/* ========== ADC ========== */
#define HAL_ADC_MODULE_ENABLED

/* ========== Timer ========== */
#define HAL_TIMER_MODULE_ENABLED

/* ========== PWM ========== */
#define HAL_PWM_MODULE_ENABLED

/* ========== DAC ========== */
#define HAL_DAC_MODULE_ENABLED

#endif /* HAL_CONF_H */
