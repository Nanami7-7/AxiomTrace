/*
 * Copyright (c) 2023, Texas Instruments Incorporated
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
 *  ============ ti_msp_dl_config.c =============
 *  Configured MSPM0 DriverLib module definitions
 *
 *  DO NOT EDIT - This file is generated for the MSPM0G350X
 *  by the SysConfig tool.
 */

#include "ti_msp_dl_config.h"

DL_TimerA_backupConfig gPWM_MOTORBackup;
DL_TimerG_backupConfig gM3Backup;
DL_TimerA_backupConfig gM4Backup;
DL_TimerG_backupConfig gM2Backup;
DL_SPI_backupConfig gSPI_0Backup;

/*
 *  ======== SYSCFG_DL_init ========
 *  Perform any initialization needed before using any board APIs
 */
SYSCONFIG_WEAK void SYSCFG_DL_init(void)
{
    SYSCFG_DL_initPower();
    SYSCFG_DL_GPIO_init();
    /* Module-Specific Initializations*/
    SYSCFG_DL_SYSCTL_init();
    SYSCFG_DL_PWM_MOTOR_init();
    SYSCFG_DL_M3_init();
    SYSCFG_DL_M4_init();
    SYSCFG_DL_M2_init();
    SYSCFG_DL_M1_init();
    SYSCFG_DL_TIMER_0_init();
    SYSCFG_DL_UART_0_DEBUG_init();
    SYSCFG_DL_SPI_0_init();
    SYSCFG_DL_ADC_VOLTAGE_init();
    SYSCFG_DL_DMA_init();
    SYSCFG_DL_SYSCTL_CLK_init();
    /* Ensure backup structures have no valid state */
	gPWM_MOTORBackup.backupRdy 	= false;
	gM3Backup.backupRdy 	= false;
	gM4Backup.backupRdy 	= false;
	gM2Backup.backupRdy 	= false;


	gSPI_0Backup.backupRdy 	= false;

}
/*
 * User should take care to save and restore register configuration in application.
 * See Retention Configuration section for more details.
 */
SYSCONFIG_WEAK bool SYSCFG_DL_saveConfiguration(void)
{
    bool retStatus = true;

	retStatus &= DL_TimerA_saveConfiguration(PWM_MOTOR_INST, &gPWM_MOTORBackup);
	retStatus &= DL_TimerG_saveConfiguration(M3_INST, &gM3Backup);
	retStatus &= DL_TimerA_saveConfiguration(M4_INST, &gM4Backup);
	retStatus &= DL_TimerG_saveConfiguration(M2_INST, &gM2Backup);
	retStatus &= DL_SPI_saveConfiguration(SPI_0_INST, &gSPI_0Backup);

    return retStatus;
}


SYSCONFIG_WEAK bool SYSCFG_DL_restoreConfiguration(void)
{
    bool retStatus = true;

	retStatus &= DL_TimerA_restoreConfiguration(PWM_MOTOR_INST, &gPWM_MOTORBackup, false);
	retStatus &= DL_TimerG_restoreConfiguration(M3_INST, &gM3Backup, false);
	retStatus &= DL_TimerA_restoreConfiguration(M4_INST, &gM4Backup, false);
	retStatus &= DL_TimerG_restoreConfiguration(M2_INST, &gM2Backup, false);
	retStatus &= DL_SPI_restoreConfiguration(SPI_0_INST, &gSPI_0Backup);

    return retStatus;
}

SYSCONFIG_WEAK void SYSCFG_DL_initPower(void)
{
    DL_GPIO_reset(GPIOA);
    DL_GPIO_reset(GPIOB);
    DL_TimerA_reset(PWM_MOTOR_INST);
    DL_TimerG_reset(M3_INST);
    DL_TimerA_reset(M4_INST);
    DL_TimerG_reset(M2_INST);
    DL_TimerG_reset(M1_INST);
    DL_TimerG_reset(TIMER_0_INST);
    DL_UART_Main_reset(UART_0_DEBUG_INST);
    DL_SPI_reset(SPI_0_INST);
    DL_ADC12_reset(ADC_VOLTAGE_INST);


    DL_GPIO_enablePower(GPIOA);
    DL_GPIO_enablePower(GPIOB);
    DL_TimerA_enablePower(PWM_MOTOR_INST);
    DL_TimerG_enablePower(M3_INST);
    DL_TimerA_enablePower(M4_INST);
    DL_TimerG_enablePower(M2_INST);
    DL_TimerG_enablePower(M1_INST);
    DL_TimerG_enablePower(TIMER_0_INST);
    DL_UART_Main_enablePower(UART_0_DEBUG_INST);
    DL_SPI_enablePower(SPI_0_INST);
    DL_ADC12_enablePower(ADC_VOLTAGE_INST);

    delay_cycles(POWER_STARTUP_DELAY);
}

SYSCONFIG_WEAK void SYSCFG_DL_GPIO_init(void)
{

    DL_GPIO_initPeripheralAnalogFunction(GPIO_HFXIN_IOMUX);
    DL_GPIO_initPeripheralAnalogFunction(GPIO_HFXOUT_IOMUX);

    DL_GPIO_initPeripheralOutputFunction(GPIO_PWM_MOTOR_C0_IOMUX,GPIO_PWM_MOTOR_C0_IOMUX_FUNC);
    DL_GPIO_enableOutput(GPIO_PWM_MOTOR_C0_PORT, GPIO_PWM_MOTOR_C0_PIN);
    DL_GPIO_initPeripheralOutputFunction(GPIO_PWM_MOTOR_C1_IOMUX,GPIO_PWM_MOTOR_C1_IOMUX_FUNC);
    DL_GPIO_enableOutput(GPIO_PWM_MOTOR_C1_PORT, GPIO_PWM_MOTOR_C1_PIN);
    DL_GPIO_initPeripheralOutputFunction(GPIO_PWM_MOTOR_C2_IOMUX,GPIO_PWM_MOTOR_C2_IOMUX_FUNC);
    DL_GPIO_enableOutput(GPIO_PWM_MOTOR_C2_PORT, GPIO_PWM_MOTOR_C2_PIN);
    DL_GPIO_initPeripheralOutputFunction(GPIO_PWM_MOTOR_C3_IOMUX,GPIO_PWM_MOTOR_C3_IOMUX_FUNC);
    DL_GPIO_enableOutput(GPIO_PWM_MOTOR_C3_PORT, GPIO_PWM_MOTOR_C3_PIN);

    DL_GPIO_initPeripheralInputFunction(GPIO_M3_C0_IOMUX,GPIO_M3_C0_IOMUX_FUNC);
    DL_GPIO_initPeripheralInputFunction(GPIO_M4_C0_IOMUX,GPIO_M4_C0_IOMUX_FUNC);
    DL_GPIO_initPeripheralInputFunction(GPIO_M2_C0_IOMUX,GPIO_M2_C0_IOMUX_FUNC);
    DL_GPIO_initPeripheralInputFunction(GPIO_M1_C0_IOMUX,GPIO_M1_C0_IOMUX_FUNC);

    DL_GPIO_initPeripheralOutputFunction(
        GPIO_UART_0_DEBUG_IOMUX_TX, GPIO_UART_0_DEBUG_IOMUX_TX_FUNC);
    DL_GPIO_initPeripheralInputFunction(
        GPIO_UART_0_DEBUG_IOMUX_RX, GPIO_UART_0_DEBUG_IOMUX_RX_FUNC);

    DL_GPIO_initPeripheralOutputFunction(
        GPIO_SPI_0_IOMUX_SCLK, GPIO_SPI_0_IOMUX_SCLK_FUNC);
    DL_GPIO_initPeripheralOutputFunction(
        GPIO_SPI_0_IOMUX_PICO, GPIO_SPI_0_IOMUX_PICO_FUNC);
    DL_GPIO_initPeripheralInputFunction(
        GPIO_SPI_0_IOMUX_POCI, GPIO_SPI_0_IOMUX_POCI_FUNC);
    DL_GPIO_initPeripheralOutputFunction(
        GPIO_SPI_0_IOMUX_CS0, GPIO_SPI_0_IOMUX_CS0_FUNC);

    DL_GPIO_initDigitalOutputFeatures(LED_A27_IOMUX,
		 DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_PULL_DOWN,
		 DL_GPIO_DRIVE_STRENGTH_LOW, DL_GPIO_HIZ_DISABLE);

    DL_GPIO_initDigitalOutput(spi_cs_PIN_0_IOMUX);

    DL_GPIO_initDigitalOutputFeatures(POWER_pb19_IOMUX,
		 DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_PULL_DOWN,
		 DL_GPIO_DRIVE_STRENGTH_LOW, DL_GPIO_HIZ_DISABLE);

    DL_GPIO_initDigitalInputFeatures(ENCODER_M1_B_IOMUX,
		 DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_PULL_UP,
		 DL_GPIO_HYSTERESIS_DISABLE, DL_GPIO_WAKEUP_DISABLE);

    DL_GPIO_initDigitalInputFeatures(ENCODER_M2_B_IOMUX,
		 DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_PULL_UP,
		 DL_GPIO_HYSTERESIS_DISABLE, DL_GPIO_WAKEUP_DISABLE);

    DL_GPIO_initDigitalInputFeatures(ENCODER_M3_B_IOMUX,
		 DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_PULL_UP,
		 DL_GPIO_HYSTERESIS_DISABLE, DL_GPIO_WAKEUP_DISABLE);

    DL_GPIO_initDigitalInputFeatures(ENCODER_M4_B_IOMUX,
		 DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_PULL_UP,
		 DL_GPIO_HYSTERESIS_DISABLE, DL_GPIO_WAKEUP_DISABLE);

    DL_GPIO_initDigitalOutputFeatures(IIC_SCL_IOMUX,
		 DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_PULL_UP,
		 DL_GPIO_DRIVE_STRENGTH_LOW, DL_GPIO_HIZ_DISABLE);

    DL_GPIO_initDigitalOutputFeatures(IIC_SDA_IOMUX,
		 DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_PULL_UP,
		 DL_GPIO_DRIVE_STRENGTH_LOW, DL_GPIO_HIZ_DISABLE);

    DL_GPIO_clearPins(GPIOA, LED_A27_PIN);
    DL_GPIO_setPins(GPIOA, IIC_SCL_PIN |
		IIC_SDA_PIN);
    DL_GPIO_enableOutput(GPIOA, LED_A27_PIN |
		IIC_SCL_PIN |
		IIC_SDA_PIN);
    DL_GPIO_clearPins(GPIOB, spi_cs_PIN_0_PIN |
		POWER_pb19_PIN);
    DL_GPIO_enableOutput(GPIOB, spi_cs_PIN_0_PIN |
		POWER_pb19_PIN);

}


static const DL_SYSCTL_SYSPLLConfig gSYSPLLConfig = {
    .inputFreq              = DL_SYSCTL_SYSPLL_INPUT_FREQ_32_48_MHZ,
	.rDivClk2x              = 1,
	.rDivClk1               = 0,
	.rDivClk0               = 0,
	.enableCLK2x            = DL_SYSCTL_SYSPLL_CLK2X_DISABLE,
	.enableCLK1             = DL_SYSCTL_SYSPLL_CLK1_ENABLE,
	.enableCLK0             = DL_SYSCTL_SYSPLL_CLK0_ENABLE,
	.sysPLLMCLK             = DL_SYSCTL_SYSPLL_MCLK_CLK0,
	.sysPLLRef              = DL_SYSCTL_SYSPLL_REF_SYSOSC,
	.qDiv                   = 4,
	.pDiv                   = DL_SYSCTL_SYSPLL_PDIV_1
};

SYSCONFIG_WEAK bool SYSCFG_DL_SYSCTL_SYSPLL_init(void)
{
    bool fFCCRatioStatus = false;
    uint32_t fFCCSysoscCount;
    uint32_t fFCCPllCount;
    uint32_t fFCCRatio;
    uint32_t fccTimeOutCounter;

    DL_SYSCTL_setFCCPeriods( DL_SYSCTL_FCC_TRIG_CNT_01 );

    /* Measuring PLL. */
    DL_SYSCTL_configFCC(DL_SYSCTL_FCC_TRIG_TYPE_RISE_RISE,
                        DL_SYSCTL_FCC_TRIG_SOURCE_LFCLK,
                        DL_SYSCTL_FCC_CLOCK_SOURCE_SYSPLLCLK0);
    /* Get SYSPLL frequency using FCC */
    fccTimeOutCounter = 0;
    DL_SYSCTL_startFCC();
    while (DL_SYSCTL_isFCCDone() == 0) {
        delay_cycles(977);  /* 1x LFCLK cycle = 32MHz/32.768kHz = 977, 30.5us */
        fccTimeOutCounter++;
        if(fccTimeOutCounter > 65){
            /* Timeout set to approximately 2ms (user-customizable) */
            break;
        }
    }

    /* get measA= SYSPLLCLK0 freq wrt LFOSC*/
    fFCCPllCount = DL_SYSCTL_readFCC();

    /* Measuring SYSPLL Source */
    DL_SYSCTL_configFCC(DL_SYSCTL_FCC_TRIG_TYPE_RISE_RISE,
                        DL_SYSCTL_FCC_TRIG_SOURCE_LFCLK,
                        DL_SYSCTL_FCC_CLOCK_SOURCE_SYSOSC);
    /* Get SYSPLL frequency using FCC */
    fccTimeOutCounter = 0;
    DL_SYSCTL_startFCC();
    while (DL_SYSCTL_isFCCDone() == 0) {
        delay_cycles(977);  /* 1x LFCLK cycle = 32MHz/32.768kHz = 977, 30.5us */
        fccTimeOutCounter++;
        if(fccTimeOutCounter > 65){
            /* Timeout set to approximately 2ms (user-customizable) */
            break;
        }
    }

    /* get measB= SYSOSC freq wrt LFOSC*/
    fFCCSysoscCount = DL_SYSCTL_readFCC();

    /* Get ratio of both measurements*/
    fFCCRatio = (fFCCPllCount * FLOAT_TO_INT_SCALE) / fFCCSysoscCount;
    /* Check ratio is within bounds*/
    if ((FCC_LOWER_BOUND <  fFCCRatio) && (fFCCRatio < FCC_UPPER_BOUND))
    {
        /* ratio is good for proceeding into application code. */
        fFCCRatioStatus = true;
    }

    return fFCCRatioStatus;
}
SYSCONFIG_WEAK void SYSCFG_DL_SYSCTL_init(void)
{

	//Low Power Mode is configured to be SLEEP0
    DL_SYSCTL_setBORThreshold(DL_SYSCTL_BOR_THRESHOLD_LEVEL_0);
    DL_SYSCTL_setFlashWaitState(DL_SYSCTL_FLASH_WAIT_STATE_2);

    
	DL_SYSCTL_setSYSOSCFreq(DL_SYSCTL_SYSOSC_FREQ_BASE);
	/* Set default configuration */
	DL_SYSCTL_disableHFXT();
	DL_SYSCTL_disableSYSPLL();
    DL_SYSCTL_setHFCLKSourceHFXTParams(DL_SYSCTL_HFXT_RANGE_4_8_MHZ,10, false);
    DL_SYSCTL_configSYSPLL((DL_SYSCTL_SYSPLLConfig *) &gSYSPLLConfig);

    /*
     * [SYSPLL_ERR_01]
     * PLL Incorrect locking WA start.
     * Insert after every PLL enable.
     * This can lead an infinite loop if the condition persists
     * and can block entry to the application code.
     */

    while (SYSCFG_DL_SYSCTL_SYSPLL_init() == false)
    {
        /* Toggle SYSPLL enable to re-enable SYSPLL and re-check incorrect locking */
        DL_SYSCTL_disableSYSPLL();
        DL_SYSCTL_enableSYSPLL();

        /* Wait until SYSPLL startup is stabilized*/
        while ((DL_SYSCTL_getClockStatus() & SYSCTL_CLKSTATUS_SYSPLLGOOD_MASK) != DL_SYSCTL_CLK_STATUS_SYSPLL_GOOD){}
    }
    DL_SYSCTL_setULPCLKDivider(DL_SYSCTL_ULPCLK_DIV_2);
    DL_SYSCTL_enableMFCLK();
    DL_SYSCTL_setMCLKSource(SYSOSC, HSCLK, DL_SYSCTL_HSCLK_SOURCE_SYSPLL);

}
SYSCONFIG_WEAK void SYSCFG_DL_SYSCTL_CLK_init(void) {
    while ((DL_SYSCTL_getClockStatus() & (DL_SYSCTL_CLK_STATUS_SYSPLL_GOOD
		 | DL_SYSCTL_CLK_STATUS_HSCLK_GOOD
		 | DL_SYSCTL_CLK_STATUS_LFOSC_GOOD))
	       != (DL_SYSCTL_CLK_STATUS_SYSPLL_GOOD
		 | DL_SYSCTL_CLK_STATUS_HSCLK_GOOD
		 | DL_SYSCTL_CLK_STATUS_LFOSC_GOOD))
	{
		/* Ensure that clocks are in default POR configuration before initialization.
		* Additionally once LFXT is enabled, the internal LFOSC is disabled, and cannot
		* be re-enabled other than by executing a BOOTRST. */
		;
	}
}



/*
 * Timer clock configuration to be sourced by  / 4 (20000000 Hz)
 * timerClkFreq = (timerClkSrc / (timerClkDivRatio * (timerClkPrescale + 1)))
 *   20000000 Hz = 20000000 Hz / (4 * (0 + 1))
 */
static const DL_TimerA_ClockConfig gPWM_MOTORClockConfig = {
    .clockSel = DL_TIMER_CLOCK_BUSCLK,
    .divideRatio = DL_TIMER_CLOCK_DIVIDE_4,
    .prescale = 0U
};

static const DL_TimerA_PWMConfig gPWM_MOTORConfig = {
    .pwmMode = DL_TIMER_PWM_MODE_EDGE_ALIGN_UP,
    .period = 1000,
    .isTimerWithFourCC = true,
    .startTimer = DL_TIMER_START,
};

SYSCONFIG_WEAK void SYSCFG_DL_PWM_MOTOR_init(void) {

    DL_TimerA_setClockConfig(
        PWM_MOTOR_INST, (DL_TimerA_ClockConfig *) &gPWM_MOTORClockConfig);

    DL_TimerA_initPWMMode(
        PWM_MOTOR_INST, (DL_TimerA_PWMConfig *) &gPWM_MOTORConfig);

    // Set Counter control to the smallest CC index being used
    DL_TimerA_setCounterControl(PWM_MOTOR_INST,DL_TIMER_CZC_CCCTL0_ZCOND,DL_TIMER_CAC_CCCTL0_ACOND,DL_TIMER_CLC_CCCTL0_LCOND);

    DL_TimerA_setCaptureCompareOutCtl(PWM_MOTOR_INST, DL_TIMER_CC_OCTL_INIT_VAL_LOW,
		DL_TIMER_CC_OCTL_INV_OUT_DISABLED, DL_TIMER_CC_OCTL_SRC_FUNCVAL,
		DL_TIMERA_CAPTURE_COMPARE_0_INDEX);

    DL_TimerA_setCaptCompUpdateMethod(PWM_MOTOR_INST, DL_TIMER_CC_UPDATE_METHOD_IMMEDIATE, DL_TIMERA_CAPTURE_COMPARE_0_INDEX);
    DL_TimerA_setCaptureCompareValue(PWM_MOTOR_INST, 500, DL_TIMER_CC_0_INDEX);

    DL_TimerA_setCaptureCompareOutCtl(PWM_MOTOR_INST, DL_TIMER_CC_OCTL_INIT_VAL_LOW,
		DL_TIMER_CC_OCTL_INV_OUT_DISABLED, DL_TIMER_CC_OCTL_SRC_FUNCVAL,
		DL_TIMERA_CAPTURE_COMPARE_1_INDEX);

    DL_TimerA_setCaptCompUpdateMethod(PWM_MOTOR_INST, DL_TIMER_CC_UPDATE_METHOD_IMMEDIATE, DL_TIMERA_CAPTURE_COMPARE_1_INDEX);
    DL_TimerA_setCaptureCompareValue(PWM_MOTOR_INST, 500, DL_TIMER_CC_1_INDEX);

    DL_TimerA_setCaptureCompareOutCtl(PWM_MOTOR_INST, DL_TIMER_CC_OCTL_INIT_VAL_LOW,
		DL_TIMER_CC_OCTL_INV_OUT_DISABLED, DL_TIMER_CC_OCTL_SRC_FUNCVAL,
		DL_TIMERA_CAPTURE_COMPARE_2_INDEX);

    DL_TimerA_setCaptCompUpdateMethod(PWM_MOTOR_INST, DL_TIMER_CC_UPDATE_METHOD_IMMEDIATE, DL_TIMERA_CAPTURE_COMPARE_2_INDEX);
    DL_TimerA_setCaptureCompareValue(PWM_MOTOR_INST, 500, DL_TIMER_CC_2_INDEX);

    DL_TimerA_setCaptureCompareOutCtl(PWM_MOTOR_INST, DL_TIMER_CC_OCTL_INIT_VAL_LOW,
		DL_TIMER_CC_OCTL_INV_OUT_DISABLED, DL_TIMER_CC_OCTL_SRC_FUNCVAL,
		DL_TIMERA_CAPTURE_COMPARE_3_INDEX);

    DL_TimerA_setCaptCompUpdateMethod(PWM_MOTOR_INST, DL_TIMER_CC_UPDATE_METHOD_IMMEDIATE, DL_TIMERA_CAPTURE_COMPARE_3_INDEX);
    DL_TimerA_setCaptureCompareValue(PWM_MOTOR_INST, 500, DL_TIMER_CC_3_INDEX);

    DL_TimerA_enableClock(PWM_MOTOR_INST);


    DL_TimerA_enableInterrupt(PWM_MOTOR_INST , DL_TIMER_INTERRUPT_LOAD_EVENT);

    DL_TimerA_setCCPDirection(PWM_MOTOR_INST , DL_TIMER_CC0_OUTPUT | DL_TIMER_CC1_OUTPUT | DL_TIMER_CC2_OUTPUT | DL_TIMER_CC3_OUTPUT );


}



/*
 * Timer clock configuration to be sourced by BUSCLK /  (20000000 Hz)
 * timerClkFreq = (timerClkSrc / (timerClkDivRatio * (timerClkPrescale + 1)))
 *   100000 Hz = 20000000 Hz / (4 * (199 + 1))
 */
static const DL_TimerG_ClockConfig gM3ClockConfig = {
    .clockSel    = DL_TIMER_CLOCK_BUSCLK,
    .divideRatio = DL_TIMER_CLOCK_DIVIDE_4,
    .prescale = 199U
};

/*
 * Timer load value (where the counter starts from) is calculated as (timerPeriod * timerClockFreq) - 1
 * M3_INST_LOAD_VALUE = (655.36ms * 100000 Hz) - 1
 */
static const DL_TimerG_CaptureCombinedConfig gM3CaptureConfig = {
    .captureMode    = DL_TIMER_CAPTURE_COMBINED_MODE_PULSE_WIDTH_AND_PERIOD_UP,
    .period         = M3_INST_LOAD_VALUE,
    .startTimer     = DL_TIMER_STOP,
    .inputChan      = DL_TIMER_INPUT_CHAN_0,
    .inputInvMode   = DL_TIMER_CC_INPUT_INV_NOINVERT,
};

SYSCONFIG_WEAK void SYSCFG_DL_M3_init(void) {

    DL_TimerG_setClockConfig(M3_INST,
        (DL_TimerG_ClockConfig *) &gM3ClockConfig);

    DL_TimerG_initCaptureCombinedMode(M3_INST,
        (DL_TimerG_CaptureCombinedConfig *) &gM3CaptureConfig);
    DL_TimerG_enableInterrupt(M3_INST , DL_TIMERG_INTERRUPT_CC0_UP_EVENT |
		DL_TIMERG_INTERRUPT_CC1_UP_EVENT |
		DL_TIMERG_INTERRUPT_LOAD_EVENT);

    DL_TimerG_enableClock(M3_INST);

}

/*
 * Timer clock configuration to be sourced by BUSCLK /  (10000000 Hz)
 * timerClkFreq = (timerClkSrc / (timerClkDivRatio * (timerClkPrescale + 1)))
 *   100000 Hz = 10000000 Hz / (8 * (99 + 1))
 */
static const DL_TimerA_ClockConfig gM4ClockConfig = {
    .clockSel    = DL_TIMER_CLOCK_BUSCLK,
    .divideRatio = DL_TIMER_CLOCK_DIVIDE_8,
    .prescale = 99U
};

/*
 * Timer load value (where the counter starts from) is calculated as (timerPeriod * timerClockFreq) - 1
 * M4_INST_LOAD_VALUE = (655.36ms * 100000 Hz) - 1
 */
static const DL_TimerA_CaptureCombinedConfig gM4CaptureConfig = {
    .captureMode    = DL_TIMER_CAPTURE_COMBINED_MODE_PULSE_WIDTH_AND_PERIOD_UP,
    .period         = M4_INST_LOAD_VALUE,
    .startTimer     = DL_TIMER_STOP,
    .inputChan      = DL_TIMER_INPUT_CHAN_0,
    .inputInvMode   = DL_TIMER_CC_INPUT_INV_NOINVERT,
};

SYSCONFIG_WEAK void SYSCFG_DL_M4_init(void) {

    DL_TimerA_setClockConfig(M4_INST,
        (DL_TimerA_ClockConfig *) &gM4ClockConfig);

    DL_TimerA_initCaptureCombinedMode(M4_INST,
        (DL_TimerA_CaptureCombinedConfig *) &gM4CaptureConfig);
    DL_TimerA_enableInterrupt(M4_INST , DL_TIMERA_INTERRUPT_CC0_UP_EVENT |
		DL_TIMERA_INTERRUPT_CC1_UP_EVENT |
		DL_TIMERA_INTERRUPT_LOAD_EVENT);

    DL_TimerA_enableClock(M4_INST);

}

/*
 * Timer clock configuration to be sourced by BUSCLK /  (20000000 Hz)
 * timerClkFreq = (timerClkSrc / (timerClkDivRatio * (timerClkPrescale + 1)))
 *   100000 Hz = 20000000 Hz / (4 * (199 + 1))
 */
static const DL_TimerG_ClockConfig gM2ClockConfig = {
    .clockSel    = DL_TIMER_CLOCK_BUSCLK,
    .divideRatio = DL_TIMER_CLOCK_DIVIDE_4,
    .prescale = 199U
};

/*
 * Timer load value (where the counter starts from) is calculated as (timerPeriod * timerClockFreq) - 1
 * M2_INST_LOAD_VALUE = (655.36ms * 100000 Hz) - 1
 */
static const DL_TimerG_CaptureCombinedConfig gM2CaptureConfig = {
    .captureMode    = DL_TIMER_CAPTURE_COMBINED_MODE_PULSE_WIDTH_AND_PERIOD_UP,
    .period         = M2_INST_LOAD_VALUE,
    .startTimer     = DL_TIMER_STOP,
    .inputChan      = DL_TIMER_INPUT_CHAN_0,
    .inputInvMode   = DL_TIMER_CC_INPUT_INV_NOINVERT,
};

SYSCONFIG_WEAK void SYSCFG_DL_M2_init(void) {

    DL_TimerG_setClockConfig(M2_INST,
        (DL_TimerG_ClockConfig *) &gM2ClockConfig);

    DL_TimerG_initCaptureCombinedMode(M2_INST,
        (DL_TimerG_CaptureCombinedConfig *) &gM2CaptureConfig);
    DL_TimerG_enableInterrupt(M2_INST , DL_TIMERG_INTERRUPT_CC0_UP_EVENT |
		DL_TIMERG_INTERRUPT_CC1_UP_EVENT |
		DL_TIMERG_INTERRUPT_LOAD_EVENT);

    DL_TimerG_enableClock(M2_INST);

}

/*
 * Timer clock configuration to be sourced by BUSCLK /  (10000000 Hz)
 * timerClkFreq = (timerClkSrc / (timerClkDivRatio * (timerClkPrescale + 1)))
 *   100000 Hz = 10000000 Hz / (4 * (99 + 1))
 */
static const DL_TimerG_ClockConfig gM1ClockConfig = {
    .clockSel    = DL_TIMER_CLOCK_BUSCLK,
    .divideRatio = DL_TIMER_CLOCK_DIVIDE_4,
    .prescale = 99U
};

/*
 * Timer load value (where the counter starts from) is calculated as (timerPeriod * timerClockFreq) - 1
 * M1_INST_LOAD_VALUE = (655.36ms * 100000 Hz) - 1
 */
static const DL_TimerG_CaptureCombinedConfig gM1CaptureConfig = {
    .captureMode    = DL_TIMER_CAPTURE_COMBINED_MODE_PULSE_WIDTH_AND_PERIOD_UP,
    .period         = M1_INST_LOAD_VALUE,
    .startTimer     = DL_TIMER_STOP,
    .inputChan      = DL_TIMER_INPUT_CHAN_0,
    .inputInvMode   = DL_TIMER_CC_INPUT_INV_NOINVERT,
};

SYSCONFIG_WEAK void SYSCFG_DL_M1_init(void) {

    DL_TimerG_setClockConfig(M1_INST,
        (DL_TimerG_ClockConfig *) &gM1ClockConfig);

    DL_TimerG_initCaptureCombinedMode(M1_INST,
        (DL_TimerG_CaptureCombinedConfig *) &gM1CaptureConfig);
    DL_TimerG_enableInterrupt(M1_INST , DL_TIMERG_INTERRUPT_CC0_UP_EVENT |
		DL_TIMERG_INTERRUPT_CC1_UP_EVENT |
		DL_TIMERG_INTERRUPT_LOAD_EVENT);

    DL_TimerG_enableClock(M1_INST);

}


/*
 * Timer clock configuration to be sourced by BUSCLK /  (5000000 Hz)
 * timerClkFreq = (timerClkSrc / (timerClkDivRatio * (timerClkPrescale + 1)))
 *   500000 Hz = 5000000 Hz / (8 * (9 + 1))
 */
static const DL_TimerG_ClockConfig gTIMER_0ClockConfig = {
    .clockSel    = DL_TIMER_CLOCK_BUSCLK,
    .divideRatio = DL_TIMER_CLOCK_DIVIDE_8,
    .prescale    = 9U,
};

/*
 * Timer load value (where the counter starts from) is calculated as (timerPeriod * timerClockFreq) - 1
 * TIMER_0_INST_LOAD_VALUE = (65.536ms * 500000 Hz) - 1
 */
static const DL_TimerG_TimerConfig gTIMER_0TimerConfig = {
    .period     = TIMER_0_INST_LOAD_VALUE,
    .timerMode  = DL_TIMER_TIMER_MODE_PERIODIC_UP,
    .startTimer = DL_TIMER_STOP,
};

SYSCONFIG_WEAK void SYSCFG_DL_TIMER_0_init(void) {

    DL_TimerG_setClockConfig(TIMER_0_INST,
        (DL_TimerG_ClockConfig *) &gTIMER_0ClockConfig);

    DL_TimerG_initTimerMode(TIMER_0_INST,
        (DL_TimerG_TimerConfig *) &gTIMER_0TimerConfig);
    DL_TimerG_enableInterrupt(TIMER_0_INST , DL_TIMERG_INTERRUPT_LOAD_EVENT);
	NVIC_SetPriority(TIMER_0_INST_INT_IRQN, 3);
    DL_TimerG_enableClock(TIMER_0_INST);





}


static const DL_UART_Main_ClockConfig gUART_0_DEBUGClockConfig = {
    .clockSel    = DL_UART_MAIN_CLOCK_BUSCLK,
    .divideRatio = DL_UART_MAIN_CLOCK_DIVIDE_RATIO_1
};

static const DL_UART_Main_Config gUART_0_DEBUGConfig = {
    .mode        = DL_UART_MAIN_MODE_NORMAL,
    .direction   = DL_UART_MAIN_DIRECTION_TX_RX,
    .flowControl = DL_UART_MAIN_FLOW_CONTROL_NONE,
    .parity      = DL_UART_MAIN_PARITY_NONE,
    .wordLength  = DL_UART_MAIN_WORD_LENGTH_8_BITS,
    .stopBits    = DL_UART_MAIN_STOP_BITS_ONE
};

SYSCONFIG_WEAK void SYSCFG_DL_UART_0_DEBUG_init(void)
{
    DL_UART_Main_setClockConfig(UART_0_DEBUG_INST, (DL_UART_Main_ClockConfig *) &gUART_0_DEBUGClockConfig);

    DL_UART_Main_init(UART_0_DEBUG_INST, (DL_UART_Main_Config *) &gUART_0_DEBUGConfig);
    /*
     * Configure baud rate by setting oversampling and baud rate divisors.
     *  Target baud rate: 115200
     *  Actual baud rate: 115190.78
     */
    DL_UART_Main_setOversampling(UART_0_DEBUG_INST, DL_UART_OVERSAMPLING_RATE_16X);
    DL_UART_Main_setBaudRateDivisor(UART_0_DEBUG_INST, UART_0_DEBUG_IBRD_40_MHZ_115200_BAUD, UART_0_DEBUG_FBRD_40_MHZ_115200_BAUD);


    /* Configure Interrupts */
    DL_UART_Main_enableInterrupt(UART_0_DEBUG_INST,
                                 DL_UART_MAIN_INTERRUPT_DMA_DONE_TX |
                                 DL_UART_MAIN_INTERRUPT_EOT_DONE |
                                 DL_UART_MAIN_INTERRUPT_RX |
                                 DL_UART_MAIN_INTERRUPT_RX_TIMEOUT_ERROR);
    /* Setting the Interrupt Priority */
    NVIC_SetPriority(UART_0_DEBUG_INST_INT_IRQN, 3);

    /* Configure DMA Transmit Event */
    DL_UART_Main_enableDMATransmitEvent(UART_0_DEBUG_INST);
    /* Configure FIFOs */
    DL_UART_Main_enableFIFOs(UART_0_DEBUG_INST);
    DL_UART_Main_setRXFIFOThreshold(UART_0_DEBUG_INST, DL_UART_RX_FIFO_LEVEL_ONE_ENTRY);
    DL_UART_Main_setTXFIFOThreshold(UART_0_DEBUG_INST, DL_UART_TX_FIFO_LEVEL_1_2_EMPTY);

    DL_UART_Main_setRXInterruptTimeout(UART_0_DEBUG_INST, 10);

    DL_UART_Main_enable(UART_0_DEBUG_INST);
}

static const DL_SPI_Config gSPI_0_config = {
    .mode        = DL_SPI_MODE_CONTROLLER,
    .frameFormat = DL_SPI_FRAME_FORMAT_MOTO4_POL0_PHA0,
    .parity      = DL_SPI_PARITY_NONE,
    .dataSize    = DL_SPI_DATA_SIZE_8,
    .bitOrder    = DL_SPI_BIT_ORDER_MSB_FIRST,
    .chipSelectPin = DL_SPI_CHIP_SELECT_0,
};

static const DL_SPI_ClockConfig gSPI_0_clockConfig = {
    .clockSel    = DL_SPI_CLOCK_BUSCLK,
    .divideRatio = DL_SPI_CLOCK_DIVIDE_RATIO_1
};

SYSCONFIG_WEAK void SYSCFG_DL_SPI_0_init(void) {
    DL_SPI_setClockConfig(SPI_0_INST, (DL_SPI_ClockConfig *) &gSPI_0_clockConfig);

    DL_SPI_init(SPI_0_INST, (DL_SPI_Config *) &gSPI_0_config);

    /* Configure Controller mode */
    /*
     * Set the bit rate clock divider to generate the serial output clock
     *     outputBitRate = (spiInputClock) / ((1 + SCR) * 2)
     *     8000000 = (80000000)/((1 + 4) * 2)
     */
    DL_SPI_setBitRateSerialClockDivider(SPI_0_INST, 4);
    /* Set RX and TX FIFO threshold levels */
    DL_SPI_setFIFOThreshold(SPI_0_INST, DL_SPI_RX_FIFO_LEVEL_1_2_FULL, DL_SPI_TX_FIFO_LEVEL_1_2_EMPTY);

    /* Enable module */
    DL_SPI_enable(SPI_0_INST);
}

/* ADC_VOLTAGE Initialization */
static const DL_ADC12_ClockConfig gADC_VOLTAGEClockConfig = {
    .clockSel       = DL_ADC12_CLOCK_SYSOSC,
    .divideRatio    = DL_ADC12_CLOCK_DIVIDE_8,
    .freqRange      = DL_ADC12_CLOCK_FREQ_RANGE_24_TO_32,
};
SYSCONFIG_WEAK void SYSCFG_DL_ADC_VOLTAGE_init(void)
{
    DL_ADC12_setClockConfig(ADC_VOLTAGE_INST, (DL_ADC12_ClockConfig *) &gADC_VOLTAGEClockConfig);
    DL_ADC12_initSingleSample(ADC_VOLTAGE_INST,
        DL_ADC12_REPEAT_MODE_ENABLED, DL_ADC12_SAMPLING_SOURCE_AUTO, DL_ADC12_TRIG_SRC_SOFTWARE,
        DL_ADC12_SAMP_CONV_RES_12_BIT, DL_ADC12_SAMP_CONV_DATA_FORMAT_UNSIGNED);
    DL_ADC12_configConversionMem(ADC_VOLTAGE_INST, ADC_VOLTAGE_ADCMEM_ADC_CH0,
        DL_ADC12_INPUT_CHAN_0, DL_ADC12_REFERENCE_VOLTAGE_VDDA, DL_ADC12_SAMPLE_TIMER_SOURCE_SCOMP0, DL_ADC12_AVERAGING_MODE_DISABLED,
        DL_ADC12_BURN_OUT_SOURCE_DISABLED, DL_ADC12_TRIGGER_MODE_AUTO_NEXT, DL_ADC12_WINDOWS_COMP_MODE_DISABLED);
    DL_ADC12_configConversionMem(ADC_VOLTAGE_INST, ADC_VOLTAGE_ADCMEM_1,
        DL_ADC12_INPUT_CHAN_1, DL_ADC12_REFERENCE_VOLTAGE_VDDA, DL_ADC12_SAMPLE_TIMER_SOURCE_SCOMP0, DL_ADC12_AVERAGING_MODE_DISABLED,
        DL_ADC12_BURN_OUT_SOURCE_DISABLED, DL_ADC12_TRIGGER_MODE_AUTO_NEXT, DL_ADC12_WINDOWS_COMP_MODE_DISABLED);
    DL_ADC12_configConversionMem(ADC_VOLTAGE_INST, ADC_VOLTAGE_ADCMEM_2,
        DL_ADC12_INPUT_CHAN_2, DL_ADC12_REFERENCE_VOLTAGE_VDDA, DL_ADC12_SAMPLE_TIMER_SOURCE_SCOMP0, DL_ADC12_AVERAGING_MODE_DISABLED,
        DL_ADC12_BURN_OUT_SOURCE_DISABLED, DL_ADC12_TRIGGER_MODE_AUTO_NEXT, DL_ADC12_WINDOWS_COMP_MODE_DISABLED);
    DL_ADC12_configConversionMem(ADC_VOLTAGE_INST, ADC_VOLTAGE_ADCMEM_3,
        DL_ADC12_INPUT_CHAN_8, DL_ADC12_REFERENCE_VOLTAGE_VDDA, DL_ADC12_SAMPLE_TIMER_SOURCE_SCOMP0, DL_ADC12_AVERAGING_MODE_DISABLED,
        DL_ADC12_BURN_OUT_SOURCE_DISABLED, DL_ADC12_TRIGGER_MODE_AUTO_NEXT, DL_ADC12_WINDOWS_COMP_MODE_DISABLED);
    DL_ADC12_configConversionMem(ADC_VOLTAGE_INST, ADC_VOLTAGE_ADCMEM_4,
        DL_ADC12_INPUT_CHAN_0, DL_ADC12_REFERENCE_VOLTAGE_VDDA, DL_ADC12_SAMPLE_TIMER_SOURCE_SCOMP0, DL_ADC12_AVERAGING_MODE_DISABLED,
        DL_ADC12_BURN_OUT_SOURCE_DISABLED, DL_ADC12_TRIGGER_MODE_AUTO_NEXT, DL_ADC12_WINDOWS_COMP_MODE_DISABLED);
    DL_ADC12_setSampleTime0(ADC_VOLTAGE_INST,40000);
    /* Enable ADC12 interrupt */
    DL_ADC12_clearInterruptStatus(ADC_VOLTAGE_INST,(DL_ADC12_INTERRUPT_MEM0_RESULT_LOADED));
    DL_ADC12_enableInterrupt(ADC_VOLTAGE_INST,(DL_ADC12_INTERRUPT_MEM0_RESULT_LOADED));
    DL_ADC12_enableConversions(ADC_VOLTAGE_INST);
}

static const DL_DMA_Config gDMA_CH1Config = {
    .transferMode   = DL_DMA_SINGLE_TRANSFER_MODE,
    .extendedMode   = DL_DMA_NORMAL_MODE,
    .destIncrement  = DL_DMA_ADDR_UNCHANGED,
    .srcIncrement   = DL_DMA_ADDR_INCREMENT,
    .destWidth      = DL_DMA_WIDTH_BYTE,
    .srcWidth       = DL_DMA_WIDTH_BYTE,
    .trigger        = UART_0_DEBUG_INST_DMA_TRIGGER,
    .triggerType    = DL_DMA_TRIGGER_TYPE_EXTERNAL,
};

SYSCONFIG_WEAK void SYSCFG_DL_DMA_CH1_init(void)
{
    DL_DMA_clearInterruptStatus(DMA, DL_DMA_INTERRUPT_CHANNEL0);
    DL_DMA_enableInterrupt(DMA, DL_DMA_INTERRUPT_CHANNEL0);
    DL_DMA_initChannel(DMA, DMA_CH1_CHAN_ID , (DL_DMA_Config *) &gDMA_CH1Config);
}
SYSCONFIG_WEAK void SYSCFG_DL_DMA_init(void){
    SYSCFG_DL_DMA_CH1_init();
}


