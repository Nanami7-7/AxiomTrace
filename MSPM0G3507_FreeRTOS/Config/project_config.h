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
#define PRJ_LED_PIN             LED_A27_PIN

/* ================================================================
 *  调试UART配置
 *  SysConfig已配置: UART0, TX=PA10/PINCM21, RX=PA11/PINCM22
 *  波特率: 115200, 时钟: 40MHz
 * ================================================================ */

/** 调试串口HAL实例 */
#define PRJ_UART_DEBUG_ID       HAL_UART_DEBUG

/* ================================================================
 *  电机PWM配置 (TB6612 - 默认禁用)
 *  SysConfig已配置: TIMA0, 4通道PWM, 20MHz时钟, 20kHz周期
 *  每个电机1个PWM通道(控速) + 2个GPIO(IN1/IN2控方向)
 *  通道: C0=PA8/PINCM19, C1=PA9/PINCM20, C2=PB17/PINCM43, C3=PB2/PINCM15
 *  驱动芯片: TB6612FNG
 *  正转: IN1=1, IN2=0, PWM=duty
 *  反转: IN1=0, IN2=1, PWM=duty
 *  刹车: IN1=1, IN2=1, PWM=0
 *  滑行: IN1=0, IN2=0, PWM=0
 * ================================================================ */
/* TB6612 驱动默认禁用, 改用 DRV8870 锁相驱动.
 * 启用方式: 定义 BSP_MOTOR_ENABLE 并恢复 ti_msp_dl_config.h
 * 中的 MOTOR_AIN1~DIN2 宏定义. */
#ifdef BSP_MOTOR_ENABLE

/** 电机PWM定时器HAL实例 */
#define PRJ_MOTOR_PWM_TIMER     HAL_TIMER_PWM_MOTOR
/** PWM时钟频率(Hz) - 引用 sysconfig 暴露的宏，避免硬编码 */
#define PRJ_MOTOR_PWM_CLK_HZ    ((unsigned long)(PWM_MOTOR_INST_CLK_FREQ))
/** PWM周期值(20kHz = 1000个20MHz时钟周期) */
#define PRJ_MOTOR_PWM_PERIOD    (1000U)

/* ---- 电机A(左前): CC0=PB8, IN1=PB24, IN2=PB20 ---- */
#define PRJ_MOTOR_A_PWM_CH      (0U)
#define PRJ_MOTOR_A_IN1_PORT    HAL_GPIO_PORT_B
#define PRJ_MOTOR_A_IN1_PIN     MOTOR_AIN1_PIN
#define PRJ_MOTOR_A_IN2_PORT    HAL_GPIO_PORT_B
#define PRJ_MOTOR_A_IN2_PIN     MOTOR_AIN2_PIN

/* ---- 电机B(左后): CC1=PA22, IN1=PA24, IN2=PA31 ---- */
#define PRJ_MOTOR_B_PWM_CH      (1U)
#define PRJ_MOTOR_B_IN1_PORT    HAL_GPIO_PORT_A
#define PRJ_MOTOR_B_IN1_PIN     MOTOR_BIN1_PIN
#define PRJ_MOTOR_B_IN2_PORT    HAL_GPIO_PORT_A
#define PRJ_MOTOR_B_IN2_PIN     MOTOR_BIN2_PIN

/* ---- 电机C(右前): CC2=PA15, IN1=PA2, IN2=PA7 ---- */
#define PRJ_MOTOR_C_PWM_CH      (2U)
#define PRJ_MOTOR_C_IN1_PORT    HAL_GPIO_PORT_A
#define PRJ_MOTOR_C_IN1_PIN     MOTOR_CIN1_PIN
#define PRJ_MOTOR_C_IN2_PORT    HAL_GPIO_PORT_A
#define PRJ_MOTOR_C_IN2_PIN     MOTOR_CIN2_PIN

/* ---- 电机D(右后): CC3=PA17, IN1=PB6, IN2=PB7 ---- */
#define PRJ_MOTOR_D_PWM_CH      (3U)
#define PRJ_MOTOR_D_IN1_PORT    HAL_GPIO_PORT_B
#define PRJ_MOTOR_D_IN1_PIN     MOTOR_DIN1_PIN
#define PRJ_MOTOR_D_IN2_PORT    HAL_GPIO_PORT_B
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

#endif /* BSP_MOTOR_ENABLE */

/* ================================================================
 *  编码器捕获配置
 *  SysConfig已配置: TIMG7/TIMA1/TIMG6/TIMG0, 组合捕获模式(脉宽+周期)
 * ================================================================ */

/**
 * 电机与编码器机械参数。
 *
 * PPR定义为编码器A相在电机轴旋转一圈时的完整脉冲周期数；当前捕获逻辑
 * 同时统计A相上升沿和下降沿，因此解码倍频固定为2。减速比用分数表示，
 * 可准确配置20:1、30:1或298:11等非整数标称减速比。
 */
#define PRJ_MOTOR_ENCODER_PPR              (13U)
#define PRJ_MOTOR_GEAR_RATIO_NUMERATOR     (20U)
#define PRJ_MOTOR_GEAR_RATIO_DENOMINATOR   (1U)
#define PRJ_ENCODER_DECODE_MULTIPLIER      (2U)

#if (PRJ_MOTOR_ENCODER_PPR == 0U)
#error "PRJ_MOTOR_ENCODER_PPR must be greater than zero"
#endif
#if (PRJ_MOTOR_GEAR_RATIO_NUMERATOR == 0U) || \
    (PRJ_MOTOR_GEAR_RATIO_DENOMINATOR == 0U)
#error "Motor gear-ratio numerator and denominator must be greater than zero"
#endif
#if (PRJ_ENCODER_DECODE_MULTIPLIER != 2U)
#error "Current encoder ISR counts both A-phase edges; multiplier must remain 2"
#endif
#if (((PRJ_MOTOR_ENCODER_PPR * PRJ_MOTOR_GEAR_RATIO_NUMERATOR * \
       PRJ_ENCODER_DECODE_MULTIPLIER) % \
      PRJ_MOTOR_GEAR_RATIO_DENOMINATOR) != 0U)
#error "Configured PPR and gear ratio do not produce an integer output-shaft count"
#endif

/** 输出轴每转计数，用于位置和RPM换算。 */
#define PRJ_MOTOR_OUTPUT_PULSES_PER_REV \
    ((PRJ_MOTOR_ENCODER_PPR * PRJ_MOTOR_GEAR_RATIO_NUMERATOR * \
      PRJ_ENCODER_DECODE_MULTIPLIER) / \
     PRJ_MOTOR_GEAR_RATIO_DENOMINATOR)

/** 兼容现有编码器BSP调用。 */
#define PRJ_ENCODER_PULSES_PER_REV  PRJ_MOTOR_OUTPUT_PULSES_PER_REV

/** 左前编码器HAL实例 */
#define PRJ_ENCODER_LF_TIMER    HAL_TIMER_CAPTURE_LF
/** 左后编码器HAL实例 */
#define PRJ_ENCODER_LB_TIMER    HAL_TIMER_CAPTURE_LB
/** 右前编码器HAL实例 */
#define PRJ_ENCODER_RF_TIMER    HAL_TIMER_CAPTURE_RF
/** 右后编码器HAL实例 */
#define PRJ_ENCODER_RB_TIMER    HAL_TIMER_CAPTURE_RB

/**
 * @brief SysConfig-generated encoder timer ISR mapping.
 * @note  The logical wheel order retains its A-phase capture timer allocation:
 *        LF TIMG7/M3, LB TIMA1/M4, RF TIMG6/M2, RB TIMG0/M1.
 */
#define PRJ_ENCODER_LF_IRQ_HANDLER  M3_INST_IRQHandler
#define PRJ_ENCODER_LB_IRQ_HANDLER  M4_INST_IRQHandler
#define PRJ_ENCODER_RF_IRQ_HANDLER  M2_INST_IRQHandler
#define PRJ_ENCODER_RB_IRQ_HANDLER  M1_INST_IRQHandler
/** 编码器B相端口(SysConfig已配置) */
#define PRJ_ENCODER_LF_B_PORT        HAL_GPIO_PORT_A
#define PRJ_ENCODER_LB_B_PORT        HAL_GPIO_PORT_A
#define PRJ_ENCODER_RF_B_PORT        HAL_GPIO_PORT_A
#define PRJ_ENCODER_RB_B_PORT        HAL_GPIO_PORT_A
/** Left-front encoder B phase: PA25 (SysConfig M3_B). */
#define PRJ_ENCODER_LF_B_PIN    ENCODER_M3_B_PIN
/** Left-back encoder B phase: PA4 (SysConfig M4_B). */
#define PRJ_ENCODER_LB_B_PIN    ENCODER_M4_B_PIN
/** Right-front encoder B phase: PA14 (SysConfig M2_B). */
#define PRJ_ENCODER_RF_B_PIN    ENCODER_M2_B_PIN
/** Right-back encoder B phase: PA13 (SysConfig M1_B). */
#define PRJ_ENCODER_RB_B_PIN    ENCODER_M1_B_PIN

/**
 * 编码器安装方向修正：车体前进时四路编码器RPM应统一为正。
 * 若只更换某一路电机/编码器安装方向，只修改对应宏，不改ISR判向逻辑。
 */
#define PRJ_ENCODER_LF_DIR_SIGN (-1)
#define PRJ_ENCODER_LB_DIR_SIGN (-1)
#define PRJ_ENCODER_RF_DIR_SIGN (+1)
#define PRJ_ENCODER_RB_DIR_SIGN (+1)

/**
 * 电机输出通道与编码器反馈的物理对应关系。
 * A/M1=右后(RB)，B/M2=右前(RF)，C/M3=左前(LF)，D/M4=左后(LB)。
 * task_control.c据此将编码器反馈和位置控制目标的车轮顺序
 * (LF/LB/RF/RB)统一重排为电机顺序(A/B/C/D)。
 */
#define PRJ_MOTOR_A_ENCODER_ID  BSP_ENCODER_RB
#define PRJ_MOTOR_B_ENCODER_ID  BSP_ENCODER_RF
#define PRJ_MOTOR_C_ENCODER_ID  BSP_ENCODER_LF
#define PRJ_MOTOR_D_ENCODER_ID  BSP_ENCODER_LB
#define PRJ_MOTOR_ENCODER_MAP { \
    PRJ_MOTOR_A_ENCODER_ID, PRJ_MOTOR_B_ENCODER_ID, \
    PRJ_MOTOR_C_ENCODER_ID, PRJ_MOTOR_D_ENCODER_ID \
}

/** 编码器配置表(顺序需与BSP_ENCODER_x一致) */
#define PRJ_ENCODER_CONFIGS { \
		{ PRJ_ENCODER_LF_TIMER, PRJ_ENCODER_LF_B_PORT, PRJ_ENCODER_LF_B_PIN, \
			PRJ_ENCODER_LF_DIR_SIGN }, \
		{ PRJ_ENCODER_LB_TIMER, PRJ_ENCODER_LB_B_PORT, PRJ_ENCODER_LB_B_PIN, \
			PRJ_ENCODER_LB_DIR_SIGN }, \
		{ PRJ_ENCODER_RF_TIMER, PRJ_ENCODER_RF_B_PORT, PRJ_ENCODER_RF_B_PIN, \
			PRJ_ENCODER_RF_DIR_SIGN }, \
		{ PRJ_ENCODER_RB_TIMER, PRJ_ENCODER_RB_B_PORT, PRJ_ENCODER_RB_B_PIN, \
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
 *  硬件SPI配置 (LSM6DSR)
 *  SPI1: SCK=PB9, MOSI=PA18, MISO=PA16, CS=PA25(独立GPIO)
 *  注意: SPI 引脚由 SysConfig 配置，spi_bridge.c 使用 ti_msp_dl_config.h 定义
 * ================================================================ */

/* ================================================================
 *  软件I2C配置 (已弃用，替换为硬件SPI)
 *  配置已移除，保留注释供历史参考
 * ================================================================ */

/* MPU6050 配置已移除，替换为 LSM6DSR */

/* ================================================================
 *  LSM6DSR六轴传感器配置
 *  通过硬件SPI接口通信(SPI1: SCK=PB9, MOSI=PA18, MISO=PA16, CS=PA25)
 *  注意: 需要在SysConfig中配置SPI1外设
 *  说明: 量程/采样率/滤波器类型由 bsp_lsm6dsr.c 直接使用
 *        lsm6dsr.h 中的枚举常量, 此处不重复定义
 * ================================================================ */

/* ================================================================
 *  IMU任务配置
 * ================================================================ */

/** IMU采集任务周期(ms), 100Hz */
#define PRJ_IMU_TASK_PERIOD_MS       (10U)

/**
 * @brief Background IMU CSV telemetry on the shared UART0.
 * @note  Disabled by default: IMU sampling and filtering continue, but no
 *        periodic DMA frame is injected into CLI/diagnostic output.
 *        Re-enable only after the UART TX arbitration work package is verified.
 */
#define PRJ_IMU_UART_TELEMETRY_ENABLE (0U)
/* IMU任务优先级与栈大小由 app_main.h 中 APP_TASK_PRIORITY_IMU /
 * APP_TASK_STACK_IMU 统一管理, 此处不重复定义 */

/* ================================================================
 *  互补滤波器配置
 * ================================================================ */

/** 互补滤波系数(0~1, 0=全信任IMU, 1=全信任编码器) */
#define PRJ_CF_ALPHA                 (0.98f)
/** 轮胎外径(mm)，应以负载状态下的有效滚动直径标定。 */
#define PRJ_MOTOR_WHEEL_DIAMETER_MM  (60.0f)
/** 轮子有效滚动半径(m)，由轮径统一派生，避免重复配置。 */
#define PRJ_CF_WHEEL_RADIUS_M \
    (PRJ_MOTOR_WHEEL_DIAMETER_MM * 0.001f * 0.5f)
/** 轮距(m, 左右轮接地点中心距离)，应按实车标定。 */
#define PRJ_CF_WHEEL_BASE_M          (0.15f)

/* ================================================================
 *  位置-速度串级控制配置
 *  用于 app_position_control.c, 在速度环(5ms)之上增加
 *  位置环/角度环(20ms), 实现精准定位和转向控制
 * ================================================================ */

/** 位置环PID参数(位置式PID, 输出RPM修正) */
#define APP_POS_PID_KP              (0.5f)
#define APP_POS_PID_KI              (0.0f)
#define APP_POS_PID_KD              (0.0f)

/** 角度环PID参数(位置式PID, 输出差速RPM) */
#define APP_YAW_PID_KP              (2.0f)
#define APP_YAW_PID_KI              (0.0f)
#define APP_YAW_PID_KD              (0.0f)

/** 规划器加速度(RPM/s, 控制加减速平滑度) */
#define APP_PLANNER_ACCEL           (500.0f)

/** 最大目标RPM(速度限幅, 防止过速) */
#define APP_PLANNER_MAX_RPM         (300.0f)

/** 到位判定阈值(位置:脉冲, 角度:度) */
#define APP_REACHED_THRESHOLD_POS   (5.0f)
#define APP_REACHED_THRESHOLD_YAW   (0.5f)

/** 到位持续周期数(20ms×10=200ms) */
#define APP_REACHED_COUNT           (10U)

/** 模式切换过渡时长(ms, 1秒渐变) */
#define APP_MODE_TRANSITION_MS      (1000U)

/* ================================================================
 *  系统参数
 * ================================================================ */

/** 系统定时器HAL实例 */
#define PRJ_SYS_TICK_TIMER      HAL_TIMER_SYS_TICK

/* ================================================================
 *  数学常量
 * ================================================================ */

/** 圆周率(float精度, 供应用层避免魔数 3.14159265f) */
#define PRJ_PI_F                (3.14159265358979f)
/** 圆周率(double精度, 供滤波器等需要double精度的模块使用) */
#define PRJ_PI_D                (3.14159265358979323846)
/** 2π(float精度) */
#define PRJ_TWO_PI_F            (6.28318530717958647692f)
/** π/2(float精度) */
#define PRJ_PI_2_F              (1.57079632679489661923f)
/** 弧度→角度转换系数(float): 180/π */
#define PRJ_RAD2DEG_F           (57.29577951308232087685f)
/** 角度→弧度转换系数(float): π/180 */
#define PRJ_DEG2RAD_F           (0.01745329251994329577f)

/* ================================================================
 *  时间转换常量(无符号整型, 用于避免魔数 1000/60000 等)
 *  使用浮点上下文时需显式 (float) cast
 * ================================================================ */

/** 每秒毫秒数 */
#define PRJ_MS_PER_S            (1000U)
/** 每分钟毫秒数(60s × 1000ms) */
#define PRJ_MS_PER_MIN          (60000U)
/** 每毫秒微秒数 */
#define PRJ_US_PER_MS           (1000U)

/** 标准重力加速度 m/s² (float精度) */
#define PRJ_GRAVITY_MS2         (9.80665f)

/** 16位无符号整数模数 (2^16), 用于定时器计数器回绕修正 */
#define PRJ_UINT16_MOD          (65536U)

/* ================================================================
 *  派生频率宏（sysconfig 未暴露，需开发者手动维护与 sysconfig 一致性）
 *
 *  以下频率值在 ti_msp_dl_config.c 中以注释形式存在，但 sysconfig
 *  未将其作为 #define 暴露在 ti_msp_dl_config.h 中。此处集中定义，
 *  供应用层引用，避免硬编码魔数。
 *
 *  ⚠️ 维护规则：修改 sysconfig 中对应定时器的分频/预分频后，
 *     必须同步更新以下宏的值，并重新验证依赖此宏的所有代码。
 *
 *  计算依据（来自 ti_msp_dl_config.c 注释）：
 *    BUSCLK = ULPCLK = CPUCLK/2 = 40MHz
 *    CAPTURE timer: BUSCLK/4/(199+1) = 100kHz
 *    TIMER_0 (TIMG8): BUSCLK/8/(9+1) = 500kHz
 * ================================================================ */

/**
 * 编码器捕获定时器实际频率(Hz)
 * 来源: ti_msp_dl_config.c 中 CAPTURE_* 的 divideRatio=DIVIDE_4, prescale=199
 * 计算: BUSCLK(40MHz) / 4 / (199+1) = 100000 Hz
 * 用途: bsp_encoder.c / app_debug.c 的 M/T 法 RPM 计算
 * 依赖: 6000000LL = 60 × PRJ_CAPTURE_TIMER_FREQ_HZ
 *
 * ⚠️ sysconfig 修改 CAPTURE_* 的分频/prescale 后必须更新此值
 */
#define PRJ_CAPTURE_TIMER_FREQ_HZ   (100000UL)

/**
 * 系统微秒计时器实际频率(Hz)
 * 来源: ti_msp_dl_config.c 中 TIMER_0 (TIMG8) 的 divideRatio=DIVIDE_8, prescale=9
 * 计算: BUSCLK(40MHz) / 8 / (9+1) = 500000 Hz (2us/tick)
 * 用途: platform_mspm0.c get_tick_us() 的微秒换算
 * 依赖: tick_to_us = count / (PRJ_SYS_TICK_TIMER_FREQ_HZ / 1000000UL)
 *        即 count * 2U (当前硬编码)
 *
 * ⚠️ sysconfig 修改 TIMER_0 的分频/prescale 后必须更新此值
 */
#define PRJ_SYS_TICK_TIMER_FREQ_HZ  (500000UL)

/**
 * 微秒计时器: 1 个 tick 对应的微秒数(×1000 扩大精度避免浮点)
 * 计算: 1000000 / PRJ_SYS_TICK_TIMER_FREQ_HZ = 2 (即 2us/tick)
 * 用途: platform_mspm0.c:85 替换硬编码 * 2U
 *
 * ⚠️ 与 PRJ_SYS_TICK_TIMER_FREQ_HZ 联动，修改一处需同步检查
 */
#define PRJ_SYS_TICK_US_PER_TICK_X1000  \
    (1000000UL * 1000UL / PRJ_SYS_TICK_TIMER_FREQ_HZ)

/**
 * 编码器 M/T 法 RPM 计算常数
 * 计算: 60 × PRJ_CAPTURE_TIMER_FREQ_HZ = 60 × 100000 = 6000000
 * 用途: bsp_encoder.c / app_debug.c 中 RPM = (delta × 60 × timer_freq) / (pulses × period)
 *
 * ⚠️ 与 PRJ_CAPTURE_TIMER_FREQ_HZ 联动
 */
#define PRJ_ENCODER_RPM_CALC_CONST  \
    (60LL * (int64_t)PRJ_CAPTURE_TIMER_FREQ_HZ)

/* ================================================================
 *  DRV8870 电机驱动配置 (锁相驱动 Locked Anti-Phase)
 *  与 TB6612 驱动并存, 可通过编译宏切换使用
 *  SysConfig已配置: TIMA0, 4通道PWM, 20MHz时钟, 20kHz周期
 *  通道: C0=PA8, C1=PA9, C2=PB17, C3=PB2
 *  驱动芯片: DRV8870DDAR (锁相驱动)
 *  硬件拓扑: MCU PWM → DRV8870 IN1(直连) + S8050反相器 → IN2
 *  正转有效区: PWM占空比 > PRJ_DRV8870_DEADBAND_HIGH_PERCENT
 *  反转有效区: PWM占空比 < PRJ_DRV8870_DEADBAND_LOW_PERCENT
 *  停止死区: 40%~55%，零命令输出50%中性占空比
 *  无需方向引脚(IN1/IN2由硬件反相器自动生成互补信号)
 * ================================================================ */

/** DRV8870 PWM定时器HAL实例(复用TIMA0) */
#define PRJ_DRV8870_PWM_TIMER     HAL_TIMER_PWM_MOTOR
/** PWM时钟频率(Hz) - 引用 sysconfig 暴露的宏 */
#define PRJ_DRV8870_PWM_CLK_HZ    ((unsigned long)(PWM_MOTOR_INST_CLK_FREQ))
/** PWM周期值(20kHz = 1000个20MHz时钟周期) */
#define PRJ_DRV8870_PWM_PERIOD    (1000U)
/** 有符号速度命令最大绝对值；业务层、PID和模型辨识均应保持一致。 */
#define PRJ_DRV8870_SPEED_COMMAND_MAX \
    (PRJ_DRV8870_PWM_PERIOD / 2U)

/**
 * 实测机械死区边界（绝对PWM占空比百分数）。
 * 0%~39.9%为反转有效区，40%~55%为停止死区，55.1%~100%为正转有效区。
 * bsp_drv8870_set_speed()会把非零有符号命令分段映射到死区之外；
 * 工厂示波器接口仍是原始绝对compare，可直接进入死区用于测量。
 */
#define PRJ_DRV8870_DEADBAND_LOW_PERCENT     (40U)
#define PRJ_DRV8870_NEUTRAL_PERCENT          (50U)
#define PRJ_DRV8870_DEADBAND_HIGH_PERCENT    (55U)

#if (PRJ_DRV8870_DEADBAND_LOW_PERCENT == 0U) || \
    (PRJ_DRV8870_DEADBAND_HIGH_PERCENT >= 100U) || \
    (PRJ_DRV8870_DEADBAND_LOW_PERCENT >= \
     PRJ_DRV8870_DEADBAND_HIGH_PERCENT)
#error "DRV8870 deadband must satisfy 0 < low < high < 100"
#endif
#if (PRJ_DRV8870_NEUTRAL_PERCENT < PRJ_DRV8870_DEADBAND_LOW_PERCENT) || \
    (PRJ_DRV8870_NEUTRAL_PERCENT > PRJ_DRV8870_DEADBAND_HIGH_PERCENT)
#error "DRV8870 neutral duty must lie inside the configured deadband"
#endif

/** Motor power gate: PB19 is active-high; SysConfig initializes it low. */
#define PRJ_DRV8870_POWER_PORT       HAL_GPIO_PORT_B
#define PRJ_DRV8870_POWER_PIN        POWER_pb19_PIN
#define PRJ_DRV8870_POWER_ON_LEVEL   true
#define PRJ_DRV8870_POWER_OFF_LEVEL  false
/** Allow VIN_OUT/DRV8870 to settle after enable and before power-off. */
#define PRJ_DRV8870_POWER_STARTUP_MS  (20U)
#define PRJ_DRV8870_POWER_SETTLE_MS   (5U)

/*
 * 零点偏移补偿 (S8050 反相器开关不对称 + DRV8870 传播延迟)
 * 文档参考: DRV8870技术文档 §3.2.2, 典型偏移 2~5 步
 * 需实测标定: 找到使电机恰好静止的 duty 值, 减去 PWM_PERIOD/2
 */
#define PRJ_DRV8870_ZERO_DUTY_OFFSET  (0)

/*
 * DRV8870 工厂硬件脉冲测试闸门。默认关闭，防止菜单/调试命令在
 * 非受控环境下驱动电机。仅在电机悬空或车体可靠支撑、实验室电源
 * 已限流、示波器/电流观测准备完成时，才可临时改为 1 并单独编译。
 * 正式运行固件必须保持 0。
 */
#ifndef PRJ_DRV8870_FACTORY_TEST_ENABLE
#define PRJ_DRV8870_FACTORY_TEST_ENABLE       (0U)
#endif

/* ---- 电机A/M1(右后): CC0=PA8 ---- */
#define PRJ_DRV8870_A_PWM_CH      (0U)

/* ---- 电机B/M2(右前): CC1=PA9 ---- */
#define PRJ_DRV8870_B_PWM_CH      (1U)

/* ---- 电机C/M3(左前): CC2=PB17 ---- */
#define PRJ_DRV8870_C_PWM_CH      (2U)

/* ---- 电机D/M4(左后): CC3=PB2 ---- */
#define PRJ_DRV8870_D_PWM_CH      (3U)

/**
 * 电机安装方向修正：正速度命令必须对应车体前进方向。
 * 某一路正命令反向时只把该路改为-1；不要同时交换编码器符号。
 */
#define PRJ_MOTOR_A_INSTALL_DIR_SIGN  (+1)
#define PRJ_MOTOR_B_INSTALL_DIR_SIGN  (+1)
#define PRJ_MOTOR_C_INSTALL_DIR_SIGN  (+1)
#define PRJ_MOTOR_D_INSTALL_DIR_SIGN  (+1)

/** 兼容原DRV8870配置宏名称。 */
#define PRJ_DRV8870_A_DIR_SIGN  PRJ_MOTOR_A_INSTALL_DIR_SIGN
#define PRJ_DRV8870_B_DIR_SIGN  PRJ_MOTOR_B_INSTALL_DIR_SIGN
#define PRJ_DRV8870_C_DIR_SIGN  PRJ_MOTOR_C_INSTALL_DIR_SIGN
#define PRJ_DRV8870_D_DIR_SIGN  PRJ_MOTOR_D_INSTALL_DIR_SIGN

/** DRV8870 电机配置表(顺序需与BSP_DRV8870_x一致) */
#define PRJ_DRV8870_CONFIGS { \
    { PRJ_DRV8870_A_PWM_CH, PRJ_DRV8870_A_DIR_SIGN, \
      PRJ_DRV8870_ZERO_DUTY_OFFSET, \
      PRJ_DRV8870_DEADBAND_LOW_PERCENT, PRJ_DRV8870_NEUTRAL_PERCENT, \
      PRJ_DRV8870_DEADBAND_HIGH_PERCENT }, \
    { PRJ_DRV8870_B_PWM_CH, PRJ_DRV8870_B_DIR_SIGN, \
      PRJ_DRV8870_ZERO_DUTY_OFFSET, \
      PRJ_DRV8870_DEADBAND_LOW_PERCENT, PRJ_DRV8870_NEUTRAL_PERCENT, \
      PRJ_DRV8870_DEADBAND_HIGH_PERCENT }, \
    { PRJ_DRV8870_C_PWM_CH, PRJ_DRV8870_C_DIR_SIGN, \
      PRJ_DRV8870_ZERO_DUTY_OFFSET, \
      PRJ_DRV8870_DEADBAND_LOW_PERCENT, PRJ_DRV8870_NEUTRAL_PERCENT, \
      PRJ_DRV8870_DEADBAND_HIGH_PERCENT }, \
    { PRJ_DRV8870_D_PWM_CH, PRJ_DRV8870_D_DIR_SIGN, \
      PRJ_DRV8870_ZERO_DUTY_OFFSET, \
      PRJ_DRV8870_DEADBAND_LOW_PERCENT, PRJ_DRV8870_NEUTRAL_PERCENT, \
      PRJ_DRV8870_DEADBAND_HIGH_PERCENT }, \
}

#ifdef __cplusplus
}
#endif

#endif /* PROJECT_CONFIG_H */
