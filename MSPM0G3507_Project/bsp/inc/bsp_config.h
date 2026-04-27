/**
 * @file    bsp_config.h
 * @brief   BSP global configuration (clock frequencies, default parameters)
 */

#ifndef BSP_CONFIG_H
#define BSP_CONFIG_H

/* ========== System Clock ========== */
#define BSP_SYSCLK_FREQ_HZ     80000000UL   /* 80 MHz MCLK */
#define BSP_BUSCLK_FREQ_HZ     80000000UL   /* 80 MHz BUSCLK */
#define BSP_LFCLK_FREQ_HZ      32768UL      /* 32.768 kHz LFCLK */

/* ========== UART Defaults ========== */
#define BSP_UART_DEFAULT_BAUDRATE   115200
#define BSP_UART_TX_BUFFER_SIZE    256
#define BSP_UART_RX_BUFFER_SIZE    256

/* ========== SPI Defaults ========== */
#define BSP_SPI0_CLOCK_FREQ_HZ     4000000   /* 4 MHz SPI clock */

/* ========== I2C Defaults ========== */
#define BSP_I2C0_SPEED             400000    /* 400 kHz Fast mode */

/* ========== ADC Defaults ========== */
#define BSP_ADC_VREF_MV            3300      /* VDD = 3.3V reference */

/* ========== Motor Defaults ========== */
#define BSP_MOTOR_PWM_FREQ_HZ      10000     /* 10 kHz PWM frequency */
#define BSP_MOTOR_COUNT            2         /* Number of motors (A, B) */

/* ========== Servo Defaults ========== */
#define BSP_SERVO_PWM_FREQ_HZ      50        /* 50 Hz (20 ms period) */
#define BSP_SERVO_MIN_PULSE_US     500       /* 0.5 ms → 0° */
#define BSP_SERVO_MAX_PULSE_US     2500      /* 2.5 ms → 180° */
#define BSP_SERVO_COUNT            2         /* Number of servo channels */

/* ========== MPU6050 Defaults ========== */
#define BSP_MPU6050_ACCEL_RANGE    0         /* 0: ±2g, 1: ±4g, 2: ±8g, 3: ±16g */
#define BSP_MPU6050_GYRO_RANGE     0         /* 0: ±250, 1: ±500, 2: ±1000, 3: ±2000 °/s */
#define BSP_MPU6050_TIMEOUT_MS     100       /* I2C timeout */

/* ========== OLED Defaults ========== */
#define BSP_OLED_WIDTH             128
#define BSP_OLED_HEIGHT            64
#define BSP_OLED_TIMEOUT_MS        100       /* I2C timeout */

/* ========== Tracer Defaults ========== */
#define BSP_TRACER_CHANNEL_COUNT   4         /* Number of IR sensor channels */

/* ========== HC-SR04 Defaults ========== */
#define BSP_HCSR04_TIMEOUT_MS      30        /* Echo timeout (max ~5m) */
#define BSP_HCSR04_TRIG_PULSE_US   10        /* Trigger pulse width */

/* ========== Board Identification ========== */
#define BSP_BOARD_NAME             "MSPM0G3507_LP"
#define BSP_BOARD_REVISION         "1.0"

/* ========== BSP Module Enable ========== */
#define BSP_USE_MOTOR              0         /* Enable motor driver */
#define BSP_USE_SERVO              0         /* Enable servo driver */
#define BSP_USE_MPU6050            0         /* Enable MPU6050 driver */
#define BSP_USE_OLED               0         /* Enable OLED driver */
#define BSP_USE_TRACER             0         /* Enable tracer driver */
#define BSP_USE_HCSR04             0         /* Enable HC-SR04 driver */

#endif /* BSP_CONFIG_H */
