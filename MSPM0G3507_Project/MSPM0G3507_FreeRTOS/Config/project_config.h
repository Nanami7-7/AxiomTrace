/**
 * @file    project_config.h
 * @brief   椤圭洰纭欢閰嶇疆闆嗕腑瀹氫箟
 * @note    鎵€鏈夊紩鑴氭槧灏勩€佸璁惧疄渚嬪垎閰嶃€佺‖浠跺弬鏁板湪姝ら泦涓畾涔夈€?
 *          鏇存崲纭欢/寮曡剼浠呴渶淇敼姝ゆ枃浠讹紝鏃犻渶鏀瑰姩椹卞姩浠ｇ爜銆?
 *          寮曡剼缂栧彿鏉ユ簮浜巘i_msp_dl_config.h(SysConfig鐢熸垚)
 */
#ifndef PROJECT_CONFIG_H
#define PROJECT_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

/* ======================== 鍖呭惈 ======================== */
#include "hal_common.h"
#include "ti_msp_dl_config.h"
#include "filter_param_defaults.h"
#include "filter_tuning.h"


/* ================================================================
 * IMU filter backend feature switches
 *
 * The Keil target may override these macros in its preprocessor
 * definitions.  The normal target keeps only KF; the factory target
 * enables all backends for diagnostic comparison.  Enum values remain
 * stable even when a backend is compiled out.
 * ================================================================ */
#ifndef PRJ_FILTER_ENABLE_COMPLEMENTARY
#define PRJ_FILTER_ENABLE_COMPLEMENTARY (0U)
#endif
#ifndef PRJ_FILTER_ENABLE_LPF
#define PRJ_FILTER_ENABLE_LPF           (0U)
#endif
#ifndef PRJ_FILTER_ENABLE_EKF
#define PRJ_FILTER_ENABLE_EKF           (0U)
#endif
#ifndef PRJ_FILTER_ENABLE_LKF
#define PRJ_FILTER_ENABLE_LKF           (0U)
#endif
#ifndef PRJ_FILTER_ENABLE_MAHONY
#define PRJ_FILTER_ENABLE_MAHONY        (0U)
#endif
#ifndef PRJ_FILTER_ENABLE_MADGWICK
#define PRJ_FILTER_ENABLE_MADGWICK      (0U)
#endif
#ifndef PRJ_FILTER_ENABLE_KF
#define PRJ_FILTER_ENABLE_KF            (1U)
#endif

#if ((PRJ_FILTER_ENABLE_COMPLEMENTARY != 0U) && (PRJ_FILTER_ENABLE_COMPLEMENTARY != 1U)) || \
    ((PRJ_FILTER_ENABLE_LPF           != 0U) && (PRJ_FILTER_ENABLE_LPF           != 1U)) || \
    ((PRJ_FILTER_ENABLE_EKF           != 0U) && (PRJ_FILTER_ENABLE_EKF           != 1U)) || \
    ((PRJ_FILTER_ENABLE_LKF           != 0U) && (PRJ_FILTER_ENABLE_LKF           != 1U)) || \
    ((PRJ_FILTER_ENABLE_MAHONY        != 0U) && (PRJ_FILTER_ENABLE_MAHONY        != 1U)) || \
    ((PRJ_FILTER_ENABLE_MADGWICK      != 0U) && (PRJ_FILTER_ENABLE_MADGWICK      != 1U)) || \
    ((PRJ_FILTER_ENABLE_KF            != 0U) && (PRJ_FILTER_ENABLE_KF            != 1U))
#error "PRJ_FILTER_ENABLE_* macros must be 0 or 1"
#endif

#if (PRJ_FILTER_ENABLE_COMPLEMENTARY == 0U) && \
    (PRJ_FILTER_ENABLE_LPF == 0U) && \
    (PRJ_FILTER_ENABLE_EKF == 0U) && \
    (PRJ_FILTER_ENABLE_LKF == 0U) && \
    (PRJ_FILTER_ENABLE_MAHONY == 0U) && \
    (PRJ_FILTER_ENABLE_MADGWICK == 0U) && \
    (PRJ_FILTER_ENABLE_KF == 0U)
#error "At least one IMU filter backend must be enabled"
#endif

/* The current IMU application uses KF for its static instance. */
#if (PRJ_FILTER_ENABLE_KF == 0U)
#error "This application target requires PRJ_FILTER_ENABLE_KF=1"
#endif

/* ================================================================
 * Project identity canonical configuration
 *
 * PRJ_* is the canonical configuration used by this firmware project.
 * The standalone project_version.h compatibility header has been removed.
 * Only the protocol/board/motor PROJECT_* aliases remain below for legacy
 * modules and external tooling; new firmware code must use the PRJ_* names.
 * ================================================================ */
/** 鍥轰欢鐗堟湰涓荤増鍙枫€?*/
#define PRJ_VERSION_MAJOR        (0U)
/** 鍥轰欢鐗堟湰娆＄増鍙枫€?*/
#define PRJ_VERSION_MINOR        (1U)
/** 鍥轰欢鐗堟湰淇鍙枫€?*/
#define PRJ_VERSION_PATCH        (0U)
/** 鍥轰欢鐗堟湰瀛楃涓层€?*/
#define PRJ_VERSION_STRING       "0.1.0"
/** 鍥轰欢涓庝笂浣嶆満鏂囨湰鍗忚鐨勫吋瀹圭骇鍒€?*/
#define PRJ_PROTOCOL_VERSION     (1U)
/** 褰撳墠纭欢鏉垮崱鍚嶇О銆?*/
#define PRJ_BOARD_NAME           "MSPM0G3507"
/** 褰撳墠鐢垫満椹卞姩鍣ㄥ悕绉般€?*/
#define PRJ_MOTOR_DRIVER_NAME    "DRV8870"

#ifndef PROJECT_PROTOCOL_VERSION
#define PROJECT_PROTOCOL_VERSION     PRJ_PROTOCOL_VERSION
#endif
#ifndef PROJECT_BOARD_NAME
#define PROJECT_BOARD_NAME           PRJ_BOARD_NAME
#endif
#ifndef PROJECT_MOTOR_DRIVER_NAME
#define PROJECT_MOTOR_DRIVER_NAME    PRJ_MOTOR_DRIVER_NAME
#endif

/* ================================================================
 *  LED閰嶇疆
 *  SysConfig宸查厤缃? GPIOA.14, PINCM36
 * ================================================================ */

/** LED绔彛(GPIOA) */
#define PRJ_LED_PORT            HAL_GPIO_PORT_A
/** LED寮曡剼缂栧彿 */
#define PRJ_LED_PIN             LED_A27_PIN

/* ================================================================
 *  璋冭瘯UART閰嶇疆
 *  SysConfig宸查厤缃? UART0, TX=PA10/PINCM21, RX=PA11/PINCM22
 *  娉㈢壒鐜? 115200, 鏃堕挓: 40MHz
 * ================================================================ */

/** 璋冭瘯涓插彛HAL瀹炰緥 */
#define PRJ_UART_DEBUG_ID       HAL_UART_DEBUG

/* ================================================================
 * VOFA+ communication limits
 * Keep protocol safety limits in the project configuration so they
 * can be reviewed and rolled back independently of the VOFA module.
 * ================================================================ */
/** Maximum absolute PID parameter accepted by VOFA commands. */
#define PRJ_VOFA_PID_PARAM_MAX    (100.0f)
/** Maximum absolute target speed accepted by VOFA commands (RPM). */
#define PRJ_VOFA_TARGET_RPM_MAX   (800.0f)

/* ================================================================
 *  JDY-23 BLE UART閰嶇疆
 *  SysConfig: UART1, TX=PB6, RX=PB7, 9600-8-N-1, no flow control.
 *  JDY-23 protocol code is isolated from this hardware mapping.
 * ================================================================ */
#define PRJ_UART_BLE_ID         HAL_UART_BLE
#define PRJ_JDY23_UART_BAUD     (9600U)

/**
 * @brief 鏄惁鍚敤 UART1/JDY-23 BLE 鑿滃崟璋冭瘯鍔熻兘銆?
 * @details
 * 璁句负 1 鏃跺垵濮嬪寲 BLE 鏈嶅姟骞跺惎鐢?UART0 鑿滃崟涓殑 ble 鍛戒护锛?
 * 璁句负 0 鏃朵笉鍒濆鍖?BLE 鏈嶅姟銆佷笉缂栬瘧 BLE 鑿滃崟澶勭悊閫昏緫锛?
 * 涓斾笉褰卞搷 UART0銆佺數鏈恒€佺紪鐮佸櫒銆丄DC銆両MU 鍙婂叾浠栬彍鍗曞懡浠ゃ€?
 *
 * 璇ュ紑鍏冲彧鎺у埗鈥滆彍鍗曡皟璇?璇婃柇鍏ュ彛鈥濓紝涓嶄細鏀瑰彉 JDY-23 椹卞姩婧愭枃浠?
 * 鏄惁琚?Keil 宸ョ▼鏀跺綍锛屼粠鑰岄伩鍏嶄笉鍚?target 鐨勬枃浠剁粍鍙戠敓婕傜Щ銆?
 */
#ifndef PRJ_BLE_MENU_ENABLE
#define PRJ_BLE_MENU_ENABLE     (1U)
#endif

#if (PRJ_BLE_MENU_ENABLE != 0U) && (PRJ_BLE_MENU_ENABLE != 1U)
#error "PRJ_BLE_MENU_ENABLE must be 0 or 1"
#endif

/**
 * @brief 鏄惁鍏佽鎵嬫満閫氳繃 BLE 閫忔槑閫氶亾杩涘叆 UART0 鑿滃崟鍛戒护瑙ｆ瀽鍣ㄣ€?
 * @details
 * 璁句负 1 鍚庯紝JDY-23 宸插缓绔嬮€忔槑杩炴帴鏃讹紝鎵嬫満鍙戦€佺殑 ASCII 鍛戒护骞朵互
 * CR/LF 缁撴潫鍚庯紝浼氬鐢ㄧ幇鏈夎彍鍗曡В鏋愯矾寰勶紝渚嬪 `A 100`銆乣stop`銆?
 * `drvscope status` 绛夈€傝鍔熻兘鍙兘椹卞姩鐢垫満锛岄粯璁ゅ叧闂互閬垮厤璇姩浣溿€?
 *
 * 璇ュ紑鍏充緷璧?PRJ_BLE_MENU_ENABLE锛涘紑鍚湰瀹忔椂蹇呴』鍚屾椂寮€鍚?BLE 鑿滃崟鏈嶅姟銆?
 */
#ifndef PRJ_BLE_MENU_CONSOLE_ENABLE
#define PRJ_BLE_MENU_CONSOLE_ENABLE (0U)
#endif

#if (PRJ_BLE_MENU_CONSOLE_ENABLE != 0U) && \
    (PRJ_BLE_MENU_CONSOLE_ENABLE != 1U)
#error "PRJ_BLE_MENU_CONSOLE_ENABLE must be 0 or 1"
#endif

#if (PRJ_BLE_MENU_CONSOLE_ENABLE != 0U) && \
    (PRJ_BLE_MENU_ENABLE == 0U)
#error "BLE menu console requires PRJ_BLE_MENU_ENABLE=1"
#endif

/* ================================================================
 *  鐢垫満椹卞姩閫夋嫨涓庣粺涓€涓氬姟鍛戒护
 *
 *  鍒嗗眰鍏崇郴:
 *    Application -> bsp_motor(缁熶竴闂ㄩ潰) -> 鑺墖鍚庣 -> HAL
 *
 *  榛樿浣跨敤 DRV8870锛汿B6612 鏄鐢ㄧ‖浠跺悗绔€備笂灞傜粺涓€浣跨敤
 *  -PRJ_MOTOR_COMMAND_MAX ~ +PRJ_MOTOR_COMMAND_MAX锛屽垏鎹㈠悗绔笉鏀瑰彉
 *  PID銆佹ā鍨嬭鲸璇嗗拰閫氫俊鍗忚涓殑鍛戒护閲忕翰銆?
 * ================================================================ */
#define PRJ_MOTOR_DRIVER_DRV8870    (1U)
#define PRJ_MOTOR_DRIVER_TB6612     (2U)

#ifndef PRJ_MOTOR_DRIVER
#define PRJ_MOTOR_DRIVER            PRJ_MOTOR_DRIVER_DRV8870
#endif

#if (PRJ_MOTOR_DRIVER != PRJ_MOTOR_DRIVER_DRV8870) && \
    (PRJ_MOTOR_DRIVER != PRJ_MOTOR_DRIVER_TB6612)
#error "PRJ_MOTOR_DRIVER must select DRV8870 or TB6612"
#endif

/** 鍚庣鏃犲叧鐨勬湁绗﹀彿涓氬姟鍛戒护鏈€澶х粷瀵瑰€笺€?*/
#define PRJ_MOTOR_COMMAND_MAX       (500U)

/** 鐢垫満瀹夎鏂瑰悜锛涙鍛戒护蹇呴』缁熶竴瀵瑰簲杞︿綋鍓嶈繘鏂瑰悜銆?*/
#define PRJ_MOTOR_A_INSTALL_DIR_SIGN  (-1)
#define PRJ_MOTOR_B_INSTALL_DIR_SIGN  (-1)
#define PRJ_MOTOR_C_INSTALL_DIR_SIGN  (+1)
#define PRJ_MOTOR_D_INSTALL_DIR_SIGN  (+1)

/* ================================================================
 *  TB6612 澶囩敤鍚庣閰嶇疆
 *
 *  褰撳墠 Config/empty.syscfg 鏄?DRV8870 榛樿鏉跨骇閰嶇疆锛屼笉鍖呭惈浠ヤ笅8涓?
 *  鏂瑰悜GPIO銆傞€夋嫨 TB6612 鍓嶅繀椤诲湪鐙珛 SysConfig 鏉跨骇閰嶇疆涓仮澶?
 *  MOTOR_AIN1~MOTOR_DIN2锛屽苟閲嶆柊鐢熸垚 ti_msp_dl_config.c/h銆?
 * ================================================================ */
#define PRJ_TB6612_PWM_TIMER        HAL_TIMER_PWM_MOTOR
#define PRJ_TB6612_PWM_CLK_HZ       ((unsigned long)(PWM_MOTOR_INST_CLK_FREQ))
#define PRJ_TB6612_PWM_PERIOD       (1000U)
#define PRJ_TB6612_POWER_STARTUP_MS (1U)

/** 璁句负1鏃剁敱杞欢鎺у埗TB6612 STBY锛?琛ㄧずSTBY宸茬敱纭欢鍥哄畾涓烘湁鏁堛€?*/
#ifndef PRJ_TB6612_STANDBY_CONTROL_ENABLE
#define PRJ_TB6612_STANDBY_CONTROL_ENABLE (0U)
#endif

/** 浠呬緵缂栬瘧闂ㄩ潰鍒ゆ柇鏉跨骇寮曡剼涓嶴TBY閰嶇疆鏄惁瀹屾暣锛涚姝㈡墜宸ュ己鍒剁疆1銆?*/
#define PRJ_TB6612_BOARD_CONFIG_AVAILABLE (0U)

#if (PRJ_MOTOR_DRIVER == PRJ_MOTOR_DRIVER_TB6612)
#if !defined(MOTOR_AIN1_PIN) || !defined(MOTOR_AIN2_PIN) || \
    !defined(MOTOR_BIN1_PIN) || !defined(MOTOR_BIN2_PIN) || \
    !defined(MOTOR_CIN1_PIN) || !defined(MOTOR_CIN2_PIN) || \
    !defined(MOTOR_DIN1_PIN) || !defined(MOTOR_DIN2_PIN)
#error "TB6612 selected: restore MOTOR_AIN1..MOTOR_DIN2 in SysConfig and regenerate ti_msp_dl_config"
#else

/* 鍘嗗彶TB6612鏉跨骇鏂瑰悜GPIO锛涜嫢澶囩敤鏉挎敼鐗堬紝鍙慨鏀规湰鑺傘€?*/
#define PRJ_TB6612_A_PWM_CH      (0U) /* M1 / 鍙冲悗 */
#define PRJ_TB6612_A_IN1_PORT    HAL_GPIO_PORT_B
#define PRJ_TB6612_A_IN1_PIN     MOTOR_AIN1_PIN  /* PB24 */
#define PRJ_TB6612_A_IN2_PORT    HAL_GPIO_PORT_B
#define PRJ_TB6612_A_IN2_PIN     MOTOR_AIN2_PIN  /* PB20 */

#define PRJ_TB6612_B_PWM_CH      (1U) /* M2 / 鍙冲墠 */
#define PRJ_TB6612_B_IN1_PORT    HAL_GPIO_PORT_A
#define PRJ_TB6612_B_IN1_PIN     MOTOR_BIN1_PIN  /* PA24 */
#define PRJ_TB6612_B_IN2_PORT    HAL_GPIO_PORT_A
#define PRJ_TB6612_B_IN2_PIN     MOTOR_BIN2_PIN  /* PA31 */

#define PRJ_TB6612_C_PWM_CH      (2U) /* M3 / 宸﹀墠 */
#define PRJ_TB6612_C_IN1_PORT    HAL_GPIO_PORT_A
#define PRJ_TB6612_C_IN1_PIN     MOTOR_CIN1_PIN  /* PA3 */
#define PRJ_TB6612_C_IN2_PORT    HAL_GPIO_PORT_A
#define PRJ_TB6612_C_IN2_PIN     MOTOR_CIN2_PIN  /* PA7 */

#define PRJ_TB6612_D_PWM_CH      (3U) /* M4 / 宸﹀悗 */
#define PRJ_TB6612_D_IN1_PORT    HAL_GPIO_PORT_B
#define PRJ_TB6612_D_IN1_PIN     MOTOR_DIN1_PIN  /* PB6 */
#define PRJ_TB6612_D_IN2_PORT    HAL_GPIO_PORT_B
#define PRJ_TB6612_D_IN2_PIN     MOTOR_DIN2_PIN  /* PB7 */

#define PRJ_TB6612_CONFIGS { \
    { PRJ_TB6612_A_PWM_CH, PRJ_TB6612_A_IN1_PORT, PRJ_TB6612_A_IN1_PIN, \
      PRJ_TB6612_A_IN2_PORT, PRJ_TB6612_A_IN2_PIN, \
      PRJ_MOTOR_A_INSTALL_DIR_SIGN }, \
    { PRJ_TB6612_B_PWM_CH, PRJ_TB6612_B_IN1_PORT, PRJ_TB6612_B_IN1_PIN, \
      PRJ_TB6612_B_IN2_PORT, PRJ_TB6612_B_IN2_PIN, \
      PRJ_MOTOR_B_INSTALL_DIR_SIGN }, \
    { PRJ_TB6612_C_PWM_CH, PRJ_TB6612_C_IN1_PORT, PRJ_TB6612_C_IN1_PIN, \
      PRJ_TB6612_C_IN2_PORT, PRJ_TB6612_C_IN2_PIN, \
      PRJ_MOTOR_C_INSTALL_DIR_SIGN }, \
    { PRJ_TB6612_D_PWM_CH, PRJ_TB6612_D_IN1_PORT, PRJ_TB6612_D_IN1_PIN, \
      PRJ_TB6612_D_IN2_PORT, PRJ_TB6612_D_IN2_PIN, \
      PRJ_MOTOR_D_INSTALL_DIR_SIGN }, \
}

#if (PRJ_TB6612_STANDBY_CONTROL_ENABLE != 0U)
#if !defined(PRJ_TB6612_STANDBY_PORT) || \
    !defined(PRJ_TB6612_STANDBY_PIN) || \
    !defined(PRJ_TB6612_STANDBY_ACTIVE_LEVEL)
#error "TB6612 STBY control enabled: define port, pin and active level"
#else
#define PRJ_TB6612_POWER_CONFIG { \
    true, PRJ_TB6612_STANDBY_PORT, PRJ_TB6612_STANDBY_PIN, \
    PRJ_TB6612_STANDBY_ACTIVE_LEVEL \
}
#undef PRJ_TB6612_BOARD_CONFIG_AVAILABLE
#define PRJ_TB6612_BOARD_CONFIG_AVAILABLE (1U)
#endif
#else
#define PRJ_TB6612_POWER_CONFIG { false, HAL_GPIO_PORT_A, 0U, true }
#undef PRJ_TB6612_BOARD_CONFIG_AVAILABLE
#define PRJ_TB6612_BOARD_CONFIG_AVAILABLE (1U)
#endif
#endif /* direction GPIO macros available */
#endif /* selected TB6612 */

/* ================================================================
 *  缂栫爜鍣ㄦ崟鑾烽厤缃?
 *  SysConfig宸查厤缃? TIMG7/TIMA1/TIMG6/TIMG0, 缁勫悎鎹曡幏妯″紡(鑴夊+鍛ㄦ湡)
 * ================================================================ */

/**
 * 鐢垫満涓庣紪鐮佸櫒鏈烘鍙傛暟銆?
 *
 * PPR瀹氫箟涓虹紪鐮佸櫒A鐩稿湪鐢垫満杞存棆杞竴鍦堟椂鐨勫畬鏁磋剦鍐插懆鏈熸暟锛涘綋鍓嶆崟鑾烽€昏緫
 * 鍚屾椂缁熻A鐩镐笂鍗囨部鍜屼笅闄嶆部锛屽洜姝よВ鐮佸€嶉鍥哄畾涓?銆傚噺閫熸瘮鐢ㄥ垎鏁拌〃绀猴紝
 * 鍙噯纭厤缃?0:1銆?0:1鎴?98:11绛夐潪鏁存暟鏍囩О鍑忛€熸瘮銆?
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

/** 杈撳嚭杞存瘡杞鏁帮紝鐢ㄤ簬浣嶇疆鍜孯PM鎹㈢畻銆?*/
#define PRJ_MOTOR_OUTPUT_PULSES_PER_REV \
    ((PRJ_MOTOR_ENCODER_PPR * PRJ_MOTOR_GEAR_RATIO_NUMERATOR * \
      PRJ_ENCODER_DECODE_MULTIPLIER) / \
     PRJ_MOTOR_GEAR_RATIO_DENOMINATOR)

/** 鍏煎鐜版湁缂栫爜鍣˙SP璋冪敤銆?*/
#define PRJ_ENCODER_PULSES_PER_REV  PRJ_MOTOR_OUTPUT_PULSES_PER_REV

/** 宸﹀墠缂栫爜鍣℉AL瀹炰緥 */
#define PRJ_ENCODER_LF_TIMER    HAL_TIMER_CAPTURE_LF
/** 宸﹀悗缂栫爜鍣℉AL瀹炰緥 */
#define PRJ_ENCODER_LB_TIMER    HAL_TIMER_CAPTURE_LB
/** 鍙冲墠缂栫爜鍣℉AL瀹炰緥 */
#define PRJ_ENCODER_RF_TIMER    HAL_TIMER_CAPTURE_RF
/** 鍙冲悗缂栫爜鍣℉AL瀹炰緥 */
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
/** 缂栫爜鍣ˋ鐩哥鍙?寮曡剼(SysConfig鎹曡幏澶嶇敤杈撳叆) */
#define PRJ_ENCODER_LF_A_PORT        HAL_GPIO_PORT_A
#define PRJ_ENCODER_LB_A_PORT        HAL_GPIO_PORT_A
#define PRJ_ENCODER_RF_A_PORT        HAL_GPIO_PORT_A
#define PRJ_ENCODER_RB_A_PORT        HAL_GPIO_PORT_A
#define PRJ_ENCODER_LF_A_PIN         GPIO_M3_C0_PIN
#define PRJ_ENCODER_LB_A_PIN         GPIO_M4_C0_PIN
#define PRJ_ENCODER_RF_A_PIN         GPIO_M2_C0_PIN
#define PRJ_ENCODER_RB_A_PIN         GPIO_M1_C0_PIN
/** 缂栫爜鍣˙鐩哥鍙?SysConfig宸查厤缃? */
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
 * 缂栫爜鍣ㄥ畨瑁呮柟鍚戜慨姝ｏ細杞︿綋鍓嶈繘鏃跺洓璺紪鐮佸櫒RPM搴旂粺涓€涓烘銆?
 * 鑻ュ彧鏇存崲鏌愪竴璺數鏈?缂栫爜鍣ㄥ畨瑁呮柟鍚戯紝鍙慨鏀瑰搴斿畯锛屼笉鏀笽SR鍒ゅ悜閫昏緫銆?
 */
#define PRJ_ENCODER_LF_DIR_SIGN (-1)
#define PRJ_ENCODER_LB_DIR_SIGN (-1)
#define PRJ_ENCODER_RF_DIR_SIGN (+1)
#define PRJ_ENCODER_RB_DIR_SIGN (+1)

/**
 * 鐢垫満杈撳嚭閫氶亾涓庣紪鐮佸櫒鍙嶉鐨勭墿鐞嗗搴斿叧绯汇€?
 * A/M1=鍙冲悗(RB)锛孊/M2=鍙冲墠(RF)锛孋/M3=宸﹀墠(LF)锛孌/M4=宸﹀悗(LB)銆?
 * task_control.c鎹灏嗙紪鐮佸櫒鍙嶉鍜屼綅缃帶鍒剁洰鏍囩殑杞﹁疆椤哄簭
 * (LF/LB/RF/RB)缁熶竴閲嶆帓涓虹數鏈洪『搴?A/B/C/D)銆?
 */
#define PRJ_MOTOR_A_ENCODER_ID  BSP_ENCODER_RB
#define PRJ_MOTOR_B_ENCODER_ID  BSP_ENCODER_RF
#define PRJ_MOTOR_C_ENCODER_ID  BSP_ENCODER_LF
#define PRJ_MOTOR_D_ENCODER_ID  BSP_ENCODER_LB
#define PRJ_MOTOR_ENCODER_MAP { \
    PRJ_MOTOR_A_ENCODER_ID, PRJ_MOTOR_B_ENCODER_ID, \
    PRJ_MOTOR_C_ENCODER_ID, PRJ_MOTOR_D_ENCODER_ID \
}

/** 缂栫爜鍣ㄩ厤缃〃(椤哄簭闇€涓嶣SP_ENCODER_x涓€鑷? */
#define PRJ_ENCODER_CONFIGS { \
		{ PRJ_ENCODER_LF_TIMER, PRJ_ENCODER_LF_A_PORT, PRJ_ENCODER_LF_A_PIN, \
			PRJ_ENCODER_LF_B_PORT, PRJ_ENCODER_LF_B_PIN, PRJ_ENCODER_LF_DIR_SIGN }, \
		{ PRJ_ENCODER_LB_TIMER, PRJ_ENCODER_LB_A_PORT, PRJ_ENCODER_LB_A_PIN, \
			PRJ_ENCODER_LB_B_PORT, PRJ_ENCODER_LB_B_PIN, PRJ_ENCODER_LB_DIR_SIGN }, \
		{ PRJ_ENCODER_RF_TIMER, PRJ_ENCODER_RF_A_PORT, PRJ_ENCODER_RF_A_PIN, \
			PRJ_ENCODER_RF_B_PORT, PRJ_ENCODER_RF_B_PIN, PRJ_ENCODER_RF_DIR_SIGN }, \
		{ PRJ_ENCODER_RB_TIMER, PRJ_ENCODER_RB_A_PORT, PRJ_ENCODER_RB_A_PIN, \
			PRJ_ENCODER_RB_B_PORT, PRJ_ENCODER_RB_B_PIN, PRJ_ENCODER_RB_DIR_SIGN }, \
}

/* ================================================================
 *  ADC閰嶇疆
 *  SysConfig宸查厤缃? ADC0, 12浣嶅崟娆￠噰鏍? 閫氶亾0(PA27), VDDA鍙傝€?.3V
 * ================================================================ */

/** 鐢靛帇ADC HAL瀹炰緥 */
#define PRJ_ADC_VOLTAGE_ID      HAL_ADC_VOLTAGE
/** ADC鍙傝€冪數鍘?mV) */
#define PRJ_ADC_VREF_MV         (3300U)
/** ADC鍒嗚鲸鐜?12浣? */
#define PRJ_ADC_RESOLUTION      (4096U)

/** Current sense shunt resistance and amplifier gain. */
#define PRJ_ADC_CURRENT_SHUNT_OHM     (0.15f)
#define PRJ_ADC_CURRENT_AMPLIFY       (10.0f)
#define PRJ_ADC_CURRENT_MA_PER_RAW \
    ((float)(PRJ_ADC_VREF_MV) / (float)(PRJ_ADC_RESOLUTION) / \
     PRJ_ADC_CURRENT_SHUNT_OHM / PRJ_ADC_CURRENT_AMPLIFY)

/** Overcurrent threshold and consecutive 5 ms control ticks. */
#define PRJ_ADC_CURRENT_OVERLOAD_MA   (1500U)
#define PRJ_ADC_CURRENT_OVERLOAD_TICKS (10U)

/* ================================================================
 *  纭欢SPI閰嶇疆 (LSM6DSR)
 *  SPI1: SCK=PB9, MOSI=PA18, MISO=PA16, CS=PA25(鐙珛GPIO)
 *  娉ㄦ剰: SPI 寮曡剼鐢?SysConfig 閰嶇疆锛宻pi_bridge.c 浣跨敤 ti_msp_dl_config.h 瀹氫箟
 * ================================================================ */

/* ================================================================
 *  杞欢I2C閰嶇疆 (宸插純鐢紝鏇挎崲涓虹‖浠禨PI)
 *  閰嶇疆宸茬Щ闄わ紝淇濈暀娉ㄩ噴渚涘巻鍙插弬鑰?
 * ================================================================ */

/* MPU6050 閰嶇疆宸茬Щ闄わ紝鏇挎崲涓?LSM6DSR */

/* ================================================================
 *  LSM6DSR鍏酱浼犳劅鍣ㄩ厤缃?
 *  閫氳繃纭欢SPI鎺ュ彛閫氫俊(SPI1: SCK=PB9, MOSI=PA18, MISO=PA16, CS=PA25)
 *  娉ㄦ剰: 闇€瑕佸湪SysConfig涓厤缃甋PI1澶栬
 *  璇存槑: 閲忕▼/閲囨牱鐜?婊ゆ尝鍣ㄧ被鍨嬬敱 bsp_lsm6dsr.c 鐩存帴浣跨敤
 *        lsm6dsr.h 涓殑鏋氫妇甯搁噺, 姝ゅ涓嶉噸澶嶅畾涔?
 * ================================================================ */

/* ================================================================
 *  MATHACL 纭欢鏁板鍔犻€熼厤缃?
 * ================================================================ */
/** 鏄惁鍚敤 MATHACL 纭欢鍔犻€?1=鍚敤锛?=杞欢鍥為€€)銆?*/
#define PRJ_MATHACL_ENABLE                  (1U)
/** 鏄惁浣跨敤 MATHACL 纭欢 ATAN2 璺緞銆?*/
#define PRJ_MATHACL_ATAN2_HW                (1U)
/** 鏄惁浣跨敤 MATHACL 纭欢 SINCOS 璺緞銆?*/
#define PRJ_MATHACL_SINCOS_HW               (1U)
/** 鏄惁涓?MATHACL 瀵勫瓨鍣ㄨ闂惎鐢ㄧ嚎绋嬪畨鍏ㄤ复鐣屽尯銆?*/
#define PRJ_MATHACL_THREAD_SAFE             (0U)
/** 鏄惁浣跨敤 MATHACL 纭欢 SQRT 璺緞锛涢粯璁ゅ叧闂互淇濈暀宸查獙璇佺殑杞欢璺緞銆?*/
#define PRJ_MATHACL_SQRT_HW                (0U)
/** 鏄惁涓?KF 缂栬瘧 MATHACL 瀹氱偣鍔犻€熻矾寰勶紱榛樿鍏抽棴浠ヤ繚鎸佺幇鏈夎繍琛岃涓恒€?*/
#define PRJ_MATHACL_KF_HW                  (0U)
/** 鏄惁涓?EKF 缂栬瘧 MATHACL 瀹氱偣闄ゆ硶鍔犻€熻矾寰勶紱榛樿鍏抽棴浠ヤ繚鎸佺幇鏈夎繍琛岃涓恒€?*/
#define PRJ_MATHACL_EKF_HW                 (0U)
/** 鏄惁缂栬瘧 MATHACL 鐭╅樀瀹為獙瀹炵幇锛涢粯璁ゅ叧闂互閬垮厤鐢熶骇鍥轰欢寮曞叆棰濆浠ｇ爜銆?*/
#define PRJ_MATHACL_MATRIX_ENABLE          (0U)

/* ================================================================
 *  IMU浠诲姟閰嶇疆
 * ================================================================ */

/** IMU閲囬泦浠诲姟鍛ㄦ湡(ms), 100Hz */
#define PRJ_IMU_TASK_PERIOD_MS       (10U)

/** IMU 鏍″噯閲囨牱甯ф暟銆?*/
#define PRJ_IMU_CALIB_SAMPLES                 (300U)
/** IMU 閰嶇疆瀹屾垚鍚庣殑绋冲畾绛夊緟鏃堕棿(ms)銆?*/
#define PRJ_IMU_CALIB_SETTLE_MS               (50U)
/** 鍔犻€熷害妯″钩鏂瑰弬鑰冨€?g^2)銆?*/
#define PRJ_IMU_CALIB_ACC_MAG_REF             (1.0f)
/** 鍔犻€熷害妯″钩鏂归潤姝㈠垽瀹氬宸?g^2)銆?*/
#define PRJ_IMU_CALIB_ACC_MAG_TOL             (0.065f)
/** 鏍″噯鐩搁偦甯у姞閫熷害宸垎闃堝€?g)銆?*/
#define PRJ_IMU_CALIB_ACC_DELTA_MAX           (0.08f)
/** IMU 鏍″噯閲囨牱闂撮殧(ms)銆?*/
#define PRJ_IMU_CALIB_SAMPLE_DELAY_MS         (9U)

/** 鍔犻€熷害鏂瑰樊婊戝姩绐楀彛闀垮害(甯?銆?*/
#define PRJ_IMU_ACC_VAR_WINDOW                (10U)
/** 棰勭暀 IMU 璇诲彇鑰楁椂琛ュ伩(us)銆?*/
#define PRJ_IMU_DT_READ_COMPENSATION_US       (200U)
/** 鍔犻€熷害闈欐鏂瑰樊闃堝€?g^2 鎬诲拰)銆?*/
#define PRJ_IMU_ACC_VAR_THRESHOLD             (0.0008f)
/** 杩愬姩鐘舵€佷簰琛ユ护娉㈢郴鏁般€?*/
#define PRJ_IMU_ALPHA_MOVING                  (0.99f)
/** 闈欐鐘舵€佷簰琛ユ护娉㈢郴鏁般€?*/
#define PRJ_IMU_ALPHA_STATIONARY              (0.30f)
/** 浜掕ˉ婊ゆ尝绯绘暟鍗曞抚鏈€澶у彉鍖栭噺銆?*/
#define PRJ_IMU_ALPHA_SMOOTH_STEP             (0.15f)

/** X/Y 杞撮潤姝㈤檧铻哄亸缃窡韪€熺巼銆?*/
#define PRJ_IMU_BIAS_STATIONARY_RATE          (0.1f)
/** Z 杞撮潤姝㈤檧铻哄亸缃窡韪€熺巼銆?*/
#define PRJ_IMU_BIAS_STATIONARY_RATE_Z        (0.1f)
/** 闄€铻鸿繍鍔ㄥ垽瀹氶槇鍊?dps)銆?*/
#define PRJ_IMU_GYRO_MOTION_THRESHOLD         (5.0f)
/** 鏄惁鍚敤鍩轰簬浠诲姟鍛ㄦ湡鐨?dt 寮傚父鏀剁揣闂ㄩ檺銆?*/
#define PRJ_IMU_ODR_ALIGN                     (0U)
/** dt 寮傚父涓嬮檺(s)銆?*/
#define PRJ_IMU_DT_ANOMALY_MIN_S              (0.003)
/** dt 寮傚父涓婇檺(s)銆?*/
#define PRJ_IMU_DT_ANOMALY_MAX_S              (0.030)
/** IMU 寮傚父 dt 鎴栨椂闂存埑鍥炵粫鏃朵娇鐢ㄧ殑榛樿鍛ㄦ湡(s)銆?*/
#define PRJ_IMU_DT_DEFAULT_S                  (0.01)


/**
 * @brief Background IMU CSV telemetry on the shared UART0.
 * @note  Disabled by default: IMU sampling and filtering continue, but no
 *        periodic DMA frame is injected into CLI/diagnostic output.
 *        Re-enable only after the UART TX arbitration work package is verified.
 */
#define PRJ_IMU_UART_TELEMETRY_ENABLE (0U)
#define PRJ_IMU_UART_TELEMETRY_PERIOD_MS  (200U)
#define PRJ_IMU_UART_TELEMETRY_BUF_SIZE   (512U)
#define PRJ_IMU_KF_FILTER_BUF_SIZE        (2048U)
/** 鍏佽閫氳繃鑿滃崟鍛戒护鏌ヨ IMU 骞跺紑鍚彈鎺?UART0 CSV 閬ユ祴銆?*/
#if (PRJ_IMU_UART_TELEMETRY_PERIOD_MS < PRJ_IMU_TASK_PERIOD_MS)
#error "PRJ_IMU_UART_TELEMETRY_PERIOD_MS must be >= PRJ_IMU_TASK_PERIOD_MS"
#endif
#if (PRJ_IMU_UART_TELEMETRY_BUF_SIZE < 128U)
#error "PRJ_IMU_UART_TELEMETRY_BUF_SIZE is too small"
#endif
#if (PRJ_IMU_KF_FILTER_BUF_SIZE < 256U)
#error "PRJ_IMU_KF_FILTER_BUF_SIZE is too small"
#endif

#define PRJ_IMU_CONSOLE_ENABLE              (1U)
/** IMU 杩炵画杈撳嚭榛樿鍛ㄦ湡(ms)锛涗笂鐢甸粯璁や粛涓哄叧闂姸鎬併€?*/
#define PRJ_IMU_CONSOLE_DEFAULT_PERIOD_MS   (100U)
/** IMU 杩炵画杈撳嚭鏈€灏忓懆鏈?ms)锛岄伩鍏嶅畬鏁磋瘖鏂抚鍗犳弧 UART 甯﹀銆?*/
#define PRJ_IMU_CONSOLE_MIN_PERIOD_MS       (20U)
/** IMU 杩炵画杈撳嚭鏈€澶у懆鏈?ms)銆?*/
#define PRJ_IMU_CONSOLE_MAX_PERIOD_MS       (1000U)

#if (PRJ_IMU_CONSOLE_ENABLE > 1U)
#error "PRJ_IMU_CONSOLE_ENABLE must be 0 or 1"
#endif
#if (PRJ_IMU_CONSOLE_MIN_PERIOD_MS == 0U) || \
    (PRJ_IMU_CONSOLE_MIN_PERIOD_MS > PRJ_IMU_CONSOLE_MAX_PERIOD_MS)
#error "Invalid IMU console period range"
#endif
#if (PRJ_IMU_CONSOLE_DEFAULT_PERIOD_MS < PRJ_IMU_CONSOLE_MIN_PERIOD_MS) || \
    (PRJ_IMU_CONSOLE_DEFAULT_PERIOD_MS > PRJ_IMU_CONSOLE_MAX_PERIOD_MS)
#error "Invalid IMU console default period"
#endif
/* ================================================================
 *  搴旂敤浠诲姟涓庢帶鍒堕粯璁ゅ弬鏁?
 *  缁熶竴鐢遍」鐩厤缃叆鍙ｇ鐞嗭紝app_main.h 浠呮彁渚?APP_* 鍏煎鍒悕銆?
 * ================================================================ */

/** 鎺у埗浠诲姟浼樺厛绾э紝鏁板€艰秺澶т紭鍏堢骇瓒婇珮銆?*/
#define PRJ_TASK_PRIORITY_CONTROL       (5U)
/** IMU浠诲姟浼樺厛绾с€?*/
#define PRJ_TASK_PRIORITY_IMU           (4U)
/** 鑿滃崟浠诲姟浼樺厛绾с€?*/
#define PRJ_TASK_PRIORITY_MENU          (2U)

/** 鎺у埗浠诲姟鏍堝ぇ灏忥紝鍗曚綅涓?FreeRTOS 鏍堝瓧銆?*/
#define PRJ_TASK_STACK_CONTROL          (256U)
/** IMU浠诲姟鏍堝ぇ灏忥紝鍗曚綅涓?FreeRTOS 鏍堝瓧銆?*/
#define PRJ_TASK_STACK_IMU              (1280U)
/** 鑿滃崟浠诲姟鏍堝ぇ灏忥紝鍗曚綅涓?FreeRTOS 鏍堝瓧銆?*/
#define PRJ_TASK_STACK_MENU             (384U)

/** 鎺у埗浠诲姟鍛ㄦ湡(ms)銆?*/
#define PRJ_CONTROL_PERIOD_MS           (5U)
/** 鑿滃崟浠诲姟杞鍛ㄦ湡(ms)銆?*/
#define PRJ_MENU_POLL_PERIOD_MS         (100U)
/** 杩愯妯″紡涓嬬殑 RPM 杈撳嚭鍛ㄦ湡(ms)銆?*/
#define PRJ_RPM_OUTPUT_PERIOD_MS        (30U)

/** 鑿滃崟鍛戒护琛岃緭鍏ョ紦鍐插尯澶у皬(瀛楄妭)銆?*/
#define PRJ_MENU_LINE_BUF_SIZE          (64U)

/** 閫熷害鐜粯璁ゆ瘮渚嬪鐩娿€?*/
#define PRJ_PID_DEFAULT_KP              (0.8f)
/** 閫熷害鐜粯璁ょН鍒嗗鐩娿€?*/
#define PRJ_PID_DEFAULT_KI              (0.3f)
/** 閫熷害鐜粯璁ゅ井鍒嗗鐩娿€?*/
#define PRJ_PID_DEFAULT_KD              (0.0f)
/** 鍓嶉妯″紡 PID 榛樿姣斾緥澧炵泭銆?*/
#define PRJ_FF_PID_DEFAULT_KP           (0.5f)
/** 鍓嶉妯″紡 PID 榛樿绉垎澧炵泭銆?*/
#define PRJ_FF_PID_DEFAULT_KI           (0.1f)
/** 鍓嶉妯″紡 PID 榛樿寰垎澧炵泭銆?*/
#define PRJ_FF_PID_DEFAULT_KD           (0.0f)

/* ================================================================
 *  浜掕ˉ婊ゆ尝鍣ㄩ厤缃?
 * ================================================================ */

/** 浜掕ˉ婊ゆ尝绯绘暟(0~1, 0=鍏ㄤ俊浠籌MU, 1=鍏ㄤ俊浠荤紪鐮佸櫒) */
#define PRJ_CF_ALPHA                 FILTER_COMP_ALPHA_DEFAULT
/** KF ?????????? */
#define PRJ_KF_Q_ANGLE_DEFAULT        FILTER_KF_Q_ANGLE_DEFAULT
/** KF ?????????? */
#define PRJ_KF_Q_BIAS_DEFAULT         FILTER_KF_Q_BIAS_DEFAULT
/** KF ??????????? */
#define PRJ_KF_R_MEASURE_DEFAULT      FILTER_KF_R_MEASURE_DEFAULT
/** KF ZUPT ????????1e6 ????? */
#define PRJ_KF_R_ZUPT_DEFAULT         FILTER_KF_R_ZUPT_DEFAULT
/** 杞儙澶栧緞(mm)锛屽簲浠ヨ礋杞界姸鎬佷笅鐨勬湁鏁堟粴鍔ㄧ洿寰勬爣瀹氥€?*/
#define PRJ_MOTOR_WHEEL_DIAMETER_MM  (60.0f)
/** 杞瓙鏈夋晥婊氬姩鍗婂緞(m)锛岀敱杞緞缁熶竴娲剧敓锛岄伩鍏嶉噸澶嶉厤缃€?*/
#define PRJ_CF_WHEEL_RADIUS_M \
    (PRJ_MOTOR_WHEEL_DIAMETER_MM * 0.001f * 0.5f)
/** 杞窛(m, 宸﹀彸杞帴鍦扮偣涓績璺濈)锛屽簲鎸夊疄杞︽爣瀹氥€?*/
#define PRJ_CF_WHEEL_BASE_M          (0.15f)

/* ================================================================
 *  浣嶇疆-閫熷害涓茬骇鎺у埗閰嶇疆
 *  鐢ㄤ簬 app_position_control.c, 鍦ㄩ€熷害鐜?5ms)涔嬩笂澧炲姞
 *  浣嶇疆鐜?瑙掑害鐜?20ms), 瀹炵幇绮惧噯瀹氫綅鍜岃浆鍚戞帶鍒?
 * ================================================================ */

/** 浣嶇疆鐜疨ID鍙傛暟(浣嶇疆寮廝ID, 杈撳嚭RPM淇) */
#define PRJ_POS_PID_KP              (0.5f)
#define PRJ_POS_PID_KI              (0.0f)
#define PRJ_POS_PID_KD              (0.0f)

/** 瑙掑害鐜疨ID鍙傛暟(浣嶇疆寮廝ID, 杈撳嚭宸€烺PM) */
#define PRJ_YAW_PID_KP              (2.0f)
#define PRJ_YAW_PID_KI              (0.0f)
#define PRJ_YAW_PID_KD              (0.0f)

/** 瑙勫垝鍣ㄥ姞閫熷害(RPM/s, 鎺у埗鍔犲噺閫熷钩婊戝害) */
#define PRJ_PLANNER_ACCEL           (500.0f)

/** 鏈€澶х洰鏍嘡PM(閫熷害闄愬箙, 闃叉杩囬€? */
#define PRJ_PLANNER_MAX_RPM         (300.0f)

/** 鍒颁綅鍒ゅ畾闃堝€?浣嶇疆:鑴夊啿, 瑙掑害:搴? */
#define PRJ_REACHED_THRESHOLD_POS   (5.0f)
#define PRJ_REACHED_THRESHOLD_YAW   (0.5f)

/** 鍒颁綅鎸佺画鍛ㄦ湡鏁?20ms脳10=200ms) */
#define PRJ_REACHED_COUNT           (10U)

/** 妯″紡鍒囨崲杩囨浮鏃堕暱(ms, 1绉掓笎鍙? */
#define PRJ_MODE_TRANSITION_MS      (1000U)

/* ================================================================
 *  绯荤粺鍙傛暟
 * ================================================================ */

/** 绯荤粺瀹氭椂鍣℉AL瀹炰緥 */
#define PRJ_SYS_TICK_TIMER      HAL_TIMER_SYS_TICK

/* ================================================================
 *  鏁板甯搁噺
 * ================================================================ */

/** 鍦嗗懆鐜?float绮惧害, 渚涘簲鐢ㄥ眰閬垮厤榄旀暟 3.14159265f) */
#define PRJ_PI_F                (3.14159265358979f)
/** 鍦嗗懆鐜?double绮惧害, 渚涙护娉㈠櫒绛夐渶瑕乨ouble绮惧害鐨勬ā鍧椾娇鐢? */
#define PRJ_PI_D                (3.14159265358979323846)
/** 2蟺(float绮惧害) */
#define PRJ_TWO_PI_F            (6.28318530717958647692f)
/** 蟺/2(float绮惧害) */
#define PRJ_PI_2_F              (1.57079632679489661923f)
/** 寮у害鈫掕搴﹁浆鎹㈢郴鏁?float): 180/蟺 */
#define PRJ_RAD2DEG_F           (57.29577951308232087685f)
/** 瑙掑害鈫掑姬搴﹁浆鎹㈢郴鏁?float): 蟺/180 */
#define PRJ_DEG2RAD_F           (0.01745329251994329577f)

/* ================================================================
 *  鏃堕棿杞崲甯搁噺(鏃犵鍙锋暣鍨? 鐢ㄤ簬閬垮厤榄旀暟 1000/60000 绛?
 *  浣跨敤娴偣涓婁笅鏂囨椂闇€鏄惧紡 (float) cast
 * ================================================================ */

/** 姣忕姣鏁?*/
#define PRJ_MS_PER_S            (1000U)
/** 姣忓垎閽熸绉掓暟(60s 脳 1000ms) */
#define PRJ_MS_PER_MIN          (60000U)
/** 姣忔绉掑井绉掓暟 */
#define PRJ_US_PER_MS           (1000U)

/** 鏍囧噯閲嶅姏鍔犻€熷害 m/s虏 (float绮惧害) */
#define PRJ_GRAVITY_MS2         (9.80665f)

/** 16浣嶆棤绗﹀彿鏁存暟妯℃暟 (2^16), 鐢ㄤ簬瀹氭椂鍣ㄨ鏁板櫒鍥炵粫淇 */
#define PRJ_UINT16_MOD          (65536U)

/* ================================================================
 *  娲剧敓棰戠巼瀹忥紙sysconfig 鏈毚闇诧紝闇€寮€鍙戣€呮墜鍔ㄧ淮鎶や笌 sysconfig 涓€鑷存€э級
 *
 *  浠ヤ笅棰戠巼鍊煎湪 ti_msp_dl_config.c 涓互娉ㄩ噴褰㈠紡瀛樺湪锛屼絾 sysconfig
 *  鏈皢鍏朵綔涓?#define 鏆撮湶鍦?ti_msp_dl_config.h 涓€傛澶勯泦涓畾涔夛紝
 *  渚涘簲鐢ㄥ眰寮曠敤锛岄伩鍏嶇‖缂栫爜榄旀暟銆?
 *
 *  鈿狅笍 缁存姢瑙勫垯锛氫慨鏀?sysconfig 涓搴斿畾鏃跺櫒鐨勫垎棰?棰勫垎棰戝悗锛?
 *     蹇呴』鍚屾鏇存柊浠ヤ笅瀹忕殑鍊硷紝骞堕噸鏂伴獙璇佷緷璧栨瀹忕殑鎵€鏈変唬鐮併€?
 *
 *  璁＄畻渚濇嵁锛堟潵鑷?ti_msp_dl_config.c 娉ㄩ噴锛夛細
 *    BUSCLK = ULPCLK = CPUCLK/2 = 40MHz
 *    CAPTURE timer: BUSCLK/4/(199+1) = 100kHz
 *    TIMER_0 (TIMG8): BUSCLK/8/(9+1) = 500kHz
 * ================================================================ */

/**
 * 缂栫爜鍣ㄦ崟鑾峰畾鏃跺櫒瀹為檯棰戠巼(Hz)
 * 鏉ユ簮: ti_msp_dl_config.c 涓?CAPTURE_* 鐨?divideRatio=DIVIDE_4, prescale=199
 * 璁＄畻: BUSCLK(40MHz) / 4 / (199+1) = 100000 Hz
 * 鐢ㄩ€? bsp_encoder.c / app_debug.c 鐨?M/T 娉?RPM 璁＄畻
 * 渚濊禆: 6000000LL = 60 脳 PRJ_CAPTURE_TIMER_FREQ_HZ
 *
 * 鈿狅笍 sysconfig 淇敼 CAPTURE_* 鐨勫垎棰?prescale 鍚庡繀椤绘洿鏂版鍊?
 */
#define PRJ_CAPTURE_TIMER_FREQ_HZ   (100000UL)

/**
 * 绯荤粺寰璁℃椂鍣ㄥ疄闄呴鐜?Hz)
 * 鏉ユ簮: ti_msp_dl_config.c 涓?TIMER_0 (TIMG8) 鐨?divideRatio=DIVIDE_8, prescale=9
 * 璁＄畻: BUSCLK(40MHz) / 8 / (9+1) = 500000 Hz (2us/tick)
 * 鐢ㄩ€? platform_mspm0.c get_tick_us() 鐨勫井绉掓崲绠?
 * 渚濊禆: tick_to_us = count / (PRJ_SYS_TICK_TIMER_FREQ_HZ / 1000000UL)
 *        鍗?count * 2U (褰撳墠纭紪鐮?
 *
 * 鈿狅笍 sysconfig 淇敼 TIMER_0 鐨勫垎棰?prescale 鍚庡繀椤绘洿鏂版鍊?
 */
#define PRJ_SYS_TICK_TIMER_FREQ_HZ  (500000UL)

/**
 * 寰璁℃椂鍣? 1 涓?tick 瀵瑰簲鐨勫井绉掓暟(脳1000 鎵╁ぇ绮惧害閬垮厤娴偣)
 * 璁＄畻: 1000000 / PRJ_SYS_TICK_TIMER_FREQ_HZ = 2 (鍗?2us/tick)
 * 鐢ㄩ€? platform_mspm0.c:85 鏇挎崲纭紪鐮?* 2U
 *
 * 鈿狅笍 涓?PRJ_SYS_TICK_TIMER_FREQ_HZ 鑱斿姩锛屼慨鏀逛竴澶勯渶鍚屾妫€鏌?
 */
#define PRJ_SYS_TICK_US_PER_TICK_X1000  \
    (1000000UL * 1000UL / PRJ_SYS_TICK_TIMER_FREQ_HZ)

/**
 * 缂栫爜鍣?M/T 娉?RPM 璁＄畻甯告暟
 * 璁＄畻: 60 脳 PRJ_CAPTURE_TIMER_FREQ_HZ = 60 脳 100000 = 6000000
 * 鐢ㄩ€? bsp_encoder.c / app_debug.c 涓?RPM = (delta 脳 60 脳 timer_freq) / (pulses 脳 period)
 *
 * 鈿狅笍 涓?PRJ_CAPTURE_TIMER_FREQ_HZ 鑱斿姩
 */
#define PRJ_ENCODER_RPM_CALC_CONST  \
    (60LL * (int64_t)PRJ_CAPTURE_TIMER_FREQ_HZ)

/* ================================================================
 *  DRV8870 鐢垫満椹卞姩閰嶇疆 (閿佺浉椹卞姩 Locked Anti-Phase)
 *  涓?TB6612 椹卞姩骞跺瓨, 鍙€氳繃缂栬瘧瀹忓垏鎹娇鐢?
 *  SysConfig宸查厤缃? TIMA0, 4閫氶亾PWM, 20MHz鏃堕挓, 20kHz鍛ㄦ湡
 *  閫氶亾: C0=PA8, C1=PA9, C2=PB17, C3=PB2
 *  椹卞姩鑺墖: DRV8870DDAR (閿佺浉椹卞姩)
 *  纭欢鎷撴墤: MCU PWM 鈫?DRV8870 IN1(鐩磋繛) + S8050鍙嶇浉鍣?鈫?IN2
 *  姝ｈ浆鏈夋晥鍖? PWM鍗犵┖姣?> PRJ_DRV8870_DEADBAND_HIGH_PERCENT
 *  鍙嶈浆鏈夋晥鍖? PWM鍗犵┖姣?< PRJ_DRV8870_DEADBAND_LOW_PERCENT
 *  鍋滄姝诲尯: 40%~55%锛岄浂鍛戒护杈撳嚭50%涓€у崰绌烘瘮
 *  鏃犻渶鏂瑰悜寮曡剼(IN1/IN2鐢辩‖浠跺弽鐩稿櫒鑷姩鐢熸垚浜掕ˉ淇″彿)
 * ================================================================ */

/** DRV8870 PWM瀹氭椂鍣℉AL瀹炰緥(澶嶇敤TIMA0) */
#define PRJ_DRV8870_PWM_TIMER     HAL_TIMER_PWM_MOTOR
/** PWM鏃堕挓棰戠巼(Hz) - 寮曠敤 sysconfig 鏆撮湶鐨勫畯 */
#define PRJ_DRV8870_PWM_CLK_HZ    ((unsigned long)(PWM_MOTOR_INST_CLK_FREQ))
/** PWM鍛ㄦ湡鍊?20kHz = 1000涓?0MHz鏃堕挓鍛ㄦ湡) */
#define PRJ_DRV8870_PWM_PERIOD    (1000U)
/** 鏈夌鍙烽€熷害鍛戒护鏈€澶х粷瀵瑰€硷紱涓氬姟灞傘€丳ID鍜屾ā鍨嬭鲸璇嗗潎搴斾繚鎸佷竴鑷淬€?*/
#define PRJ_DRV8870_SPEED_COMMAND_MAX PRJ_MOTOR_COMMAND_MAX

/**
 * 瀹炴祴鏈烘姝诲尯杈圭晫锛堢粷瀵筆WM鍗犵┖姣旂櫨鍒嗘暟锛夈€?
 * 0%~39.9%涓哄弽杞湁鏁堝尯锛?0%~55%涓哄仠姝㈡鍖猴紝55.1%~100%涓烘杞湁鏁堝尯銆?
 * bsp_drv8870_set_speed()浼氭妸闈為浂鏈夌鍙峰懡浠ゅ垎娈垫槧灏勫埌姝诲尯涔嬪锛?
 * 宸ュ巶绀烘尝鍣ㄦ帴鍙ｄ粛鏄師濮嬬粷瀵筩ompare锛屽彲鐩存帴杩涘叆姝诲尯鐢ㄤ簬娴嬮噺銆?
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
 * 闆剁偣鍋忕Щ琛ュ伩 (S8050 鍙嶇浉鍣ㄥ紑鍏充笉瀵圭О + DRV8870 浼犳挱寤惰繜)
 * 鏂囨。鍙傝€? DRV8870鎶€鏈枃妗?搂3.2.2, 鍏稿瀷鍋忕Щ 2~5 姝?
 * 闇€瀹炴祴鏍囧畾: 鎵惧埌浣跨數鏈烘伆濂介潤姝㈢殑 duty 鍊? 鍑忓幓 PWM_PERIOD/2
 */
#define PRJ_DRV8870_ZERO_DUTY_OFFSET  (0)

/*
 * DRV8870 宸ュ巶纭欢鑴夊啿娴嬭瘯闂搁棬銆傞粯璁ゅ叧闂紝闃叉鑿滃崟/璋冭瘯鍛戒护鍦?
 * 闈炲彈鎺х幆澧冧笅椹卞姩鐢垫満銆備粎鍦ㄧ數鏈烘偓绌烘垨杞︿綋鍙潬鏀拺銆佸疄楠屽鐢垫簮
 * 宸查檺娴併€佺ず娉㈠櫒/鐢垫祦瑙傛祴鍑嗗瀹屾垚鏃讹紝鎵嶅彲涓存椂鏀逛负 1 骞跺崟鐙紪璇戙€?
 * 姝ｅ紡杩愯鍥轰欢蹇呴』淇濇寔 0銆?
 */
#ifndef PRJ_DRV8870_FACTORY_TEST_ENABLE
#define PRJ_DRV8870_FACTORY_TEST_ENABLE       (0U)
#endif

/* ---- 鐢垫満A/M1(鍙冲悗): CC0=PA8 ---- */
#define PRJ_DRV8870_A_PWM_CH      (0U)

/* ---- 鐢垫満B/M2(鍙冲墠): CC1=PA9 ---- */
#define PRJ_DRV8870_B_PWM_CH      (1U)

/* ---- 鐢垫満C/M3(宸﹀墠): CC2=PB17 ---- */
#define PRJ_DRV8870_C_PWM_CH      (2U)

/* ---- 鐢垫満D/M4(宸﹀悗): CC3=PB2 ---- */
#define PRJ_DRV8870_D_PWM_CH      (3U)


/** 鍏煎鍘烡RV8870閰嶇疆瀹忓悕绉般€?*/
#define PRJ_DRV8870_A_DIR_SIGN  PRJ_MOTOR_A_INSTALL_DIR_SIGN
#define PRJ_DRV8870_B_DIR_SIGN  PRJ_MOTOR_B_INSTALL_DIR_SIGN
#define PRJ_DRV8870_C_DIR_SIGN  PRJ_MOTOR_C_INSTALL_DIR_SIGN
#define PRJ_DRV8870_D_DIR_SIGN  PRJ_MOTOR_D_INSTALL_DIR_SIGN

/** DRV8870 鐢垫満閰嶇疆琛?椤哄簭闇€涓嶣SP_DRV8870_x涓€鑷? */
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
#if (PRJ_DRV8870_SPEED_COMMAND_MAX != (PRJ_DRV8870_PWM_PERIOD / 2U))
#error "DRV8870 backend requires command max equal to half of PWM period"
#endif

/* 涓婂眰鍙娇鐢ㄨ繖浜涙墍閫夊悗绔埆鍚嶏紝涓嶇洿鎺ュ紩鐢ㄨ姱鐗囦笓鐢ㄥ弬鏁般€?*/
#if (PRJ_MOTOR_DRIVER == PRJ_MOTOR_DRIVER_DRV8870)
#define PRJ_MOTOR_PWM_TIMER         PRJ_DRV8870_PWM_TIMER
#define PRJ_MOTOR_PWM_CLK_HZ        PRJ_DRV8870_PWM_CLK_HZ
#define PRJ_MOTOR_PWM_PERIOD        PRJ_DRV8870_PWM_PERIOD
#define PRJ_MOTOR_POWER_STARTUP_MS  PRJ_DRV8870_POWER_STARTUP_MS
#define PRJ_MOTOR_POWER_SETTLE_MS   PRJ_DRV8870_POWER_SETTLE_MS
#else
#define PRJ_MOTOR_PWM_TIMER         PRJ_TB6612_PWM_TIMER
#define PRJ_MOTOR_PWM_CLK_HZ        PRJ_TB6612_PWM_CLK_HZ
#define PRJ_MOTOR_PWM_PERIOD        PRJ_TB6612_PWM_PERIOD
#define PRJ_MOTOR_POWER_STARTUP_MS  PRJ_TB6612_POWER_STARTUP_MS
#define PRJ_MOTOR_POWER_SETTLE_MS   (0U)
#endif

#if (PRJ_DRV8870_FACTORY_TEST_ENABLE != 0U) && \
    (PRJ_MOTOR_DRIVER != PRJ_MOTOR_DRIVER_DRV8870)
#error "DRV8870 factory-test target requires the DRV8870 motor backend"
#endif

#ifdef __cplusplus
}
#endif

#endif /* PROJECT_CONFIG_H */

