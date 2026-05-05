/**
 * @file    project_config.h
 * @brief   项目硬件配置集中定义
 * @note    所有引脚映射、外设实例分配、硬件参数在此集中定义。
 *          更换硬件/引脚仅需修改此文件，无需改动驱动代码。
 *          引脚编号来源于ti_msp_dl_config.h(SysConfig生成)
 */
#ifndef PROJECT_CONFIG_H
#define PROJECT_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

/* ======================== 包含 ======================== */
#include "hal_common.h"
#include "ti_msp_dl_config.h"

/* ================================================================
 *  LED配置
 *  SysConfig已配置: GPIOA.14, PINCM36
 * ================================================================ */

/** LED端口(GPIOA) */
#define PRJ_LED_PORT            HAL_GPIO_PORT_A
/** LED引脚编号 */
#define PRJ_LED_PIN             LED_A14_PIN

/* ================================================================
 *  调试UART配置
 *  SysConfig已配置: UART0, TX=PA10/PINCM21, RX=PA11/PINCM22
 *  波特率: 115200, 时钟: 40MHz
 * ================================================================ */

/** 调试串口HAL实例 */
#define PRJ_UART_DEBUG_ID       HAL_UART_DEBUG

/* ================================================================
 *  电机PWM配置
 *  SysConfig已配置: TIMA0, 4通道PWM, 20MHz时钟, 1kHz周期
 *  每个电机1个PWM通道(控速) + 2个GPIO(IN1/IN2控方向)
 *  通道: C0=PA0/PINCM1, C1=PA1/PINCM2, C2=PA15/PINCM37, C3=PA12/PINCM34
 *  驱动芯片: TB6612FNG
 *  正转: IN1=1, IN2=0, PWM=duty
 *  反转: IN1=0, IN2=1, PWM=duty
 *  刹车: IN1=1, IN2=1, PWM=0
 *  滑行: IN1=0, IN2=0, PWM=0
 * ================================================================ */

/** 电机PWM定时器HAL实例 */
#define PRJ_MOTOR_PWM_TIMER     HAL_TIMER_PWM_MOTOR
/** PWM时钟频率(Hz) */
#define PRJ_MOTOR_PWM_CLK_HZ    (20000000UL)
/** PWM周期值(1kHz = 20000个20MHz时钟周期) */
#define PRJ_MOTOR_PWM_PERIOD    (20000U)

/* ---- 电机A(左前): CC0=PA0, IN1=PB20, IN2=PB24 ---- */
#define PRJ_MOTOR_A_PWM_CH      (0U)
#define PRJ_MOTOR_A_IN1_PORT    HAL_GPIO_PORT_B
#define PRJ_MOTOR_A_IN1_PIN     MOTOR_AIN1_PIN
#define PRJ_MOTOR_A_IN2_PORT    HAL_GPIO_PORT_B
#define PRJ_MOTOR_A_IN2_PIN     MOTOR_AIN2_PIN

/* ---- 电机B(左后): CC1=PA1, IN1=PA4, IN2=PA7 ---- */
#define PRJ_MOTOR_B_PWM_CH      (1U)
#define PRJ_MOTOR_B_IN1_PORT    HAL_GPIO_PORT_A
#define PRJ_MOTOR_B_IN1_PIN     MOTOR_BIN1_PIN
#define PRJ_MOTOR_B_IN2_PORT    HAL_GPIO_PORT_A
#define PRJ_MOTOR_B_IN2_PIN     MOTOR_BIN2_PIN

/* ---- 电机C(右前): CC2=PA15, IN1=PB2, IN2=PB3 ---- */
#define PRJ_MOTOR_C_PWM_CH      (2U)
#define PRJ_MOTOR_C_IN1_PORT    HAL_GPIO_PORT_B
#define PRJ_MOTOR_C_IN1_PIN     MOTOR_CIN1_PIN
#define PRJ_MOTOR_C_IN2_PORT    HAL_GPIO_PORT_B
#define PRJ_MOTOR_C_IN2_PIN     MOTOR_CIN2_PIN

/* ---- 电机D(右后): CC3=PA12, IN1=PA8, IN2=PA9 ---- */
#define PRJ_MOTOR_D_PWM_CH      (3U)
#define PRJ_MOTOR_D_IN1_PORT    HAL_GPIO_PORT_A
#define PRJ_MOTOR_D_IN1_PIN     MOTOR_DIN1_PIN
#define PRJ_MOTOR_D_IN2_PORT    HAL_GPIO_PORT_A
#define PRJ_MOTOR_D_IN2_PIN     MOTOR_DIN2_PIN

/** 电机方向修正(+1/-1, 0视为+1) */
#define PRJ_MOTOR_A_DIR_SIGN    (1)
#define PRJ_MOTOR_B_DIR_SIGN    (1)
#define PRJ_MOTOR_C_DIR_SIGN    (1)
#define PRJ_MOTOR_D_DIR_SIGN    (1)

/** 电机配置表(顺序需与BSP_MOTOR_x一致) */
#define PRJ_MOTOR_CONFIGS { \
		{ PRJ_MOTOR_A_PWM_CH, PRJ_MOTOR_A_IN1_PORT, PRJ_MOTOR_A_IN1_PIN, \
			PRJ_MOTOR_A_IN2_PORT, PRJ_MOTOR_A_IN2_PIN, PRJ_MOTOR_A_DIR_SIGN }, \
		{ PRJ_MOTOR_B_PWM_CH, PRJ_MOTOR_B_IN1_PORT, PRJ_MOTOR_B_IN1_PIN, \
			PRJ_MOTOR_B_IN2_PORT, PRJ_MOTOR_B_IN2_PIN, PRJ_MOTOR_B_DIR_SIGN }, \
		{ PRJ_MOTOR_C_PWM_CH, PRJ_MOTOR_C_IN1_PORT, PRJ_MOTOR_C_IN1_PIN, \
			PRJ_MOTOR_C_IN2_PORT, PRJ_MOTOR_C_IN2_PIN, PRJ_MOTOR_C_DIR_SIGN }, \
		{ PRJ_MOTOR_D_PWM_CH, PRJ_MOTOR_D_IN1_PORT, PRJ_MOTOR_D_IN1_PIN, \
			PRJ_MOTOR_D_IN2_PORT, PRJ_MOTOR_D_IN2_PIN, PRJ_MOTOR_D_DIR_SIGN }, \
}

/* ================================================================
 *  编码器捕获配置
 *  SysConfig已配置: TIMG8/7/6/0, 组合捕获模式(脉宽+周期)
 * ================================================================ */

/** 编码器每转脉冲数(单相PPR) */
#define PRJ_ENCODER_PPR         (13U)
/** 减速比(电机转/输出轴转) */
#define PRJ_ENCODER_REDUCTION_RATIO  (20U)
/** 每转总脉冲数(PPR×减速比×2倍频, 用于M法RPM计算) */
#define PRJ_ENCODER_PULSES_PER_REV   \
	(PRJ_ENCODER_PPR * PRJ_ENCODER_REDUCTION_RATIO * 2U)

/** 左前编码器HAL实例 */
#define PRJ_ENCODER_LF_TIMER    HAL_TIMER_CAPTURE_LF
/** 左后编码器HAL实例 */
#define PRJ_ENCODER_LB_TIMER    HAL_TIMER_CAPTURE_LB
/** 右前编码器HAL实例 */
#define PRJ_ENCODER_RF_TIMER    HAL_TIMER_CAPTURE_RF
/** 右后编码器HAL实例 */
#define PRJ_ENCODER_RB_TIMER    HAL_TIMER_CAPTURE_RB

/** 编码器B相端口(SysConfig已配置, GPIOA) */
#define PRJ_ENCODER_PORT        HAL_GPIO_PORT_A
/** 左前B相引脚(PA28) */
#define PRJ_ENCODER_LF_B_PIN    ENCODER_LEFT_FRONT_B_PIN
/** 左后B相引脚(PA31) */
#define PRJ_ENCODER_LB_B_PIN    ENCODER_LEFT_BACK_B_PIN
/** 右前B相引脚(PA2) */
#define PRJ_ENCODER_RF_B_PIN    ENCODER_RIGHT_FRONT_B_PIN
/** 右后B相引脚(PA3) */
#define PRJ_ENCODER_RB_B_PIN    ENCODER_RIGHT_BACK_B_PIN

/** 编码器方向修正(+1/-1, 0视为+1) */
#define PRJ_ENCODER_LF_DIR_SIGN (-1)
#define PRJ_ENCODER_LB_DIR_SIGN (-1)
#define PRJ_ENCODER_RF_DIR_SIGN (-1)
#define PRJ_ENCODER_RB_DIR_SIGN (-1)

/** 编码器配置表(顺序需与BSP_ENCODER_x一致) */
#define PRJ_ENCODER_CONFIGS { \
		{ PRJ_ENCODER_LF_TIMER, PRJ_ENCODER_PORT, PRJ_ENCODER_LF_B_PIN, \
			PRJ_ENCODER_LF_DIR_SIGN }, \
		{ PRJ_ENCODER_LB_TIMER, PRJ_ENCODER_PORT, PRJ_ENCODER_LB_B_PIN, \
			PRJ_ENCODER_LB_DIR_SIGN }, \
		{ PRJ_ENCODER_RF_TIMER, PRJ_ENCODER_PORT, PRJ_ENCODER_RF_B_PIN, \
			PRJ_ENCODER_RF_DIR_SIGN }, \
		{ PRJ_ENCODER_RB_TIMER, PRJ_ENCODER_PORT, PRJ_ENCODER_RB_B_PIN, \
			PRJ_ENCODER_RB_DIR_SIGN }, \
}

/* ================================================================
 *  ADC配置
 *  SysConfig已配置: ADC0, 12位单次采样, 通道0(PA27), VDDA参考3.3V
 * ================================================================ */

/** 电压ADC HAL实例 */
#define PRJ_ADC_VOLTAGE_ID      HAL_ADC_VOLTAGE
/** ADC参考电压(mV) */
#define PRJ_ADC_VREF_MV         (3300U)
/** ADC分辨率(12位) */
#define PRJ_ADC_RESOLUTION      (4096U)

/* ================================================================
 *  软件I2C配置
 *  SysConfig未配置, 由BSP层通过HAL GPIO初始化
 *  默认引脚: SCL=PB21/PINCM49, SDA=PB22/PINCM50
 *  可根据实际板卡修改
 * ================================================================ */

/** 软件I2C SCL端口 */
#define PRJ_SOFT_I2C_SCL_PORT   HAL_GPIO_PORT_B
/** 软件I2C SCL引脚 */
#define PRJ_SOFT_I2C_SCL_PIN    DL_GPIO_PIN_21
/** 软件I2C SCL引脚复用 */
#define PRJ_SOFT_I2C_SCL_IOMUX  (IOMUX_PINCM49)
/** 软件I2C SDA端口 */
#define PRJ_SOFT_I2C_SDA_PORT   HAL_GPIO_PORT_B
/** 软件I2C SDA引脚 */
#define PRJ_SOFT_I2C_SDA_PIN    DL_GPIO_PIN_22
/** 软件I2C SDA引脚复用 */
#define PRJ_SOFT_I2C_SDA_IOMUX  (IOMUX_PINCM50)
/** 软件I2C时钟频率(Hz), 标准模式100kHz */
#define PRJ_SOFT_I2C_CLK_HZ     (100000UL)

/* ================================================================
 *  系统参数
 * ================================================================ */

/** CPU时钟频率(Hz) */
#define PRJ_CPUCLK_HZ           (80000000UL)
/** 系统定时器HAL实例 */
#define PRJ_SYS_TICK_TIMER      HAL_TIMER_SYS_TICK

#ifdef __cplusplus
}
#endif

#endif /* PROJECT_CONFIG_H */
