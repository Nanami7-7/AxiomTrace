/**
 * @file    bsp_pin_config.h
 * @brief   Board-level pin mapping table (logical name → physical pin)
 * @note    Change this file when porting to a different board
 */

#ifndef BSP_PIN_CONFIG_H
#define BSP_PIN_CONFIG_H

/* ========== LED Pin Definitions ========== */
#define BSP_LED1_PORT       GPIO_PORT_A
#define BSP_LED1_PIN        DL_GPIO_PIN_0
#define BSP_LED1_ACTIVE     HAL_GPIO_PIN_HIGH    /* LED on when pin is HIGH */

#define BSP_LED2_PORT       GPIO_PORT_A
#define BSP_LED2_PIN        DL_GPIO_PIN_1
#define BSP_LED2_ACTIVE     HAL_GPIO_PIN_HIGH

/* ========== Button Pin Definitions ========== */
#define BSP_KEY1_PORT       GPIO_PORT_B
#define BSP_KEY1_PIN        DL_GPIO_PIN_2
#define BSP_KEY1_ACTIVE     HAL_GPIO_PIN_LOW     /* Pressed = LOW (pull-up) */

#define BSP_KEY2_PORT       GPIO_PORT_B
#define BSP_KEY2_PIN        DL_GPIO_PIN_3
#define BSP_KEY2_ACTIVE     HAL_GPIO_PIN_LOW

/* ========== Debug UART Pin Definitions ========== */
#define BSP_DEBUG_UART_INST       UART_0_INST
#define BSP_DEBUG_UART_TX_PORT    GPIO_PORT_A
#define BSP_DEBUG_UART_TX_PIN     DL_GPIO_PIN_10
#define BSP_DEBUG_UART_RX_PORT    GPIO_PORT_A
#define BSP_DEBUG_UART_RX_PIN     DL_GPIO_PIN_11
#define BSP_DEBUG_UART_BAUDRATE   115200

/* ========== SPI0 Pin Definitions (Flash / Display) ========== */
#define BSP_SPI0_INST             SPI_0_INST
#define BSP_SPI0_SCLK_PORT        GPIO_PORT_A
#define BSP_SPI0_SCLK_PIN         DL_GPIO_PIN_4
#define BSP_SPI0_MOSI_PORT        GPIO_PORT_A
#define BSP_SPI0_MOSI_PIN         DL_GPIO_PIN_5
#define BSP_SPI0_MISO_PORT        GPIO_PORT_A
#define BSP_SPI0_MISO_PIN         DL_GPIO_PIN_6
#define BSP_SPI0_CS_PORT          GPIO_PORT_A
#define BSP_SPI0_CS_PIN           DL_GPIO_PIN_7

/* ========== I2C0 Pin Definitions (Sensor) ========== */
#define BSP_I2C0_INST             I2C_0_INST
#define BSP_I2C0_SDA_PORT         GPIO_PORT_A
#define BSP_I2C0_SDA_PIN          DL_GPIO_PIN_8
#define BSP_I2C0_SCL_PORT         GPIO_PORT_A
#define BSP_I2C0_SCL_PIN          DL_GPIO_PIN_9

/* ========== ADC Channel Definitions ========== */
#define BSP_ADC0_INST             ADC0_INST
#define BSP_ADC_TEMP_CHANNEL      0   /* Internal temperature sensor */
#define BSP_ADC_AIN0_CHANNEL      1   /* External analog input 0 */

/* ========== Motor Pin Definitions (TB6612 / L298N) ========== */
#define BSP_MOTOR_A_PWM_INST      TIMA0_INST
#define BSP_MOTOR_A_PWM_CHANNEL   0
#define BSP_MOTOR_A_IN1_PORT      GPIO_PORT_B
#define BSP_MOTOR_A_IN1_PIN       DL_GPIO_PIN_10
#define BSP_MOTOR_A_IN2_PORT      GPIO_PORT_B
#define BSP_MOTOR_A_IN2_PIN       DL_GPIO_PIN_11

#define BSP_MOTOR_B_PWM_INST      TIMA0_INST
#define BSP_MOTOR_B_PWM_CHANNEL   1
#define BSP_MOTOR_B_IN1_PORT      GPIO_PORT_B
#define BSP_MOTOR_B_IN1_PIN       DL_GPIO_PIN_12
#define BSP_MOTOR_B_IN2_PORT      GPIO_PORT_B
#define BSP_MOTOR_B_IN2_PIN       DL_GPIO_PIN_13

/* ========== Servo Pin Definitions ========== */
#define BSP_SERVO0_PWM_INST       TIMG0_INST
#define BSP_SERVO0_PWM_CHANNEL    0
#define BSP_SERVO1_PWM_INST       TIMG0_INST
#define BSP_SERVO1_PWM_CHANNEL    1

/* ========== MPU6050 Pin Definitions (uses I2C0 bus) ========== */
#define BSP_MPU6050_I2C_INST      BSP_I2C0_INST
#define BSP_MPU6050_I2C_ADDR      0x68    /* AD0 = 0 */

/* ========== OLED Pin Definitions (uses I2C0 bus) ========== */
#define BSP_OLED_I2C_INST         BSP_I2C0_INST
#define BSP_OLED_I2C_ADDR         0x3C

/* ========== Tracer (Line Follower) Pin Definitions ========== */
/* Channel count is configured in bsp_config.h via BSP_TRACER_CHANNEL_COUNT (4~8) */
#define BSP_TRACER_CH0_PORT       GPIO_PORT_A
#define BSP_TRACER_CH0_PIN        DL_GPIO_PIN_15
#define BSP_TRACER_CH1_PORT       GPIO_PORT_A
#define BSP_TRACER_CH1_PIN        DL_GPIO_PIN_16
#define BSP_TRACER_CH2_PORT       GPIO_PORT_A
#define BSP_TRACER_CH2_PIN        DL_GPIO_PIN_17
#define BSP_TRACER_CH3_PORT       GPIO_PORT_A
#define BSP_TRACER_CH3_PIN        DL_GPIO_PIN_18
#define BSP_TRACER_CH4_PORT       GPIO_PORT_A
#define BSP_TRACER_CH4_PIN        DL_GPIO_PIN_19
#define BSP_TRACER_CH5_PORT       GPIO_PORT_A
#define BSP_TRACER_CH5_PIN        DL_GPIO_PIN_20
#define BSP_TRACER_CH6_PORT       GPIO_PORT_A
#define BSP_TRACER_CH6_PIN        DL_GPIO_PIN_21
#define BSP_TRACER_CH7_PORT       GPIO_PORT_A
#define BSP_TRACER_CH7_PIN        DL_GPIO_PIN_22

/* ========== HC-SR04 Ultrasonic Pin Definitions ========== */
#define BSP_HCSR04_TRIG_PORT      GPIO_PORT_B
#define BSP_HCSR04_TRIG_PIN       DL_GPIO_PIN_14
#define BSP_HCSR04_ECHO_PORT      GPIO_PORT_B
#define BSP_HCSR04_ECHO_PIN       DL_GPIO_PIN_15
#define BSP_HCSR04_TIMER_INST     TIMG1_INST

#endif /* BSP_PIN_CONFIG_H */
