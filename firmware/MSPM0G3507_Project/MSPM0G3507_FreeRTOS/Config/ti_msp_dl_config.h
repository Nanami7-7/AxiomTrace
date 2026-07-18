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
/* Defines for SYSPLL_ERR_01 Workaround */
/* Represent 1.000 as 1000 */
#define FLOAT_TO_INT_SCALE                                               (1000U)
#define FCC_EXPECTED_RATIO                                                  2500
#define FCC_UPPER_BOUND                       (FCC_EXPECTED_RATIO * (1 + 0.003))
#define FCC_LOWER_BOUND                       (FCC_EXPECTED_RATIO * (1 - 0.003))

bool SYSCFG_DL_SYSCTL_SYSPLL_init(void);


/* Defines for PWM_MOTOR */
#define PWM_MOTOR_INST                                                     TIMA0
#define PWM_MOTOR_INST_IRQHandler                               TIMA0_IRQHandler
#define PWM_MOTOR_INST_INT_IRQN                                 (TIMA0_INT_IRQn)
#define PWM_MOTOR_INST_CLK_FREQ                                         20000000
/* GPIO defines for channel 0 */
#define GPIO_PWM_MOTOR_C0_PORT                                             GPIOA
#define GPIO_PWM_MOTOR_C0_PIN                                      DL_GPIO_PIN_8
#define GPIO_PWM_MOTOR_C0_IOMUX                                  (IOMUX_PINCM19)
#define GPIO_PWM_MOTOR_C0_IOMUX_FUNC                 IOMUX_PINCM19_PF_TIMA0_CCP0
#define GPIO_PWM_MOTOR_C0_IDX                                DL_TIMER_CC_0_INDEX
/* GPIO defines for channel 1 */
#define GPIO_PWM_MOTOR_C1_PORT                                             GPIOA
#define GPIO_PWM_MOTOR_C1_PIN                                      DL_GPIO_PIN_9
#define GPIO_PWM_MOTOR_C1_IOMUX                                  (IOMUX_PINCM20)
#define GPIO_PWM_MOTOR_C1_IOMUX_FUNC                 IOMUX_PINCM20_PF_TIMA0_CCP1
#define GPIO_PWM_MOTOR_C1_IDX                                DL_TIMER_CC_1_INDEX
/* GPIO defines for channel 2 */
#define GPIO_PWM_MOTOR_C2_PORT                                             GPIOB
#define GPIO_PWM_MOTOR_C2_PIN                                     DL_GPIO_PIN_17
#define GPIO_PWM_MOTOR_C2_IOMUX                                  (IOMUX_PINCM43)
#define GPIO_PWM_MOTOR_C2_IOMUX_FUNC                 IOMUX_PINCM43_PF_TIMA0_CCP2
#define GPIO_PWM_MOTOR_C2_IDX                                DL_TIMER_CC_2_INDEX
/* GPIO defines for channel 3 */
#define GPIO_PWM_MOTOR_C3_PORT                                             GPIOB
#define GPIO_PWM_MOTOR_C3_PIN                                      DL_GPIO_PIN_2
#define GPIO_PWM_MOTOR_C3_IOMUX                                  (IOMUX_PINCM15)
#define GPIO_PWM_MOTOR_C3_IOMUX_FUNC                 IOMUX_PINCM15_PF_TIMA0_CCP3
#define GPIO_PWM_MOTOR_C3_IDX                                DL_TIMER_CC_3_INDEX



/* Defines for M3 */
#define M3_INST                                                          (TIMG7)
#define M3_INST_IRQHandler                                      TIMG7_IRQHandler
#define M3_INST_INT_IRQN                                        (TIMG7_INT_IRQn)
#define M3_INST_LOAD_VALUE                                              (65535U)
/* GPIO defines for channel 0 */
#define GPIO_M3_C0_PORT                                                    GPIOA
#define GPIO_M3_C0_PIN                                            DL_GPIO_PIN_26
#define GPIO_M3_C0_IOMUX                                         (IOMUX_PINCM59)
#define GPIO_M3_C0_IOMUX_FUNC                        IOMUX_PINCM59_PF_TIMG7_CCP0

/* Defines for M4 */
#define M4_INST                                                          (TIMA1)
#define M4_INST_IRQHandler                                      TIMA1_IRQHandler
#define M4_INST_INT_IRQN                                        (TIMA1_INT_IRQn)
#define M4_INST_LOAD_VALUE                                              (65535U)
/* GPIO defines for channel 0 */
#define GPIO_M4_C0_PORT                                                    GPIOA
#define GPIO_M4_C0_PIN                                            DL_GPIO_PIN_28
#define GPIO_M4_C0_IOMUX                                          (IOMUX_PINCM3)
#define GPIO_M4_C0_IOMUX_FUNC                         IOMUX_PINCM3_PF_TIMA1_CCP0

/* Defines for M2 */
#define M2_INST                                                          (TIMG6)
#define M2_INST_IRQHandler                                      TIMG6_IRQHandler
#define M2_INST_INT_IRQN                                        (TIMG6_INT_IRQn)
#define M2_INST_LOAD_VALUE                                              (65535U)
/* GPIO defines for channel 0 */
#define GPIO_M2_C0_PORT                                                    GPIOA
#define GPIO_M2_C0_PIN                                            DL_GPIO_PIN_21
#define GPIO_M2_C0_IOMUX                                         (IOMUX_PINCM46)
#define GPIO_M2_C0_IOMUX_FUNC                        IOMUX_PINCM46_PF_TIMG6_CCP0

/* Defines for M1 */
#define M1_INST                                                          (TIMG0)
#define M1_INST_IRQHandler                                      TIMG0_IRQHandler
#define M1_INST_INT_IRQN                                        (TIMG0_INT_IRQn)
#define M1_INST_LOAD_VALUE                                              (65535U)
/* GPIO defines for channel 0 */
#define GPIO_M1_C0_PORT                                                    GPIOA
#define GPIO_M1_C0_PIN                                            DL_GPIO_PIN_12
#define GPIO_M1_C0_IOMUX                                         (IOMUX_PINCM34)
#define GPIO_M1_C0_IOMUX_FUNC                        IOMUX_PINCM34_PF_TIMG0_CCP0





/* Defines for TIMER_0 */
#define TIMER_0_INST                                                     (TIMG8)
#define TIMER_0_INST_IRQHandler                                 TIMG8_IRQHandler
#define TIMER_0_INST_INT_IRQN                                   (TIMG8_INT_IRQn)
#define TIMER_0_INST_LOAD_VALUE                                         (32767U)



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




/* Defines for SPI_0 */
#define SPI_0_INST                                                         SPI1
#define SPI_0_INST_IRQHandler                                   SPI1_IRQHandler
#define SPI_0_INST_INT_IRQN                                       SPI1_INT_IRQn
#define GPIO_SPI_0_PICO_PORT                                              GPIOB
#define GPIO_SPI_0_PICO_PIN                                      DL_GPIO_PIN_15
#define GPIO_SPI_0_IOMUX_PICO                                   (IOMUX_PINCM32)
#define GPIO_SPI_0_IOMUX_PICO_FUNC                   IOMUX_PINCM32_PF_SPI1_PICO
#define GPIO_SPI_0_POCI_PORT                                              GPIOB
#define GPIO_SPI_0_POCI_PIN                                      DL_GPIO_PIN_14
#define GPIO_SPI_0_IOMUX_POCI                                   (IOMUX_PINCM31)
#define GPIO_SPI_0_IOMUX_POCI_FUNC                   IOMUX_PINCM31_PF_SPI1_POCI
/* GPIO configuration for SPI_0 */
#define GPIO_SPI_0_SCLK_PORT                                              GPIOB
#define GPIO_SPI_0_SCLK_PIN                                      DL_GPIO_PIN_16
#define GPIO_SPI_0_IOMUX_SCLK                                   (IOMUX_PINCM33)
#define GPIO_SPI_0_IOMUX_SCLK_FUNC                   IOMUX_PINCM33_PF_SPI1_SCLK
#define GPIO_SPI_0_CS0_PORT                                               GPIOA
#define GPIO_SPI_0_CS0_PIN                                        DL_GPIO_PIN_2
#define GPIO_SPI_0_IOMUX_CS0                                     (IOMUX_PINCM7)
#define GPIO_SPI_0_IOMUX_CS0_FUNC                      IOMUX_PINCM7_PF_SPI1_CS0



/* Defines for ADC_VOLTAGE */
#define ADC_VOLTAGE_INST                                                    ADC1
#define ADC_VOLTAGE_INST_IRQHandler                              ADC1_IRQHandler
#define ADC_VOLTAGE_INST_INT_IRQN                                (ADC1_INT_IRQn)
#define ADC_VOLTAGE_ADCMEM_ADC_CH0                            DL_ADC12_MEM_IDX_0
#define ADC_VOLTAGE_ADCMEM_ADC_CH0_REF           DL_ADC12_REFERENCE_VOLTAGE_VDDA
#define ADC_VOLTAGE_ADCMEM_ADC_CH0_REF_VOLTAGE_V                                     3.3
#define ADC_VOLTAGE_ADCMEM_1                                  DL_ADC12_MEM_IDX_1
#define ADC_VOLTAGE_ADCMEM_1_REF                 DL_ADC12_REFERENCE_VOLTAGE_VDDA
#define ADC_VOLTAGE_ADCMEM_1_REF_VOLTAGE_V                                     3.3
#define ADC_VOLTAGE_ADCMEM_2                                  DL_ADC12_MEM_IDX_2
#define ADC_VOLTAGE_ADCMEM_2_REF                 DL_ADC12_REFERENCE_VOLTAGE_VDDA
#define ADC_VOLTAGE_ADCMEM_2_REF_VOLTAGE_V                                     3.3
#define ADC_VOLTAGE_ADCMEM_3                                  DL_ADC12_MEM_IDX_3
#define ADC_VOLTAGE_ADCMEM_3_REF                 DL_ADC12_REFERENCE_VOLTAGE_VDDA
#define ADC_VOLTAGE_ADCMEM_3_REF_VOLTAGE_V                                     3.3
#define ADC_VOLTAGE_ADCMEM_4                                  DL_ADC12_MEM_IDX_4
#define ADC_VOLTAGE_ADCMEM_4_REF                 DL_ADC12_REFERENCE_VOLTAGE_VDDA
#define ADC_VOLTAGE_ADCMEM_4_REF_VOLTAGE_V                                     3.3
#define GPIO_ADC_VOLTAGE_C0_PORT                                           GPIOA
#define GPIO_ADC_VOLTAGE_C0_PIN                                   DL_GPIO_PIN_15
#define GPIO_ADC_VOLTAGE_IOMUX_C0                                (IOMUX_PINCM37)
#define GPIO_ADC_VOLTAGE_IOMUX_C0_FUNC            (IOMUX_PINCM37_PF_UNCONNECTED)
#define GPIO_ADC_VOLTAGE_C1_PORT                                           GPIOA
#define GPIO_ADC_VOLTAGE_C1_PIN                                   DL_GPIO_PIN_16
#define GPIO_ADC_VOLTAGE_IOMUX_C1                                (IOMUX_PINCM38)
#define GPIO_ADC_VOLTAGE_IOMUX_C1_FUNC            (IOMUX_PINCM38_PF_UNCONNECTED)
#define GPIO_ADC_VOLTAGE_C2_PORT                                           GPIOA
#define GPIO_ADC_VOLTAGE_C2_PIN                                   DL_GPIO_PIN_17
#define GPIO_ADC_VOLTAGE_IOMUX_C2                                (IOMUX_PINCM39)
#define GPIO_ADC_VOLTAGE_IOMUX_C2_FUNC            (IOMUX_PINCM39_PF_UNCONNECTED)
#define GPIO_ADC_VOLTAGE_C8_PORT                                           GPIOA
#define GPIO_ADC_VOLTAGE_C8_PIN                                   DL_GPIO_PIN_22
#define GPIO_ADC_VOLTAGE_IOMUX_C8                                (IOMUX_PINCM47)
#define GPIO_ADC_VOLTAGE_IOMUX_C8_FUNC            (IOMUX_PINCM47_PF_UNCONNECTED)



/* Defines for DMA_CH1 */
#define DMA_CH1_CHAN_ID                                                      (0)
#define UART_0_DEBUG_INST_DMA_TRIGGER                        (DMA_UART0_TX_TRIG)


/* Port definition for Pin Group LED */
#define LED_PORT                                                         (GPIOA)

/* Defines for A27: GPIOA.27 with pinCMx 60 on package pin 47 */
#define LED_A27_PIN                                             (DL_GPIO_PIN_27)
#define LED_A27_IOMUX                                            (IOMUX_PINCM60)
/* Port definition for Pin Group spi_cs */
#define spi_cs_PORT                                                      (GPIOB)

/* Defines for PIN_0: GPIOB.9 with pinCMx 26 on package pin 23 */
#define spi_cs_PIN_0_PIN                                         (DL_GPIO_PIN_9)
#define spi_cs_PIN_0_IOMUX                                       (IOMUX_PINCM26)
/* Port definition for Pin Group POWER */
#define POWER_PORT                                                       (GPIOB)

/* Defines for pb19: GPIOB.19 with pinCMx 45 on package pin 38 */
#define POWER_pb19_PIN                                          (DL_GPIO_PIN_19)
#define POWER_pb19_IOMUX                                         (IOMUX_PINCM45)
/* Port definition for Pin Group ENCODER */
#define ENCODER_PORT                                                     (GPIOA)

/* Defines for M1_B: GPIOA.13 with pinCMx 35 on package pin 28 */
#define ENCODER_M1_B_PIN                                        (DL_GPIO_PIN_13)
#define ENCODER_M1_B_IOMUX                                       (IOMUX_PINCM35)
/* Defines for M2_B: GPIOA.14 with pinCMx 36 on package pin 29 */
#define ENCODER_M2_B_PIN                                        (DL_GPIO_PIN_14)
#define ENCODER_M2_B_IOMUX                                       (IOMUX_PINCM36)
/* Defines for M3_B: GPIOA.25 with pinCMx 55 on package pin 45 */
#define ENCODER_M3_B_PIN                                        (DL_GPIO_PIN_25)
#define ENCODER_M3_B_IOMUX                                       (IOMUX_PINCM55)
/* Defines for M4_B: GPIOA.4 with pinCMx 9 on package pin 10 */
#define ENCODER_M4_B_PIN                                         (DL_GPIO_PIN_4)
#define ENCODER_M4_B_IOMUX                                        (IOMUX_PINCM9)
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

bool SYSCFG_DL_SYSCTL_SYSPLL_init(void);
void SYSCFG_DL_PWM_MOTOR_init(void);
void SYSCFG_DL_M3_init(void);
void SYSCFG_DL_M4_init(void);
void SYSCFG_DL_M2_init(void);
void SYSCFG_DL_M1_init(void);
void SYSCFG_DL_TIMER_0_init(void);
void SYSCFG_DL_UART_0_DEBUG_init(void);
void SYSCFG_DL_SPI_0_init(void);
void SYSCFG_DL_ADC_VOLTAGE_init(void);
void SYSCFG_DL_DMA_init(void);


bool SYSCFG_DL_saveConfiguration(void);
bool SYSCFG_DL_restoreConfiguration(void);

#ifdef __cplusplus
}
#endif

#endif /* ti_msp_dl_config_h */
