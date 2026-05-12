/*
 * Copyright (c) 2023, Texas Instruments Incorporated - http://www.ti.com
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * *  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * *  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * *  Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 *  ============ ti_msp_dl_config.h =============
 *  Configured MSPM0 DriverLib module declarations
 *
 *  DO NOT EDIT - This file is generated for the MSPM0G350X
 *  by the SysConfig tool.
 */
#ifndef ti_msp_dl_config_h
#define ti_msp_dl_config_h

#define CONFIG_MSPM0G350X
#define CONFIG_MSPM0G3507

#if defined(__ti_version__) || defined(__TI_COMPILER_VERSION__)
#define SYSCONFIG_WEAK __attribute__((weak))
#elif defined(__IAR_SYSTEMS_ICC__)
#define SYSCONFIG_WEAK __weak
#elif defined(__GNUC__)
#define SYSCONFIG_WEAK __attribute__((weak))
#endif

#include <ti/devices/msp/msp.h>
#include <ti/driverlib/driverlib.h>
#include <ti/driverlib/m0p/dl_core.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 *  ======== SYSCFG_DL_init ========
 *  Perform all required MSP DL initialization
 *
 *  This function should be called once at a point before any use of
 *  MSP DL.
 */


/* clang-format off */

#define POWER_STARTUP_DELAY                                                (16)


#define GPIO_HFXT_PORT                                                     GPIOA
#define GPIO_HFXIN_PIN                                             DL_GPIO_PIN_5
#define GPIO_HFXIN_IOMUX                                         (IOMUX_PINCM10)
#define GPIO_HFXOUT_PIN                                            DL_GPIO_PIN_6
#define GPIO_HFXOUT_IOMUX                                        (IOMUX_PINCM11)
#define CPUCLK_FREQ                                                     80000000



/* Defines for PWM_MOTOR */
#define PWM_MOTOR_INST                                                     TIMA0
#define PWM_MOTOR_INST_IRQHandler                               TIMA0_IRQHandler
#define PWM_MOTOR_INST_INT_IRQN                                 (TIMA0_INT_IRQn)
#define PWM_MOTOR_INST_CLK_FREQ                                         20000000
/* GPIO defines for channel 0 */
#define GPIO_PWM_MOTOR_C0_PORT                                             GPIOA
#define GPIO_PWM_MOTOR_C0_PIN                                     DL_GPIO_PIN_21
#define GPIO_PWM_MOTOR_C0_IOMUX                                  (IOMUX_PINCM46)
#define GPIO_PWM_MOTOR_C0_IOMUX_FUNC                 IOMUX_PINCM46_PF_TIMA0_CCP0
#define GPIO_PWM_MOTOR_C0_IDX                                DL_TIMER_CC_0_INDEX
/* GPIO defines for channel 1 */
#define GPIO_PWM_MOTOR_C1_PORT                                             GPIOA
#define GPIO_PWM_MOTOR_C1_PIN                                     DL_GPIO_PIN_22
#define GPIO_PWM_MOTOR_C1_IOMUX                                  (IOMUX_PINCM47)
#define GPIO_PWM_MOTOR_C1_IOMUX_FUNC                 IOMUX_PINCM47_PF_TIMA0_CCP1
#define GPIO_PWM_MOTOR_C1_IDX                                DL_TIMER_CC_1_INDEX
/* GPIO defines for channel 2 */
#define GPIO_PWM_MOTOR_C2_PORT                                             GPIOA
#define GPIO_PWM_MOTOR_C2_PIN                                     DL_GPIO_PIN_15
#define GPIO_PWM_MOTOR_C2_IOMUX                                  (IOMUX_PINCM37)
#define GPIO_PWM_MOTOR_C2_IOMUX_FUNC                 IOMUX_PINCM37_PF_TIMA0_CCP2
#define GPIO_PWM_MOTOR_C2_IDX                                DL_TIMER_CC_2_INDEX
/* GPIO defines for channel 3 */
#define GPIO_PWM_MOTOR_C3_PORT                                             GPIOA
#define GPIO_PWM_MOTOR_C3_PIN                                     DL_GPIO_PIN_12
#define GPIO_PWM_MOTOR_C3_IOMUX                                  (IOMUX_PINCM34)
#define GPIO_PWM_MOTOR_C3_IOMUX_FUNC                 IOMUX_PINCM34_PF_TIMA0_CCP3
#define GPIO_PWM_MOTOR_C3_IDX                                DL_TIMER_CC_3_INDEX



/* Defines for CAPTURE_LEFT_FRONT */
#define CAPTURE_LEFT_FRONT_INST                                          (TIMG8)
#define CAPTURE_LEFT_FRONT_INST_IRQHandler                        TIMG8_IRQHandler
#define CAPTURE_LEFT_FRONT_INST_INT_IRQN                        (TIMG8_INT_IRQn)
#define CAPTURE_LEFT_FRONT_INST_LOAD_VALUE                                (65535U)
/* GPIO defines for channel 0 */
#define GPIO_CAPTURE_LEFT_FRONT_C0_PORT                                    GPIOA
#define GPIO_CAPTURE_LEFT_FRONT_C0_PIN                            DL_GPIO_PIN_26
#define GPIO_CAPTURE_LEFT_FRONT_C0_IOMUX                         (IOMUX_PINCM59)
#define GPIO_CAPTURE_LEFT_FRONT_C0_IOMUX_FUNC             IOMUX_PINCM59_PF_TIMG8_CCP0

/* Defines for CAPTURE_LEFT_BACK */
#define CAPTURE_LEFT_BACK_INST                                           (TIMG7)
#define CAPTURE_LEFT_BACK_INST_IRQHandler                        TIMG7_IRQHandler
#define CAPTURE_LEFT_BACK_INST_INT_IRQN                         (TIMG7_INT_IRQn)
#define CAPTURE_LEFT_BACK_INST_LOAD_VALUE                                (65535U)
/* GPIO defines for channel 0 */
#define GPIO_CAPTURE_LEFT_BACK_C0_PORT                                     GPIOA
#define GPIO_CAPTURE_LEFT_BACK_C0_PIN                             DL_GPIO_PIN_17
#define GPIO_CAPTURE_LEFT_BACK_C0_IOMUX                          (IOMUX_PINCM39)
#define GPIO_CAPTURE_LEFT_BACK_C0_IOMUX_FUNC             IOMUX_PINCM39_PF_TIMG7_CCP0

/* Defines for CAPTURE_RIGHT_FRONT */
#define CAPTURE_RIGHT_FRONT_INST                                         (TIMG6)
#define CAPTURE_RIGHT_FRONT_INST_IRQHandler                        TIMG6_IRQHandler
#define CAPTURE_RIGHT_FRONT_INST_INT_IRQN                        (TIMG6_INT_IRQn)
#define CAPTURE_RIGHT_FRONT_INST_LOAD_VALUE                                (65535U)
/* GPIO defines for channel 0 */
#define GPIO_CAPTURE_RIGHT_FRONT_C0_PORT                                   GPIOB
#define GPIO_CAPTURE_RIGHT_FRONT_C0_PIN                            DL_GPIO_PIN_2
#define GPIO_CAPTURE_RIGHT_FRONT_C0_IOMUX                         (IOMUX_PINCM15)
#define GPIO_CAPTURE_RIGHT_FRONT_C0_IOMUX_FUNC             IOMUX_PINCM15_PF_TIMG6_CCP0

/* Defines for CAPTURE_RIGHT_BACK */
#define CAPTURE_RIGHT_BACK_INST                                          (TIMG0)
#define CAPTURE_RIGHT_BACK_INST_IRQHandler                        TIMG0_IRQHandler
#define CAPTURE_RIGHT_BACK_INST_INT_IRQN                        (TIMG0_INT_IRQn)
#define CAPTURE_RIGHT_BACK_INST_LOAD_VALUE                                (65535U)
/* GPIO defines for channel 0 */
#define GPIO_CAPTURE_RIGHT_BACK_C0_PORT                                    GPIOA
#define GPIO_CAPTURE_RIGHT_BACK_C0_PIN                            DL_GPIO_PIN_23
#define GPIO_CAPTURE_RIGHT_BACK_C0_IOMUX                         (IOMUX_PINCM53)
#define GPIO_CAPTURE_RIGHT_BACK_C0_IOMUX_FUNC             IOMUX_PINCM53_PF_TIMG0_CCP0





/* Defines for TIMER_0 */
#define TIMER_0_INST                                                     (TIMA1)
#define TIMER_0_INST_IRQHandler                                 TIMA1_IRQHandler
#define TIMER_0_INST_INT_IRQN                                   (TIMA1_INT_IRQn)
#define TIMER_0_INST_LOAD_VALUE                                         (65535U)



/* Defines for UART_0_DEBUG */
#define UART_0_DEBUG_INST                                                  UART0
#define UART_0_DEBUG_INST_FREQUENCY                                     40000000
#define UART_0_DEBUG_INST_IRQHandler                            UART0_IRQHandler
#define UART_0_DEBUG_INST_INT_IRQN                                UART0_INT_IRQn
#define GPIO_UART_0_DEBUG_RX_PORT                                          GPIOA
#define GPIO_UART_0_DEBUG_TX_PORT                                          GPIOA
#define GPIO_UART_0_DEBUG_RX_PIN                                  DL_GPIO_PIN_11
#define GPIO_UART_0_DEBUG_TX_PIN                                  DL_GPIO_PIN_10
#define GPIO_UART_0_DEBUG_IOMUX_RX                               (IOMUX_PINCM22)
#define GPIO_UART_0_DEBUG_IOMUX_TX                               (IOMUX_PINCM21)
#define GPIO_UART_0_DEBUG_IOMUX_RX_FUNC                IOMUX_PINCM22_PF_UART0_RX
#define GPIO_UART_0_DEBUG_IOMUX_TX_FUNC                IOMUX_PINCM21_PF_UART0_TX
#define UART_0_DEBUG_BAUD_RATE                                          (115200)
#define UART_0_DEBUG_IBRD_40_MHZ_115200_BAUD                                (21)
#define UART_0_DEBUG_FBRD_40_MHZ_115200_BAUD                                (45)





/* Defines for ADC_VOLTAGE */
#define ADC_VOLTAGE_INST                                                    ADC0
#define ADC_VOLTAGE_INST_IRQHandler                              ADC0_IRQHandler
#define ADC_VOLTAGE_INST_INT_IRQN                                (ADC0_INT_IRQn)
#define ADC_VOLTAGE_ADCMEM_ADC_CH0                            DL_ADC12_MEM_IDX_0
#define ADC_VOLTAGE_ADCMEM_ADC_CH0_REF           DL_ADC12_REFERENCE_VOLTAGE_VDDA
#define ADC_VOLTAGE_ADCMEM_ADC_CH0_REF_VOLTAGE_V                                     3.3
#define GPIO_ADC_VOLTAGE_C0_PORT                                           GPIOA
#define GPIO_ADC_VOLTAGE_C0_PIN                                   DL_GPIO_PIN_27



/* Port definition for Pin Group LED */
#define LED_PORT                                                         (GPIOA)

/* Defines for A14: GPIOA.14 with pinCMx 36 on package pin 29 */
#define LED_A14_PIN                                             (DL_GPIO_PIN_14)
#define LED_A14_IOMUX                                            (IOMUX_PINCM36)
/* Port definition for Pin Group ENCODER */
#define ENCODER_PORT                                                     (GPIOA)

/* Defines for LEFT_FRONT_B: GPIOA.28 with pinCMx 3 on package pin 3 */
#define ENCODER_LEFT_FRONT_B_PIN                                (DL_GPIO_PIN_28)
#define ENCODER_LEFT_FRONT_B_IOMUX                                (IOMUX_PINCM3)
/* Defines for LEFT_BACK_B: GPIOA.3 with pinCMx 8 on package pin 9 */
#define ENCODER_LEFT_BACK_B_PIN                                  (DL_GPIO_PIN_3)
#define ENCODER_LEFT_BACK_B_IOMUX                                 (IOMUX_PINCM8)
/* Defines for RIGHT_FRONT_B: GPIOA.2 with pinCMx 7 on package pin 8 */
#define ENCODER_RIGHT_FRONT_B_PIN                                (DL_GPIO_PIN_2)
#define ENCODER_RIGHT_FRONT_B_IOMUX                               (IOMUX_PINCM7)
/* Defines for RIGHT_BACK_B: GPIOA.31 with pinCMx 6 on package pin 5 */
#define ENCODER_RIGHT_BACK_B_PIN                                (DL_GPIO_PIN_31)
#define ENCODER_RIGHT_BACK_B_IOMUX                                (IOMUX_PINCM6)
/* Defines for AIN1: GPIOB.20 with pinCMx 48 on package pin 41 */
#define MOTOR_AIN1_PORT                                                  (GPIOB)
#define MOTOR_AIN1_PIN                                          (DL_GPIO_PIN_20)
#define MOTOR_AIN1_IOMUX                                         (IOMUX_PINCM48)
/* Defines for AIN2: GPIOB.24 with pinCMx 52 on package pin 42 */
#define MOTOR_AIN2_PORT                                                  (GPIOB)
#define MOTOR_AIN2_PIN                                          (DL_GPIO_PIN_24)
#define MOTOR_AIN2_IOMUX                                         (IOMUX_PINCM52)
/* Defines for BIN1: GPIOA.4 with pinCMx 9 on package pin 10 */
#define MOTOR_BIN1_PORT                                                  (GPIOA)
#define MOTOR_BIN1_PIN                                           (DL_GPIO_PIN_4)
#define MOTOR_BIN1_IOMUX                                          (IOMUX_PINCM9)
/* Defines for BIN2: GPIOA.7 with pinCMx 14 on package pin 13 */
#define MOTOR_BIN2_PORT                                                  (GPIOA)
#define MOTOR_BIN2_PIN                                           (DL_GPIO_PIN_7)
#define MOTOR_BIN2_IOMUX                                         (IOMUX_PINCM14)
/* Defines for CIN1: GPIOB.3 with pinCMx 16 on package pin 15 */
#define MOTOR_CIN1_PORT                                                  (GPIOB)
#define MOTOR_CIN1_PIN                                           (DL_GPIO_PIN_3)
#define MOTOR_CIN1_IOMUX                                         (IOMUX_PINCM16)
/* Defines for CIN2: GPIOA.8 with pinCMx 19 on package pin 16 */
#define MOTOR_CIN2_PORT                                                  (GPIOA)
#define MOTOR_CIN2_PIN                                           (DL_GPIO_PIN_8)
#define MOTOR_CIN2_IOMUX                                         (IOMUX_PINCM19)
/* Defines for DIN1: GPIOA.9 with pinCMx 20 on package pin 17 */
#define MOTOR_DIN1_PORT                                                  (GPIOA)
#define MOTOR_DIN1_PIN                                           (DL_GPIO_PIN_9)
#define MOTOR_DIN1_IOMUX                                         (IOMUX_PINCM20)
/* Defines for DIN2: GPIOB.6 with pinCMx 23 on package pin 20 */
#define MOTOR_DIN2_PORT                                                  (GPIOB)
#define MOTOR_DIN2_PIN                                           (DL_GPIO_PIN_6)
#define MOTOR_DIN2_IOMUX                                         (IOMUX_PINCM23)
/* Port definition for Pin Group IIC */
#define IIC_PORT                                                         (GPIOA)

/* Defines for SCL: GPIOA.0 with pinCMx 1 on package pin 1 */
#define IIC_SCL_PIN                                              (DL_GPIO_PIN_0)
#define IIC_SCL_IOMUX                                             (IOMUX_PINCM1)
/* Defines for SDA: GPIOA.1 with pinCMx 2 on package pin 2 */
#define IIC_SDA_PIN                                              (DL_GPIO_PIN_1)
#define IIC_SDA_IOMUX                                             (IOMUX_PINCM2)

/* clang-format on */

void SYSCFG_DL_init(void);
void SYSCFG_DL_initPower(void);
void SYSCFG_DL_GPIO_init(void);
void SYSCFG_DL_SYSCTL_init(void);
void SYSCFG_DL_SYSCTL_CLK_init(void);
void SYSCFG_DL_PWM_MOTOR_init(void);
void SYSCFG_DL_CAPTURE_LEFT_FRONT_init(void);
void SYSCFG_DL_CAPTURE_LEFT_BACK_init(void);
void SYSCFG_DL_CAPTURE_RIGHT_FRONT_init(void);
void SYSCFG_DL_CAPTURE_RIGHT_BACK_init(void);
void SYSCFG_DL_TIMER_0_init(void);
void SYSCFG_DL_UART_0_DEBUG_init(void);
void SYSCFG_DL_ADC_VOLTAGE_init(void);


bool SYSCFG_DL_saveConfiguration(void);
bool SYSCFG_DL_restoreConfiguration(void);

#ifdef __cplusplus
}
#endif

#endif /* ti_msp_dl_config_h */
