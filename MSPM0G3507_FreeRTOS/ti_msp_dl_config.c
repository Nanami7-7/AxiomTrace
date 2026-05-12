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
DL_TimerG_backupConfig gCAPTURE_LEFT_BACKBackup;
DL_TimerG_backupConfig gCAPTURE_RIGHT_FRONTBackup;
DL_TimerA_backupConfig gTIMER_0Backup;

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
    SYSCFG_DL_CAPTURE_LEFT_FRONT_init();
    SYSCFG_DL_CAPTURE_LEFT_BACK_init();
    SYSCFG_DL_CAPTURE_RIGHT_FRONT_init();
    SYSCFG_DL_CAPTURE_RIGHT_BACK_init();
    SYSCFG_DL_TIMER_0_init();
    SYSCFG_DL_UART_0_DEBUG_init();
    SYSCFG_DL_ADC_VOLTAGE_init();
    SYSCFG_DL_SYSCTL_CLK_init();
    /* Ensure backup structures have no valid state */
	gPWM_MOTORBackup.backupRdy 	= false;
	gCAPTURE_LEFT_BACKBackup.backupRdy 	= false;
	gCAPTURE_RIGHT_FRONTBackup.backupRdy 	= false;
	gTIMER_0Backup.backupRdy 	= false;


}
/*
 * User should take care to save and restore register configuration in application.
 * See Retention Configuration section for more details.
 */
SYSCONFIG_WEAK bool SYSCFG_DL_saveConfiguration(void)
{
    bool retStatus = true;

	retStatus &= DL_TimerA_saveConfiguration(PWM_MOTOR_INST, &gPWM_MOTORBackup);
	retStatus &= DL_TimerG_saveConfiguration(CAPTURE_LEFT_BACK_INST, &gCAPTURE_LEFT_BACKBackup);
	retStatus &= DL_TimerG_saveConfiguration(CAPTURE_RIGHT_FRONT_INST, &gCAPTURE_RIGHT_FRONTBackup);
	retStatus &= DL_TimerA_saveConfiguration(TIMER_0_INST, &gTIMER_0Backup);

    return retStatus;
}


SYSCONFIG_WEAK bool SYSCFG_DL_restoreConfiguration(void)
{
    bool retStatus = true;

	retStatus &= DL_TimerA_restoreConfiguration(PWM_MOTOR_INST, &gPWM_MOTORBackup, false);
	retStatus &= DL_TimerG_restoreConfiguration(CAPTURE_LEFT_BACK_INST, &gCAPTURE_LEFT_BACKBackup, false);
	retStatus &= DL_TimerG_restoreConfiguration(CAPTURE_RIGHT_FRONT_INST, &gCAPTURE_RIGHT_FRONTBackup, false);
	retStatus &= DL_TimerA_restoreConfiguration(TIMER_0_INST, &gTIMER_0Backup, false);

    return retStatus;
}

SYSCONFIG_WEAK void SYSCFG_DL_initPower(void)
{
    DL_GPIO_reset(GPIOA);
    DL_GPIO_reset(GPIOB);
    DL_TimerA_reset(PWM_MOTOR_INST);
    DL_TimerG_reset(CAPTURE_LEFT_FRONT_INST);
    DL_TimerG_reset(CAPTURE_LEFT_BACK_INST);
    DL_TimerG_reset(CAPTURE_RIGHT_FRONT_INST);
    DL_TimerG_reset(CAPTURE_RIGHT_BACK_INST);
    DL_TimerA_reset(TIMER_0_INST);
    DL_UART_Main_reset(UART_0_DEBUG_INST);
    DL_ADC12_reset(ADC_VOLTAGE_INST);

    DL_GPIO_enablePower(GPIOA);
    DL_GPIO_enablePower(GPIOB);
    DL_TimerA_enablePower(PWM_MOTOR_INST);
    DL_TimerG_enablePower(CAPTURE_LEFT_FRONT_INST);
    DL_TimerG_enablePower(CAPTURE_LEFT_BACK_INST);
    DL_TimerG_enablePower(CAPTURE_RIGHT_FRONT_INST);
    DL_TimerG_enablePower(CAPTURE_RIGHT_BACK_INST);
    DL_TimerA_enablePower(TIMER_0_INST);
    DL_UART_Main_enablePower(UART_0_DEBUG_INST);
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

    DL_GPIO_initPeripheralInputFunction(GPIO_CAPTURE_LEFT_FRONT_C0_IOMUX,GPIO_CAPTURE_LEFT_FRONT_C0_IOMUX_FUNC);
    DL_GPIO_initPeripheralInputFunction(GPIO_CAPTURE_LEFT_BACK_C0_IOMUX,GPIO_CAPTURE_LEFT_BACK_C0_IOMUX_FUNC);
    DL_GPIO_initPeripheralInputFunction(GPIO_CAPTURE_RIGHT_FRONT_C0_IOMUX,GPIO_CAPTURE_RIGHT_FRONT_C0_IOMUX_FUNC);
    DL_GPIO_initPeripheralInputFunction(GPIO_CAPTURE_RIGHT_BACK_C0_IOMUX,GPIO_CAPTURE_RIGHT_BACK_C0_IOMUX_FUNC);

    DL_GPIO_initPeripheralOutputFunction(
        GPIO_UART_0_DEBUG_IOMUX_TX, GPIO_UART_0_DEBUG_IOMUX_TX_FUNC);
    DL_GPIO_initPeripheralInputFunction(
        GPIO_UART_0_DEBUG_IOMUX_RX, GPIO_UART_0_DEBUG_IOMUX_RX_FUNC);

    DL_GPIO_initDigitalOutputFeatures(LED_A14_IOMUX,
		 DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_PULL_DOWN,
		 DL_GPIO_DRIVE_STRENGTH_LOW, DL_GPIO_HIZ_DISABLE);

    DL_GPIO_initDigitalInputFeatures(ENCODER_LEFT_FRONT_B_IOMUX,
		 DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_PULL_UP,
		 DL_GPIO_HYSTERESIS_DISABLE, DL_GPIO_WAKEUP_DISABLE);

    DL_GPIO_initDigitalInputFeatures(ENCODER_LEFT_BACK_B_IOMUX,
		 DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_PULL_UP,
		 DL_GPIO_HYSTERESIS_DISABLE, DL_GPIO_WAKEUP_DISABLE);

    DL_GPIO_initDigitalInputFeatures(ENCODER_RIGHT_FRONT_B_IOMUX,
		 DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_PULL_UP,
		 DL_GPIO_HYSTERESIS_DISABLE, DL_GPIO_WAKEUP_DISABLE);

    DL_GPIO_initDigitalInputFeatures(ENCODER_RIGHT_BACK_B_IOMUX,
		 DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_PULL_UP,
		 DL_GPIO_HYSTERESIS_DISABLE, DL_GPIO_WAKEUP_DISABLE);

    DL_GPIO_initDigitalOutput(MOTOR_AIN1_IOMUX);

    DL_GPIO_initDigitalOutput(MOTOR_AIN2_IOMUX);

    DL_GPIO_initDigitalOutput(MOTOR_BIN1_IOMUX);

    DL_GPIO_initDigitalOutput(MOTOR_BIN2_IOMUX);

    DL_GPIO_initDigitalOutput(MOTOR_CIN1_IOMUX);

    DL_GPIO_initDigitalOutput(MOTOR_CIN2_IOMUX);

    DL_GPIO_initDigitalOutput(MOTOR_DIN1_IOMUX);

    DL_GPIO_initDigitalOutput(MOTOR_DIN2_IOMUX);

    DL_GPIO_initDigitalOutputFeatures(IIC_SCL_IOMUX,
		 DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_PULL_UP,
		 DL_GPIO_DRIVE_STRENGTH_LOW, DL_GPIO_HIZ_DISABLE);

    DL_GPIO_initDigitalOutputFeatures(IIC_SDA_IOMUX,
		 DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_PULL_UP,
		 DL_GPIO_DRIVE_STRENGTH_LOW, DL_GPIO_HIZ_DISABLE);

    DL_GPIO_clearPins(GPIOA, LED_A14_PIN |
		MOTOR_BIN1_PIN |
		MOTOR_BIN2_PIN |
		MOTOR_CIN2_PIN |
		MOTOR_DIN1_PIN);
    DL_GPIO_setPins(GPIOA, IIC_SCL_PIN |
		IIC_SDA_PIN);
    DL_GPIO_enableOutput(GPIOA, LED_A14_PIN |
		MOTOR_BIN1_PIN |
		MOTOR_BIN2_PIN |
		MOTOR_CIN2_PIN |
		MOTOR_DIN1_PIN |
		IIC_SCL_PIN |
		IIC_SDA_PIN);
    DL_GPIO_clearPins(GPIOB, MOTOR_AIN1_PIN |
		MOTOR_AIN2_PIN |
		MOTOR_CIN1_PIN |
		MOTOR_DIN2_PIN);
    DL_GPIO_enableOutput(GPIOB, MOTOR_AIN1_PIN |
		MOTOR_AIN2_PIN |
		MOTOR_CIN1_PIN |
		MOTOR_DIN2_PIN);

}


static const DL_SYSCTL_SYSPLLConfig gSYSPLLConfig = {
    .inputFreq              = DL_SYSCTL_SYSPLL_INPUT_FREQ_32_48_MHZ,
	.rDivClk2x              = 1,
	.rDivClk1               = 0,
	.rDivClk0               = 0,
	.enableCLK2x            = DL_SYSCTL_SYSPLL_CLK2X_DISABLE,
	.enableCLK1             = DL_SYSCTL_SYSPLL_CLK1_DISABLE,
	.enableCLK0             = DL_SYSCTL_SYSPLL_CLK0_ENABLE,
	.sysPLLMCLK             = DL_SYSCTL_SYSPLL_MCLK_CLK0,
	.sysPLLRef              = DL_SYSCTL_SYSPLL_REF_HFCLK,
	.qDiv                   = 3,
	.pDiv                   = DL_SYSCTL_SYSPLL_PDIV_1
};
SYSCONFIG_WEAK void SYSCFG_DL_SYSCTL_init(void)
{

	//Low Power Mode is configured to be SLEEP0
    DL_SYSCTL_setBORThreshold(DL_SYSCTL_BOR_THRESHOLD_LEVEL_0);
    DL_SYSCTL_setFlashWaitState(DL_SYSCTL_FLASH_WAIT_STATE_2);

    
	DL_SYSCTL_setSYSOSCFreq(DL_SYSCTL_SYSOSC_FREQ_BASE);
	/* Set default configuration */
	DL_SYSCTL_disableHFXT();
	DL_SYSCTL_disableSYSPLL();
    DL_SYSCTL_setHFCLKSourceHFXTParams(DL_SYSCTL_HFXT_RANGE_32_48_MHZ,10, true);
    DL_SYSCTL_configSYSPLL((DL_SYSCTL_SYSPLLConfig *) &gSYSPLLConfig);
    DL_SYSCTL_setULPCLKDivider(DL_SYSCTL_ULPCLK_DIV_2);
    DL_SYSCTL_enableMFCLK();
    DL_SYSCTL_setMCLKSource(SYSOSC, HSCLK, DL_SYSCTL_HSCLK_SOURCE_SYSPLL);

}
SYSCONFIG_WEAK void SYSCFG_DL_SYSCTL_CLK_init(void) {
    while ((DL_SYSCTL_getClockStatus() & (DL_SYSCTL_CLK_STATUS_SYSPLL_GOOD
		 | DL_SYSCTL_CLK_STATUS_HFCLK_GOOD
		 | DL_SYSCTL_CLK_STATUS_HSCLK_GOOD
		 | DL_SYSCTL_CLK_STATUS_LFOSC_GOOD))
	       != (DL_SYSCTL_CLK_STATUS_SYSPLL_GOOD
		 | DL_SYSCTL_CLK_STATUS_HFCLK_GOOD
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
    DL_TimerA_setCaptureCompareValue(PWM_MOTOR_INST, 0, DL_TIMER_CC_0_INDEX);

    DL_TimerA_setCaptureCompareOutCtl(PWM_MOTOR_INST, DL_TIMER_CC_OCTL_INIT_VAL_LOW,
		DL_TIMER_CC_OCTL_INV_OUT_DISABLED, DL_TIMER_CC_OCTL_SRC_FUNCVAL,
		DL_TIMERA_CAPTURE_COMPARE_1_INDEX);

    DL_TimerA_setCaptCompUpdateMethod(PWM_MOTOR_INST, DL_TIMER_CC_UPDATE_METHOD_IMMEDIATE, DL_TIMERA_CAPTURE_COMPARE_1_INDEX);
    DL_TimerA_setCaptureCompareValue(PWM_MOTOR_INST, 0, DL_TIMER_CC_1_INDEX);

    DL_TimerA_setCaptureCompareOutCtl(PWM_MOTOR_INST, DL_TIMER_CC_OCTL_INIT_VAL_LOW,
		DL_TIMER_CC_OCTL_INV_OUT_DISABLED, DL_TIMER_CC_OCTL_SRC_FUNCVAL,
		DL_TIMERA_CAPTURE_COMPARE_2_INDEX);

    DL_TimerA_setCaptCompUpdateMethod(PWM_MOTOR_INST, DL_TIMER_CC_UPDATE_METHOD_IMMEDIATE, DL_TIMERA_CAPTURE_COMPARE_2_INDEX);
    DL_TimerA_setCaptureCompareValue(PWM_MOTOR_INST, 0, DL_TIMER_CC_2_INDEX);

    DL_TimerA_setCaptureCompareOutCtl(PWM_MOTOR_INST, DL_TIMER_CC_OCTL_INIT_VAL_LOW,
		DL_TIMER_CC_OCTL_INV_OUT_DISABLED, DL_TIMER_CC_OCTL_SRC_FUNCVAL,
		DL_TIMERA_CAPTURE_COMPARE_3_INDEX);

    DL_TimerA_setCaptCompUpdateMethod(PWM_MOTOR_INST, DL_TIMER_CC_UPDATE_METHOD_IMMEDIATE, DL_TIMERA_CAPTURE_COMPARE_3_INDEX);
    DL_TimerA_setCaptureCompareValue(PWM_MOTOR_INST, 0, DL_TIMER_CC_3_INDEX);

    DL_TimerA_enableClock(PWM_MOTOR_INST);


    DL_TimerA_enableInterrupt(PWM_MOTOR_INST , DL_TIMER_INTERRUPT_LOAD_EVENT);

    DL_TimerA_setCCPDirection(PWM_MOTOR_INST , DL_TIMER_CC0_OUTPUT | DL_TIMER_CC1_OUTPUT | DL_TIMER_CC2_OUTPUT | DL_TIMER_CC3_OUTPUT );


}



/*
 * Timer clock configuration to be sourced by BUSCLK /  (10000000 Hz)
 * timerClkFreq = (timerClkSrc / (timerClkDivRatio * (timerClkPrescale + 1)))
 *   100000 Hz = 10000000 Hz / (4 * (99 + 1))
 */
static const DL_TimerG_ClockConfig gCAPTURE_LEFT_FRONTClockConfig = {
    .clockSel    = DL_TIMER_CLOCK_BUSCLK,
    .divideRatio = DL_TIMER_CLOCK_DIVIDE_4,
    .prescale = 99U
};

/*
 * Timer load value (where the counter starts from) is calculated as (timerPeriod * timerClockFreq) - 1
 * CAPTURE_LEFT_FRONT_INST_LOAD_VALUE = (655.36ms * 100000 Hz) - 1
 */
static const DL_TimerG_CaptureCombinedConfig gCAPTURE_LEFT_FRONTCaptureConfig = {
    .captureMode    = DL_TIMER_CAPTURE_COMBINED_MODE_PULSE_WIDTH_AND_PERIOD_UP,
    .period         = CAPTURE_LEFT_FRONT_INST_LOAD_VALUE,
    .startTimer     = DL_TIMER_STOP,
    .inputChan      = DL_TIMER_INPUT_CHAN_0,
    .inputInvMode   = DL_TIMER_CC_INPUT_INV_NOINVERT,
};

SYSCONFIG_WEAK void SYSCFG_DL_CAPTURE_LEFT_FRONT_init(void) {

    DL_TimerG_setClockConfig(CAPTURE_LEFT_FRONT_INST,
        (DL_TimerG_ClockConfig *) &gCAPTURE_LEFT_FRONTClockConfig);

    DL_TimerG_initCaptureCombinedMode(CAPTURE_LEFT_FRONT_INST,
        (DL_TimerG_CaptureCombinedConfig *) &gCAPTURE_LEFT_FRONTCaptureConfig);
    DL_TimerG_enableInterrupt(CAPTURE_LEFT_FRONT_INST , DL_TIMERG_INTERRUPT_CC0_UP_EVENT |
		DL_TIMERG_INTERRUPT_CC1_UP_EVENT |
		DL_TIMERG_INTERRUPT_LOAD_EVENT);

    DL_TimerG_enableClock(CAPTURE_LEFT_FRONT_INST);

}

/*
 * Timer clock configuration to be sourced by BUSCLK /  (20000000 Hz)
 * timerClkFreq = (timerClkSrc / (timerClkDivRatio * (timerClkPrescale + 1)))
 *   100000 Hz = 20000000 Hz / (4 * (199 + 1))
 */
static const DL_TimerG_ClockConfig gCAPTURE_LEFT_BACKClockConfig = {
    .clockSel    = DL_TIMER_CLOCK_BUSCLK,
    .divideRatio = DL_TIMER_CLOCK_DIVIDE_4,
    .prescale = 199U
};

/*
 * Timer load value (where the counter starts from) is calculated as (timerPeriod * timerClockFreq) - 1
 * CAPTURE_LEFT_BACK_INST_LOAD_VALUE = (655.36ms * 100000 Hz) - 1
 */
static const DL_TimerG_CaptureCombinedConfig gCAPTURE_LEFT_BACKCaptureConfig = {
    .captureMode    = DL_TIMER_CAPTURE_COMBINED_MODE_PULSE_WIDTH_AND_PERIOD_UP,
    .period         = CAPTURE_LEFT_BACK_INST_LOAD_VALUE,
    .startTimer     = DL_TIMER_STOP,
    .inputChan      = DL_TIMER_INPUT_CHAN_0,
    .inputInvMode   = DL_TIMER_CC_INPUT_INV_NOINVERT,
};

SYSCONFIG_WEAK void SYSCFG_DL_CAPTURE_LEFT_BACK_init(void) {

    DL_TimerG_setClockConfig(CAPTURE_LEFT_BACK_INST,
        (DL_TimerG_ClockConfig *) &gCAPTURE_LEFT_BACKClockConfig);

    DL_TimerG_initCaptureCombinedMode(CAPTURE_LEFT_BACK_INST,
        (DL_TimerG_CaptureCombinedConfig *) &gCAPTURE_LEFT_BACKCaptureConfig);
    DL_TimerG_enableInterrupt(CAPTURE_LEFT_BACK_INST , DL_TIMERG_INTERRUPT_CC0_UP_EVENT |
		DL_TIMERG_INTERRUPT_CC1_UP_EVENT |
		DL_TIMERG_INTERRUPT_LOAD_EVENT);

    DL_TimerG_enableClock(CAPTURE_LEFT_BACK_INST);

}

/*
 * Timer clock configuration to be sourced by BUSCLK /  (20000000 Hz)
 * timerClkFreq = (timerClkSrc / (timerClkDivRatio * (timerClkPrescale + 1)))
 *   100000 Hz = 20000000 Hz / (4 * (199 + 1))
 */
static const DL_TimerG_ClockConfig gCAPTURE_RIGHT_FRONTClockConfig = {
    .clockSel    = DL_TIMER_CLOCK_BUSCLK,
    .divideRatio = DL_TIMER_CLOCK_DIVIDE_4,
    .prescale = 199U
};

/*
 * Timer load value (where the counter starts from) is calculated as (timerPeriod * timerClockFreq) - 1
 * CAPTURE_RIGHT_FRONT_INST_LOAD_VALUE = (655.36ms * 100000 Hz) - 1
 */
static const DL_TimerG_CaptureCombinedConfig gCAPTURE_RIGHT_FRONTCaptureConfig = {
    .captureMode    = DL_TIMER_CAPTURE_COMBINED_MODE_PULSE_WIDTH_AND_PERIOD_UP,
    .period         = CAPTURE_RIGHT_FRONT_INST_LOAD_VALUE,
    .startTimer     = DL_TIMER_STOP,
    .inputChan      = DL_TIMER_INPUT_CHAN_0,
    .inputInvMode   = DL_TIMER_CC_INPUT_INV_NOINVERT,
};

SYSCONFIG_WEAK void SYSCFG_DL_CAPTURE_RIGHT_FRONT_init(void) {

    DL_TimerG_setClockConfig(CAPTURE_RIGHT_FRONT_INST,
        (DL_TimerG_ClockConfig *) &gCAPTURE_RIGHT_FRONTClockConfig);

    DL_TimerG_initCaptureCombinedMode(CAPTURE_RIGHT_FRONT_INST,
        (DL_TimerG_CaptureCombinedConfig *) &gCAPTURE_RIGHT_FRONTCaptureConfig);
    DL_TimerG_enableInterrupt(CAPTURE_RIGHT_FRONT_INST , DL_TIMERG_INTERRUPT_CC0_UP_EVENT |
		DL_TIMERG_INTERRUPT_CC1_UP_EVENT |
		DL_TIMERG_INTERRUPT_LOAD_EVENT);

    DL_TimerG_enableClock(CAPTURE_RIGHT_FRONT_INST);

}

/*
 * Timer clock configuration to be sourced by BUSCLK /  (10000000 Hz)
 * timerClkFreq = (timerClkSrc / (timerClkDivRatio * (timerClkPrescale + 1)))
 *   100000 Hz = 10000000 Hz / (4 * (99 + 1))
 */
static const DL_TimerG_ClockConfig gCAPTURE_RIGHT_BACKClockConfig = {
    .clockSel    = DL_TIMER_CLOCK_BUSCLK,
    .divideRatio = DL_TIMER_CLOCK_DIVIDE_4,
    .prescale = 99U
};

/*
 * Timer load value (where the counter starts from) is calculated as (timerPeriod * timerClockFreq) - 1
 * CAPTURE_RIGHT_BACK_INST_LOAD_VALUE = (655.36ms * 100000 Hz) - 1
 */
static const DL_TimerG_CaptureCombinedConfig gCAPTURE_RIGHT_BACKCaptureConfig = {
    .captureMode    = DL_TIMER_CAPTURE_COMBINED_MODE_PULSE_WIDTH_AND_PERIOD_UP,
    .period         = CAPTURE_RIGHT_BACK_INST_LOAD_VALUE,
    .startTimer     = DL_TIMER_STOP,
    .inputChan      = DL_TIMER_INPUT_CHAN_0,
    .inputInvMode   = DL_TIMER_CC_INPUT_INV_NOINVERT,
};

SYSCONFIG_WEAK void SYSCFG_DL_CAPTURE_RIGHT_BACK_init(void) {

    DL_TimerG_setClockConfig(CAPTURE_RIGHT_BACK_INST,
        (DL_TimerG_ClockConfig *) &gCAPTURE_RIGHT_BACKClockConfig);

    DL_TimerG_initCaptureCombinedMode(CAPTURE_RIGHT_BACK_INST,
        (DL_TimerG_CaptureCombinedConfig *) &gCAPTURE_RIGHT_BACKCaptureConfig);
    DL_TimerG_enableInterrupt(CAPTURE_RIGHT_BACK_INST , DL_TIMERG_INTERRUPT_CC0_UP_EVENT |
		DL_TIMERG_INTERRUPT_CC1_UP_EVENT |
		DL_TIMERG_INTERRUPT_LOAD_EVENT);

    DL_TimerG_enableClock(CAPTURE_RIGHT_BACK_INST);

}


/*
 * Timer clock configuration to be sourced by BUSCLK /  (10000000 Hz)
 * timerClkFreq = (timerClkSrc / (timerClkDivRatio * (timerClkPrescale + 1)))
 *   1000000 Hz = 10000000 Hz / (8 * (9 + 1))
 */
static const DL_TimerA_ClockConfig gTIMER_0ClockConfig = {
    .clockSel    = DL_TIMER_CLOCK_BUSCLK,
    .divideRatio = DL_TIMER_CLOCK_DIVIDE_8,
    .prescale    = 9U,
};

/*
 * Timer load value (where the counter starts from) is calculated as (timerPeriod * timerClockFreq) - 1
 * TIMER_0_INST_LOAD_VALUE = (65.536ms * 1000000 Hz) - 1
 */
static const DL_TimerA_TimerConfig gTIMER_0TimerConfig = {
    .period     = TIMER_0_INST_LOAD_VALUE,
    .timerMode  = DL_TIMER_TIMER_MODE_PERIODIC_UP,
    .startTimer = DL_TIMER_STOP,
};

SYSCONFIG_WEAK void SYSCFG_DL_TIMER_0_init(void) {

    DL_TimerA_setClockConfig(TIMER_0_INST,
        (DL_TimerA_ClockConfig *) &gTIMER_0ClockConfig);

    DL_TimerA_initTimerMode(TIMER_0_INST,
        (DL_TimerA_TimerConfig *) &gTIMER_0TimerConfig);
    DL_TimerA_enableInterrupt(TIMER_0_INST , DL_TIMERA_INTERRUPT_LOAD_EVENT);
	NVIC_SetPriority(TIMER_0_INST_INT_IRQN, 3);
    DL_TimerA_enableClock(TIMER_0_INST);





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
                                 DL_UART_MAIN_INTERRUPT_RX);
    /* Setting the Interrupt Priority */
    NVIC_SetPriority(UART_0_DEBUG_INST_INT_IRQN, 3);


    DL_UART_Main_enable(UART_0_DEBUG_INST);
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
    DL_ADC12_setSampleTime0(ADC_VOLTAGE_INST,40000);
    /* Enable ADC12 interrupt */
    DL_ADC12_clearInterruptStatus(ADC_VOLTAGE_INST,(DL_ADC12_INTERRUPT_MEM0_RESULT_LOADED));
    DL_ADC12_enableInterrupt(ADC_VOLTAGE_INST,(DL_ADC12_INTERRUPT_MEM0_RESULT_LOADED));
    DL_ADC12_enableConversions(ADC_VOLTAGE_INST);
}

