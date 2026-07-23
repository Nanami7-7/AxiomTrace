/**
 * @file    project_config.h
 * @brief   项目唯一配置入口
 * @note    所有用户可调参数集中于此。修改后重新编译即可生效�?
 *          ti_msp_dl_config.h(SysConfig生成) �?FreeRTOSConfig.h 保持独立�?
 *
 * 配置分区:
 *   1  版本信息        7  ADC配置
 *   2  系统参数        8  IMU配置
 *   3  LED             9  互补滤波
 *   4  调试串口        10 位置-速度控制
 *   5  电机驱动        11 时间频率派生
 *   6  编码�?         12 数学常量
 *   13 FreeRTOS任务    18 MATHACL
 *   14 菜单            19 编译验证
 *   15 PID默认参数
 *   16 滤波器编译开�?
 *   17 滤波器默认参�?
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
 * 1. 版本信息 (�?project_version.h)
 * ================================================================ */
#define PROJECT_VERSION_MAJOR        (0U)
#define PROJECT_VERSION_MINOR        (1U)
#define PROJECT_VERSION_PATCH        (0U)
#define PROJECT_VERSION_STRING       "0.1.0"
#define PROJECT_PROTOCOL_VERSION     (1U)
#define PROJECT_BOARD_NAME           "MSPM0G3507"
#define PROJECT_MOTOR_DRIVER_NAME    "DRV8870"

/* ================================================================
 * 2. 系统参数
 * ================================================================ */
#define SYS_TICK_TIMER               HAL_TIMER_SYS_TICK
#define SYS_CLK_HZ                   (80000000UL)
#define BUS_CLK_HZ                   (40000000UL)
#define MS_PER_S                     (1000U)
#define MS_PER_MIN                   (60000U)
#define US_PER_MS                    (1000U)

/* ================================================================
 * 3. LED配置
 * SysConfig: GPIOA.14, PINCM36
 * 注意: ti_msp_dl_config.h 已定�?LED_PORT (GPIOA), 此处重定义为
 *       HAL_GPIO_PORT_A 以统一抽象层。先 #undef 避免重定义警告�?
 * ================================================================ */
#undef LED_PORT
#define LED_PORT                     HAL_GPIO_PORT_A
#define LED_PIN                       LED_A27_PIN

/* ================================================================
 * 4. 调试串口配置
 * SysConfig: UART0, TX=PA10, RX=PA11, 115200bps
 * ================================================================ */
#define UART_DEBUG_ID                HAL_UART_DEBUG

/* ================================================================
 * 4b. BLE串口配置 (JDY-23, UART1)
 * SysConfig: UART1, TX=PB6, RX=PB7, 9600bps
 * ================================================================ */
#define UART_BLE_ID                  HAL_UART_BLE
#define JDY23_UART_BAUD              (9600U)

/* BLE 菜单功能开�?*/
#ifndef BLE_MENU_ENABLE
#define BLE_MENU_ENABLE              (0U)
#endif
#ifndef BLE_MENU_CONSOLE_ENABLE
#define BLE_MENU_CONSOLE_ENABLE     (0U)
#endif

/* ================================================================
 * 5. 电机驱动配置
 *
 * 分层: Application -> bsp_motor(门面) -> 芯片后端 -> HAL
 * 上层统一使用 -MOTOR_COMMAND_MAX ~ +MOTOR_COMMAND_MAX
 * ================================================================ */
#define MOTOR_DRV8870                (1U)
#define MOTOR_TB6612                 (2U)

#ifndef MOTOR_DRIVER
#define MOTOR_DRIVER                  MOTOR_DRV8870
#endif

#define MOTOR_COMMAND_MAX            (500U)

#define MOTOR_A_DIR_SIGN             (+1)
#define MOTOR_B_DIR_SIGN             (+1)
#define MOTOR_C_DIR_SIGN             (+1)
#define MOTOR_D_DIR_SIGN             (+1)

/* ---- DRV8870 (锁相驱动, 默认) ----
 * 硬件: MCU PWM -> DRV8870 IN1(直连) + S8050反相�?-> IN2
 * 正转: duty > DEADBAND_HIGH; 反转: duty < DEADBAND_LOW
 * 死区: DEADBAND_LOW~DEADBAND_HIGH; 零命�? NEUTRAL
 */
#define DRV8870_PWM_TIMER            HAL_TIMER_PWM_MOTOR
#define DRV8870_PWM_CLK_HZ           ((unsigned long)(PWM_MOTOR_INST_CLK_FREQ))
#define DRV8870_PWM_PERIOD           (1000U)
#define DRV8870_COMMAND_MAX          MOTOR_COMMAND_MAX

#define DRV8870_DEADBAND_LOW         (40U)
#define DRV8870_NEUTRAL              (50U)
#define DRV8870_DEADBAND_HIGH        (55U)
#define DRV8870_ZERO_DUTY_OFFSET     (0)

#define DRV8870_POWER_PORT           HAL_GPIO_PORT_B
#define DRV8870_POWER_PIN            POWER_pb19_PIN
#define DRV8870_POWER_ON_LEVEL        true
#define DRV8870_POWER_OFF_LEVEL       false
#define DRV8870_POWER_STARTUP_MS      (20U)
#define DRV8870_POWER_SETTLE_MS       (5U)

#ifndef DRV8870_FACTORY_TEST_ENABLE
#define DRV8870_FACTORY_TEST_ENABLE   (0U)
#endif

#define DRV8870_A_PWM_CH             (0U)
#define DRV8870_B_PWM_CH             (1U)
#define DRV8870_C_PWM_CH             (2U)
#define DRV8870_D_PWM_CH             (3U)

#define DRV8870_CONFIGS { \
    { DRV8870_A_PWM_CH, MOTOR_A_DIR_SIGN, DRV8870_ZERO_DUTY_OFFSET, \
      DRV8870_DEADBAND_LOW, DRV8870_NEUTRAL, DRV8870_DEADBAND_HIGH }, \
    { DRV8870_B_PWM_CH, MOTOR_B_DIR_SIGN, DRV8870_ZERO_DUTY_OFFSET, \
      DRV8870_DEADBAND_LOW, DRV8870_NEUTRAL, DRV8870_DEADBAND_HIGH }, \
    { DRV8870_C_PWM_CH, MOTOR_C_DIR_SIGN, DRV8870_ZERO_DUTY_OFFSET, \
      DRV8870_DEADBAND_LOW, DRV8870_NEUTRAL, DRV8870_DEADBAND_HIGH }, \
    { DRV8870_D_PWM_CH, MOTOR_D_DIR_SIGN, DRV8870_ZERO_DUTY_OFFSET, \
      DRV8870_DEADBAND_LOW, DRV8870_NEUTRAL, DRV8870_DEADBAND_HIGH }, \
}

/* ---- TB6612 (备用, 需在SysConfig恢复方向GPIO) ---- */
#define TB6612_PWM_TIMER             HAL_TIMER_PWM_MOTOR
#define TB6612_PWM_CLK_HZ            ((unsigned long)(PWM_MOTOR_INST_CLK_FREQ))
#define TB6612_PWM_PERIOD             (1000U)
#define TB6612_POWER_STARTUP_MS       (1U)

#ifndef TB6612_STANDBY_CONTROL_ENABLE
#define TB6612_STANDBY_CONTROL_ENABLE (0U)
#endif
#define TB6612_BOARD_CONFIG_AVAILABLE  (0U)

#if (MOTOR_DRIVER == MOTOR_TB6612)
#if !defined(MOTOR_AIN1_PIN) || !defined(MOTOR_AIN2_PIN) || \
    !defined(MOTOR_BIN1_PIN) || !defined(MOTOR_BIN2_PIN) || \
    !defined(MOTOR_CIN1_PIN) || !defined(MOTOR_CIN2_PIN) || \
    !defined(MOTOR_DIN1_PIN) || !defined(MOTOR_DIN2_PIN)
#error "TB6612 selected: restore MOTOR_AIN1..MOTOR_DIN2 in SysConfig"
#else
#define TB6612_A_PWM_CH             (0U)
#define TB6612_A_IN1_PORT           HAL_GPIO_PORT_B
#define TB6612_A_IN1_PIN            MOTOR_AIN1_PIN
#define TB6612_A_IN2_PORT           HAL_GPIO_PORT_B
#define TB6612_A_IN2_PIN            MOTOR_AIN2_PIN

#define TB6612_B_PWM_CH             (1U)
#define TB6612_B_IN1_PORT           HAL_GPIO_PORT_A
#define TB6612_B_IN1_PIN            MOTOR_BIN1_PIN
#define TB6612_B_IN2_PORT           HAL_GPIO_PORT_A
#define TB6612_B_IN2_PIN            MOTOR_BIN2_PIN

#define TB6612_C_PWM_CH             (2U)
#define TB6612_C_IN1_PORT           HAL_GPIO_PORT_A
#define TB6612_C_IN1_PIN            MOTOR_CIN1_PIN
#define TB6612_C_IN2_PORT           HAL_GPIO_PORT_A
#define TB6612_C_IN2_PIN            MOTOR_CIN2_PIN

#define TB6612_D_PWM_CH             (3U)
#define TB6612_D_IN1_PORT           HAL_GPIO_PORT_B
#define TB6612_D_IN1_PIN            MOTOR_DIN1_PIN
#define TB6612_D_IN2_PORT           HAL_GPIO_PORT_B
#define TB6612_D_IN2_PIN            MOTOR_DIN2_PIN

#define TB6612_CONFIGS { \
    { TB6612_A_PWM_CH, TB6612_A_IN1_PORT, TB6612_A_IN1_PIN, \
      TB6612_A_IN2_PORT, TB6612_A_IN2_PIN, MOTOR_A_DIR_SIGN }, \
    { TB6612_B_PWM_CH, TB6612_B_IN1_PORT, TB6612_B_IN1_PIN, \
      TB6612_B_IN2_PORT, TB6612_B_IN2_PIN, MOTOR_B_DIR_SIGN }, \
    { TB6612_C_PWM_CH, TB6612_C_IN1_PORT, TB6612_C_IN1_PIN, \
      TB6612_C_IN2_PORT, TB6612_C_IN2_PIN, MOTOR_C_DIR_SIGN }, \
    { TB6612_D_PWM_CH, TB6612_D_IN1_PORT, TB6612_D_IN1_PIN, \
      TB6612_D_IN2_PORT, TB6612_D_IN2_PIN, MOTOR_D_DIR_SIGN }, \
}

#if (TB6612_STANDBY_CONTROL_ENABLE != 0U)
#if !defined(TB6612_STANDBY_PORT) || !defined(TB6612_STANDBY_PIN) || \
    !defined(TB6612_STANDBY_ACTIVE_LEVEL)
#error "TB6612 STBY control enabled: define port, pin and active level"
#else
#define TB6612_POWER_CONFIG { \
    true, TB6612_STANDBY_PORT, TB6612_STANDBY_PIN, TB6612_STANDBY_ACTIVE_LEVEL \
}
#undef TB6612_BOARD_CONFIG_AVAILABLE
#define TB6612_BOARD_CONFIG_AVAILABLE (1U)
#endif
#else
#define TB6612_POWER_CONFIG { false, HAL_GPIO_PORT_A, 0U, true }
#undef TB6612_BOARD_CONFIG_AVAILABLE
#define TB6612_BOARD_CONFIG_AVAILABLE (1U)
#endif
#endif /* direction GPIO macros available */
#endif /* selected TB6612 */

/* 后端无关别名 */
#if (MOTOR_DRIVER == MOTOR_DRV8870)
#define MOTOR_PWM_TIMER              DRV8870_PWM_TIMER
#define MOTOR_PWM_CLK_HZ             DRV8870_PWM_CLK_HZ
#define MOTOR_PWM_PERIOD             DRV8870_PWM_PERIOD
#define MOTOR_POWER_STARTUP_MS       DRV8870_POWER_STARTUP_MS
#define MOTOR_POWER_SETTLE_MS        DRV8870_POWER_SETTLE_MS
#else
#define MOTOR_PWM_TIMER              TB6612_PWM_TIMER
#define MOTOR_PWM_CLK_HZ             TB6612_PWM_CLK_HZ
#define MOTOR_PWM_PERIOD             TB6612_PWM_PERIOD
#define MOTOR_POWER_STARTUP_MS       TB6612_POWER_STARTUP_MS
#define MOTOR_POWER_SETTLE_MS        (0U)
#endif

/* ================================================================
 * 6. 编码器配�?
 * SysConfig: TIMG7/TIMA1/TIMG6/TIMG0, 组合捕获模式
 * ================================================================ */
#define ENC_PPR                      (13U)
#define ENC_GEAR_NUM                  (20U)
#define ENC_GEAR_DEN                  (1U)
#define ENC_DECODE_MULT               (2U)

#define ENC_OUTPUT_PPR \
    ((ENC_PPR * ENC_GEAR_NUM * ENC_DECODE_MULT) / ENC_GEAR_DEN)

/* 兼容旧引�?*/
#define ENC_PULSES_PER_REV            ENC_OUTPUT_PPR

#define ENC_LF_TIMER                  HAL_TIMER_CAPTURE_LF
#define ENC_LB_TIMER                  HAL_TIMER_CAPTURE_LB
#define ENC_RF_TIMER                  HAL_TIMER_CAPTURE_RF
#define ENC_RB_TIMER                  HAL_TIMER_CAPTURE_RB

#define ENC_LF_IRQ_HANDLER            M3_INST_IRQHandler
#define ENC_LB_IRQ_HANDLER            M4_INST_IRQHandler
#define ENC_RF_IRQ_HANDLER            M2_INST_IRQHandler
#define ENC_RB_IRQ_HANDLER            M1_INST_IRQHandler

#define ENC_LF_B_PORT                 HAL_GPIO_PORT_A
#define ENC_LB_B_PORT                 HAL_GPIO_PORT_A
#define ENC_RF_B_PORT                 HAL_GPIO_PORT_A
#define ENC_RB_B_PORT                 HAL_GPIO_PORT_A

#define ENC_LF_B_PIN                  ENCODER_M3_B_PIN
#define ENC_LB_B_PIN                  ENCODER_M4_B_PIN
#define ENC_RF_B_PIN                  ENCODER_M2_B_PIN
#define ENC_RB_B_PIN                  ENCODER_M1_B_PIN

#define ENC_LF_DIR_SIGN               (-1)
#define ENC_LB_DIR_SIGN               (-1)
#define ENC_RF_DIR_SIGN               (+1)
#define ENC_RB_DIR_SIGN               (+1)

/* 电机A/M1=右后(RB), B/M2=右前(RF), C/M3=左前(LF), D/M4=左后(LB) */
#define MOTOR_A_ENC_ID               BSP_ENCODER_RB
#define MOTOR_B_ENC_ID               BSP_ENCODER_RF
#define MOTOR_C_ENC_ID               BSP_ENCODER_LF
#define MOTOR_D_ENC_ID               BSP_ENCODER_LB
#define MOTOR_ENC_MAP                { \
    MOTOR_A_ENC_ID, MOTOR_B_ENC_ID, MOTOR_C_ENC_ID, MOTOR_D_ENC_ID \
}

#define ENC_CONFIGS { \
    { ENC_LF_TIMER, ENC_LF_B_PORT, ENC_LF_B_PIN, ENC_LF_DIR_SIGN }, \
    { ENC_LB_TIMER, ENC_LB_B_PORT, ENC_LB_B_PIN, ENC_LB_DIR_SIGN }, \
    { ENC_RF_TIMER, ENC_RF_B_PORT, ENC_RF_B_PIN, ENC_RF_DIR_SIGN }, \
    { ENC_RB_TIMER, ENC_RB_B_PORT, ENC_RB_B_PIN, ENC_RB_DIR_SIGN }, \
}

/* ================================================================
 * 7. ADC配置
 * SysConfig: ADC0, 12�? PA27, VDDA=3.3V
 * ================================================================ */
#define ADC_VOLTAGE_ID               HAL_ADC_VOLTAGE
#define ADC_VREF_MV                   (3300U)
#define ADC_RESOLUTION                (4096U)

/* ================================================================
 * 8. IMU配置
 * ================================================================ */
#define IMU_TASK_PERIOD_MS            (10U)
#define IMU_UART_TELEMETRY_ENABLE     (0U)

/* ================================================================
 * 9. 互补滤波配置
 * ================================================================ */
#define CF_ALPHA                      (0.98f)
#define WHEEL_DIAMETER_MM             (60.0f)
#define WHEEL_RADIUS_M                (WHEEL_DIAMETER_MM * 0.001f * 0.5f)
#define WHEEL_BASE_M                  (0.15f)

/* ================================================================
 * 10. 位置-速度控制配置
 * ================================================================ */
#define POS_PID_KP                    (0.5f)
#define POS_PID_KI                    (0.0f)
#define POS_PID_KD                    (0.0f)

#define YAW_PID_KP                    (2.0f)
#define YAW_PID_KI                    (0.0f)
#define YAW_PID_KD                    (0.0f)

#define PLANNER_ACCEL                 (500.0f)
#define PLANNER_MAX_RPM               (300.0f)
#define REACHED_THRESHOLD_POS         (5.0f)
#define REACHED_THRESHOLD_YAW         (0.5f)
#define REACHED_COUNT                  (10U)
#define MODE_TRANSITION_MS            (1000U)

/* ================================================================
 * 11. 时间频率派生�?
 * (sysconfig未暴�? 需手动维护与sysconfig一致�?
 *
 * BUSCLK = ULPCLK = CPUCLK/2 = 40MHz
 * CAPTURE: BUSCLK/4/(199+1) = 100kHz
 * TIMER_0: BUSCLK/8/(9+1) = 500kHz
 * ================================================================ */
#define CAPTURE_TIMER_FREQ_HZ         (100000UL)
#define SYS_TICK_TIMER_FREQ_HZ        (500000UL)
#define SYS_TICK_US_PER_TICK_X1000 \
    (1000000UL * 1000UL / SYS_TICK_TIMER_FREQ_HZ)
#define ENCODER_RPM_CALC_CONST \
    (60LL * (int64_t)CAPTURE_TIMER_FREQ_HZ)

/* ================================================================
 * 12. 数学常量
 * ================================================================ */
#define PI_F                          (3.14159265358979f)
#define PI_D                          (3.14159265358979323846)
#define TWO_PI_F                      (6.28318530717958647692f)
#define PI_2_F                        (1.57079632679489661923f)
#define RAD2DEG_F                     (57.29577951308232087685f)
#define DEG2RAD_F                     (0.01745329251994329577f)
#define GRAVITY_MS2                   (9.80665f)
#define UINT16_MOD                    (65536U)

/* ================================================================
 * 13. FreeRTOS任务配置
 * ================================================================ */
#define TASK_PRIO_CONTROL             (5U)
#define TASK_PRIO_IMU                 (4U)
#define TASK_PRIO_MENU                (2U)

#define TASK_STACK_CONTROL            (256U)
#define TASK_STACK_IMU                (1280U)
#define TASK_STACK_MENU               (384U)

#define CONTROL_PERIOD_MS             (5U)
#define MENU_POLL_PERIOD_MS           (100U)
#define RPM_OUTPUT_PERIOD_MS           (30U)

/* ================================================================
 * 14. 菜单与VOFA配置
 * ================================================================ */
#define MENU_LINE_BUF_SIZE            (64U)

/* VOFA+ 通信参数 */
#define VOFA_PID_PARAM_MAX           (100.0f)   /* PID参数最大绝对�?*/
#define VOFA_TARGET_RPM_MAX          (800.0f)   /* 目标RPM上限 */
#define VOFA_TELEMETRY_CHANNEL_COUNT (11U)      /* 遥测通道�?*/

/* ================================================================
 * 15. PID默认参数
 * ================================================================ */
#define PID_KP_DEFAULT                (0.8f)
#define PID_KI_DEFAULT                (0.3f)
#define PID_KD_DEFAULT                (0.0f)

#define FF_PID_KP_DEFAULT             (0.5f)
#define FF_PID_KI_DEFAULT             (0.1f)
#define FF_PID_KD_DEFAULT             (0.0f)

/* ================================================================
 * 16. 滤波器编译开�?(�?filter_config.h)
 * 修改后需 Rebuild All。值为1的算法被编译并允许运行时切换�?
 * ================================================================ */
#ifndef FILTER_COMP_ENABLE
#define FILTER_COMP_ENABLE            (0)
#endif
#ifndef FILTER_LPF_ENABLE
#define FILTER_LPF_ENABLE             (0)
#endif
#ifndef FILTER_ESKF_ENABLE
#define FILTER_ESKF_ENABLE            (0)
#endif
#ifndef FILTER_LKF_ENABLE
#define FILTER_LKF_ENABLE             (0)
#endif
#ifndef FILTER_MAHONY_ENABLE
#define FILTER_MAHONY_ENABLE          (0)
#endif
#ifndef FILTER_MADGWICK_ENABLE
#define FILTER_MADGWICK_ENABLE        (0)
#endif
#ifndef FILTER_KF_ENABLE
#define FILTER_KF_ENABLE              (1)
#endif

#ifndef FILTER_DEBUG_VERBOSE
#define FILTER_DEBUG_VERBOSE          (0)
#endif

/* ================================================================
 * 17. 滤波器默认参�?(�?filter_config.h)
 * ================================================================ */

/* 互补滤波�?*/
#define COMP_ALPHA_DEFAULT            (0.98f)
#define COMP_ALPHA_MIN                (0.90f)
#define COMP_ALPHA_MAX                (0.99f)
#define COMP_ALPHA_DEFAULT_DB         (0.98)
#define COMP_ALPHA_INV_DB             (0.02)

/* LPF */
#define LPF_CUTOFF_DEFAULT            (10.0f)
#define LPF_CUTOFF_MIN                (1.0f)
#define LPF_CUTOFF_MAX                (50.0f)

/* EKF/ESKF */
#define EKF_Q_ANGLE_DEFAULT           (0.001f)
#define EKF_Q_ANGLE_MIN               (0.0001f)
#define EKF_Q_ANGLE_MAX               (0.1f)
#define EKF_Q_BIAS_DEFAULT            (0.002f)
#define EKF_Q_BIAS_MIN                (0.0001f)
#define EKF_Q_BIAS_MAX                (0.1f)
#define EKF_R_MEASURE_DEFAULT         (0.001f)
#define EKF_R_MEASURE_MIN             (0.0001f)
#define EKF_R_MEASURE_MAX             (1.0f)
#define EKF_BIAS_LIMIT_DEFAULT        (20.0f)
#define EKF_BIAS_LIMIT_MIN            (5.0f)
#define EKF_BIAS_LIMIT_MAX            (50.0f)
#define EKF_CHI2_THRESHOLD_DEFAULT    (11.34f)
#define EKF_CHI2_THRESHOLD_MIN        (5.0f)
#define EKF_CHI2_THRESHOLD_MAX        (20.0f)
#define EKF_R_ADAPT_ENABLE_DEFAULT    (0)
#define EKF_R_ADAPT_FACTOR_MIN        (0.1f)
#define EKF_R_ADAPT_FACTOR_MAX        (10.0f)

/* Mahony */
#define MAHONY_KP_DEFAULT             (10.0f)
#define MAHONY_KP_MIN                 (0.1f)
#define MAHONY_KP_MAX                 (10.0f)
#define MAHONY_KI_DEFAULT             (0.3f)
#define MAHONY_KI_MIN                 (0.0f)
#define MAHONY_KI_MAX                 (0.5f)
#define MAHONY_ACC_GATE_LOW           (0.9f)
#define MAHONY_ACC_GATE_HIGH          (1.1f)
#define MAHONY_GYRO_SAT_TH            (212.0f)
#define MAHONY_BIAS_LIMIT             (20.0f)
#define MAHONY_STATIC_GYRO_TH         (2.0f)
#define MAHONY_STATIC_ACC_TH          (0.05f)
#define MAHONY_KP_STATIC_BOOST        (1.5f)
#define MAHONY_KP_DYN_ACC_FACTOR      (0.7f)
#define MAHONY_KI_DYN1_FACTOR         (0.1f)
#define MAHONY_KI_DYN2_FACTOR         (0.01f)
#define MAHONY_KI_ACC_POOR            (0.5f)
#define MAHONY_INTEGRAL_LIMIT         (0.5f)
#define KF_ZUPT_AXIS_GATE             (0.5f)

/* Madgwick */
#define MADGWICK_BETA_DEFAULT         (0.5f)
#define MADGWICK_BETA_MIN             (0.001f)
#define MADGWICK_BETA_MAX             (0.5f)

/* KF (纯卡尔曼) */
#define KF_Q_ANGLE_DEFAULT            (0.003f)
#define KF_Q_ANGLE_MIN                (0.0001f)
#define KF_Q_ANGLE_MAX                (0.01f)
#define KF_Q_BIAS_DEFAULT             (0.001f)
#define KF_Q_BIAS_MIN                 (0.0001f)
#define KF_Q_BIAS_MAX                 (0.01f)
#define KF_R_MEASURE_DEFAULT          (0.03f)
#define KF_R_MEASURE_MIN              (0.001f)
#define KF_R_MEASURE_MAX              (0.5f)
#define KF_ANGLE_MIN_DEFAULT         (-180.0f)
#define KF_ANGLE_MAX_DEFAULT          (180.0f)
#define KF_R_ZUPT_DEFAULT             (1e6f)
#define KF_R_ZUPT_MIN                 (0.0001f)
#define KF_R_ZUPT_MAX                 (1e6f)

/* 退化策�?*/
#define DEGRADE_ACC_LOW              (0.5f)
#define DEGRADE_ACC_HIGH             (2.0f)
#define DEGRADE_GYRO_THRESHOLD       (400.0f)
#define DEGRADE_VARIANCE_THRESH      (0.01f)

/* ================================================================
 * 18. MATHACL配置 (�?bsp_mathacl.h)
 *
 * 性能测试: SQRT hw 0.86x(�?, ATAN2 hw 1.26x, SINCOS hw 2.51x
 * ================================================================ */
#ifndef MATHACL_ENABLE
#define MATHACL_ENABLE                (1)
#endif

#if (MATHACL_ENABLE != 0)
  #ifndef MATHACL_ATAN2_HW
  #define MATHACL_ATAN2_HW            (1)
  #endif
  #ifndef MATHACL_SINCOS_HW
  #define MATHACL_SINCOS_HW           (1)
  #endif
#endif

/* ================================================================
 * 19. IMU校准与自适应参数 (�?bsp_lsm6dsr.h)
 * ================================================================ */
#ifndef IMU_CALIB_SAMPLES
#define IMU_CALIB_SAMPLES             (300)
#endif
#ifndef IMU_CALIB_SETTLE_MS
#define IMU_CALIB_SETTLE_MS          (50)
#endif
#ifndef IMU_CALIB_ACC_MAG_REF
#define IMU_CALIB_ACC_MAG_REF        (1.0f)
#endif
#ifndef IMU_CALIB_ACC_MAG_TOL
#define IMU_CALIB_ACC_MAG_TOL        (0.065f)
#endif
#ifndef IMU_CALIB_ACC_DELTA_MAX
#define IMU_CALIB_ACC_DELTA_MAX      (0.08f)
#endif
#ifndef IMU_CALIB_SAMPLE_DELAY_MS
#define IMU_CALIB_SAMPLE_DELAY_MS    (9)
#endif

#ifndef IMU_ACC_VAR_WINDOW
#define IMU_ACC_VAR_WINDOW            (10)
#endif
#ifndef IMU_DT_READ_COMPENSATION_US
#define IMU_DT_READ_COMPENSATION_US  (200U)
#endif
#ifndef IMU_ACC_VAR_THRESHOLD
#define IMU_ACC_VAR_THRESHOLD        (0.0008f)
#endif
#ifndef IMU_ALPHA_MOVING
#define IMU_ALPHA_MOVING              (0.99f)
#endif
#ifndef IMU_ALPHA_STATIONARY
#define IMU_ALPHA_STATIONARY          (0.30f)
#endif
#ifndef IMU_ALPHA_SMOOTH_STEP
#define IMU_ALPHA_SMOOTH_STEP         (0.15f)
#endif

#ifndef IMU_BIAS_STATIONARY_RATE
#define IMU_BIAS_STATIONARY_RATE      (0.1f)
#endif
#ifndef IMU_BIAS_STATIONARY_RATE_Z
#define IMU_BIAS_STATIONARY_RATE_Z    (0.1f)
#endif
#ifndef IMU_GYRO_MOTION_THRESHOLD
#define IMU_GYRO_MOTION_THRESHOLD     (5.0f)
#endif

#ifndef IMU_ODR_ALIGN
#define IMU_ODR_ALIGN                 (0)
#endif
#define IMU_DT_ANOMALY_MIN_S          (0.003)
#define IMU_DT_ANOMALY_MAX_S          (0.030)

/* ================================================================
 * 20. 编译期配置验�?
 * ================================================================ */
#if (MOTOR_DRIVER != MOTOR_DRV8870) && (MOTOR_DRIVER != MOTOR_TB6612)
#error "MOTOR_DRIVER must be MOTOR_DRV8870 or MOTOR_TB6612"
#endif

#if (ENC_PPR == 0U)
#error "ENC_PPR must be greater than zero"
#endif
#if (ENC_GEAR_NUM == 0U) || (ENC_GEAR_DEN == 0U)
#error "Encoder gear ratio numerator and denominator must be > 0"
#endif
#if (ENC_DECODE_MULT != 2U)
#error "Current encoder ISR counts both A-phase edges; multiplier must be 2"
#endif
#if (((ENC_PPR * ENC_GEAR_NUM * ENC_DECODE_MULT) % ENC_GEAR_DEN) != 0U)
#error "Configured PPR and gear ratio do not produce an integer count"
#endif

#if (DRV8870_DEADBAND_LOW == 0U) || (DRV8870_DEADBAND_HIGH >= 100U) || \
    (DRV8870_DEADBAND_LOW >= DRV8870_DEADBAND_HIGH)
#error "DRV8870 deadband must satisfy 0 < low < high < 100"
#endif
#if (DRV8870_NEUTRAL < DRV8870_DEADBAND_LOW) || \
    (DRV8870_NEUTRAL > DRV8870_DEADBAND_HIGH)
#error "DRV8870 neutral duty must lie inside the configured deadband"
#endif
#if (DRV8870_COMMAND_MAX != (DRV8870_PWM_PERIOD / 2U))
#error "DRV8870 backend requires command max equal to half of PWM period"
#endif

#if !(FILTER_COMP_ENABLE || FILTER_LPF_ENABLE || FILTER_ESKF_ENABLE || \
      FILTER_LKF_ENABLE || FILTER_MAHONY_ENABLE || FILTER_MADGWICK_ENABLE || \
      FILTER_KF_ENABLE)
#error "At least one filter implementation must be enabled"
#endif

#if (DRV8870_FACTORY_TEST_ENABLE != 0U) && (MOTOR_DRIVER != MOTOR_DRV8870)
#error "DRV8870 factory-test target requires the DRV8870 motor backend"
#endif

/* ================================================================
 * 21. 旧前缀兼容映射 (PRJ_ -> 新前缀)
 * 保证现有代码无需修改, 新代码使用简化前缀
 * ================================================================ */
#define PRJ_VERSION_MAJOR           PROJECT_VERSION_MAJOR
#define PRJ_VERSION_MINOR           PROJECT_VERSION_MINOR
#define PRJ_VERSION_PATCH           PROJECT_VERSION_PATCH
#define PRJ_VERSION_STRING           PROJECT_VERSION_STRING
#define PRJ_PROTOCOL_VERSION          PROJECT_PROTOCOL_VERSION
#define PRJ_BOARD_NAME               PROJECT_BOARD_NAME
#define PRJ_MOTOR_DRIVER_NAME         PROJECT_MOTOR_DRIVER_NAME

#define PRJ_LED_PORT                 LED_PORT
#define PRJ_LED_PIN                  LED_PIN
#define PRJ_UART_DEBUG_ID            UART_DEBUG_ID

#define PRJ_MOTOR_DRIVER             MOTOR_DRIVER
#define PRJ_MOTOR_DRIVER_DRV8870    MOTOR_DRV8870
#define PRJ_MOTOR_DRIVER_TB6612     MOTOR_TB6612
#define PRJ_MOTOR_COMMAND_MAX        MOTOR_COMMAND_MAX
#define PRJ_MOTOR_A_INSTALL_DIR_SIGN MOTOR_A_DIR_SIGN
#define PRJ_MOTOR_B_INSTALL_DIR_SIGN MOTOR_B_DIR_SIGN
#define PRJ_MOTOR_C_INSTALL_DIR_SIGN MOTOR_C_DIR_SIGN
#define PRJ_MOTOR_D_INSTALL_DIR_SIGN MOTOR_D_DIR_SIGN

#define PRJ_TB6612_PWM_TIMER         TB6612_PWM_TIMER
#define PRJ_TB6612_PWM_CLK_HZ        TB6612_PWM_CLK_HZ
#define PRJ_TB6612_PWM_PERIOD        TB6612_PWM_PERIOD
#define PRJ_TB6612_POWER_STARTUP_MS  TB6612_POWER_STARTUP_MS
#define PRJ_TB6612_STANDBY_CONTROL_ENABLE TB6612_STANDBY_CONTROL_ENABLE
#define PRJ_TB6612_BOARD_CONFIG_AVAILABLE TB6612_BOARD_CONFIG_AVAILABLE
#define PRJ_TB6612_CONFIGS           TB6612_CONFIGS
#define PRJ_TB6612_POWER_CONFIG     TB6612_POWER_CONFIG

#define PRJ_DRV8870_PWM_TIMER        DRV8870_PWM_TIMER
#define PRJ_DRV8870_PWM_CLK_HZ       DRV8870_PWM_CLK_HZ
#define PRJ_DRV8870_PWM_PERIOD       DRV8870_PWM_PERIOD
#define PRJ_DRV8870_SPEED_COMMAND_MAX DRV8870_COMMAND_MAX
#define PRJ_DRV8870_DEADBAND_LOW_PERCENT  DRV8870_DEADBAND_LOW
#define PRJ_DRV8870_NEUTRAL_PERCENT       DRV8870_NEUTRAL
#define PRJ_DRV8870_DEADBAND_HIGH_PERCENT DRV8870_DEADBAND_HIGH
#define PRJ_DRV8870_ZERO_DUTY_OFFSET      DRV8870_ZERO_DUTY_OFFSET
#define PRJ_DRV8870_POWER_PORT       DRV8870_POWER_PORT
#define PRJ_DRV8870_POWER_PIN        DRV8870_POWER_PIN
#define PRJ_DRV8870_POWER_ON_LEVEL    DRV8870_POWER_ON_LEVEL
#define PRJ_DRV8870_POWER_OFF_LEVEL   DRV8870_POWER_OFF_LEVEL
#define PRJ_DRV8870_POWER_STARTUP_MS  DRV8870_POWER_STARTUP_MS
#define PRJ_DRV8870_POWER_SETTLE_MS   DRV8870_POWER_SETTLE_MS
#define PRJ_DRV8870_FACTORY_TEST_ENABLE DRV8870_FACTORY_TEST_ENABLE
#define PRJ_DRV8870_A_PWM_CH         DRV8870_A_PWM_CH
#define PRJ_DRV8870_B_PWM_CH         DRV8870_B_PWM_CH
#define PRJ_DRV8870_C_PWM_CH         DRV8870_C_PWM_CH
#define PRJ_DRV8870_D_PWM_CH         DRV8870_D_PWM_CH
#define PRJ_DRV8870_A_DIR_SIGN       MOTOR_A_DIR_SIGN
#define PRJ_DRV8870_B_DIR_SIGN       MOTOR_B_DIR_SIGN
#define PRJ_DRV8870_C_DIR_SIGN       MOTOR_C_DIR_SIGN
#define PRJ_DRV8870_D_DIR_SIGN       MOTOR_D_DIR_SIGN
#define PRJ_DRV8870_CONFIGS          DRV8870_CONFIGS

#define PRJ_MOTOR_PWM_TIMER          MOTOR_PWM_TIMER
#define PRJ_MOTOR_PWM_CLK_HZ         MOTOR_PWM_CLK_HZ
#define PRJ_MOTOR_PWM_PERIOD         MOTOR_PWM_PERIOD
#define PRJ_MOTOR_POWER_STARTUP_MS   MOTOR_POWER_STARTUP_MS
#define PRJ_MOTOR_POWER_SETTLE_MS    MOTOR_POWER_SETTLE_MS

#define PRJ_MOTOR_ENCODER_PPR        ENC_PPR
#define PRJ_MOTOR_GEAR_RATIO_NUMERATOR ENC_GEAR_NUM
#define PRJ_MOTOR_GEAR_RATIO_DENOMINATOR ENC_GEAR_DEN
#define PRJ_ENCODER_DECODE_MULTIPLIER ENC_DECODE_MULT
#define PRJ_MOTOR_OUTPUT_PULSES_PER_REV ENC_OUTPUT_PPR
#define PRJ_ENCODER_PULSES_PER_REV  ENC_PULSES_PER_REV
#define PRJ_ENCODER_LF_TIMER         ENC_LF_TIMER
#define PRJ_ENCODER_LB_TIMER         ENC_LB_TIMER
#define PRJ_ENCODER_RF_TIMER         ENC_RF_TIMER
#define PRJ_ENCODER_RB_TIMER         ENC_RB_TIMER
#define PRJ_ENCODER_LF_IRQ_HANDLER   ENC_LF_IRQ_HANDLER
#define PRJ_ENCODER_LB_IRQ_HANDLER   ENC_LB_IRQ_HANDLER
#define PRJ_ENCODER_RF_IRQ_HANDLER   ENC_RF_IRQ_HANDLER
#define PRJ_ENCODER_RB_IRQ_HANDLER   ENC_RB_IRQ_HANDLER
#define PRJ_ENCODER_LF_B_PORT        ENC_LF_B_PORT
#define PRJ_ENCODER_LB_B_PORT        ENC_LB_B_PORT
#define PRJ_ENCODER_RF_B_PORT        ENC_RF_B_PORT
#define PRJ_ENCODER_RB_B_PORT        ENC_RB_B_PORT
#define PRJ_ENCODER_LF_B_PIN        ENC_LF_B_PIN
#define PRJ_ENCODER_LB_B_PIN        ENC_LB_B_PIN
#define PRJ_ENCODER_RF_B_PIN        ENC_RF_B_PIN
#define PRJ_ENCODER_RB_B_PIN        ENC_RB_B_PIN
#define PRJ_ENCODER_LF_DIR_SIGN      ENC_LF_DIR_SIGN
#define PRJ_ENCODER_LB_DIR_SIGN      ENC_LB_DIR_SIGN
#define PRJ_ENCODER_RF_DIR_SIGN      ENC_RF_DIR_SIGN
#define PRJ_ENCODER_RB_DIR_SIGN      ENC_RB_DIR_SIGN
#define PRJ_MOTOR_A_ENCODER_ID      MOTOR_A_ENC_ID
#define PRJ_MOTOR_B_ENCODER_ID      MOTOR_B_ENC_ID
#define PRJ_MOTOR_C_ENCODER_ID      MOTOR_C_ENC_ID
#define PRJ_MOTOR_D_ENCODER_ID      MOTOR_D_ENC_ID
#define PRJ_MOTOR_ENCODER_MAP        MOTOR_ENC_MAP
#define PRJ_ENCODER_CONFIGS          ENC_CONFIGS

#define PRJ_ADC_VOLTAGE_ID           ADC_VOLTAGE_ID
#define PRJ_ADC_VREF_MV              ADC_VREF_MV
#define PRJ_ADC_RESOLUTION           ADC_RESOLUTION

#define PRJ_IMU_TASK_PERIOD_MS       IMU_TASK_PERIOD_MS
#define PRJ_IMU_UART_TELEMETRY_ENABLE IMU_UART_TELEMETRY_ENABLE

#define PRJ_CF_ALPHA                 CF_ALPHA
#define PRJ_MOTOR_WHEEL_DIAMETER_MM  WHEEL_DIAMETER_MM
#define PRJ_CF_WHEEL_RADIUS_M        WHEEL_RADIUS_M
#define PRJ_CF_WHEEL_BASE_M          WHEEL_BASE_M

#define PRJ_SYS_TICK_TIMER           SYS_TICK_TIMER
#define PRJ_PI_F                      PI_F
#define PRJ_PI_D                      PI_D
#define PRJ_TWO_PI_F                  TWO_PI_F
#define PRJ_PI_2_F                    PI_2_F
#define PRJ_RAD2DEG_F                 RAD2DEG_F
#define PRJ_DEG2RAD_F                 DEG2RAD_F
#define PRJ_GRAVITY_MS2               GRAVITY_MS2
#define PRJ_UINT16_MOD                UINT16_MOD
#define PRJ_MS_PER_S                  MS_PER_S
#define PRJ_MS_PER_MIN                MS_PER_MIN
#define PRJ_US_PER_MS                 US_PER_MS

#define PRJ_CAPTURE_TIMER_FREQ_HZ     CAPTURE_TIMER_FREQ_HZ
#define PRJ_SYS_TICK_TIMER_FREQ_HZ   SYS_TICK_TIMER_FREQ_HZ
#define PRJ_SYS_TICK_US_PER_TICK_X1000 SYS_TICK_US_PER_TICK_X1000
#define PRJ_ENCODER_RPM_CALC_CONST   ENCODER_RPM_CALC_CONST

#define BSP_MATHACL_ENABLE           MATHACL_ENABLE
#define BSP_MATHACL_ATAN2_HW         MATHACL_ATAN2_HW
#define BSP_MATHACL_SINCOS_HW        MATHACL_SINCOS_HW

/* BLE ����ӳ�� */
#define PRJ_UART_BLE_ID              UART_BLE_ID
#define PRJ_JDY23_UART_BAUD          JDY23_UART_BAUD
#define PRJ_BLE_MENU_ENABLE           BLE_MENU_ENABLE
#define PRJ_BLE_MENU_CONSOLE_ENABLE  BLE_MENU_CONSOLE_ENABLE

#ifdef __cplusplus
}
#endif

#endif /* PROJECT_CONFIG_H */
