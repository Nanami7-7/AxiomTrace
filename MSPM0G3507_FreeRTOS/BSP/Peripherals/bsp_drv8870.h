/**
 * @file    bsp_drv8870.h
 * @brief   DRV8870 电机驱动接口 (锁相驱动 Locked Anti-Phase)
 * @note    基于 hal_timer(PWM) 实现单 PWM 锁相驱动
 *          四电机驱动(DRV8870DDAR), 每个电机仅需1个PWM通道
 *          PWM由TIMA0产生(4通道CC0~CC3)
 *
 *          硬件拓扑:
 *            MCU PWM ──┬──→ DRV8870 IN1 (直连)
 *                      └──→ S8050 反相器 ──→ DRV8870 IN2
 *            IN1 与 IN2 由同一PWM及外部反相器关联，50%占空比 = 平均电压中性
 *
 *          当前实测有效区(可在project_config.h中调整):
 *          | PWM占空比 | 平均输出/机械状态 |
 *          |-----------|-------------------|
 *          | 0%~<40%   | 反转有效区        |
 *          | 40%~55%   | 机械停止死区      |
 *          | >55%~100% | 正转有效区        |
 *          | 50%       | 零命令中性点      |
 *
 *          bsp_drv8870_set_speed()对有符号命令执行分段线性补偿，任何非零
 *          命令都会映射到死区之外；示波器工厂接口仍允许原始绝对compare。
 *
 *          与 TB6612FNG 的关键差异:
 *            1. 无需 IN1/IN2 方向 GPIO(硬件反相器自动生成互补信号)
 *            2. 速度与方向统一编码到单路 PWM 占空比
 *            3. 停止命令 = 50%占空比的平均电压中性，非 IN1=IN2=1 硬件刹车
 *            4. 芯片内部死区只保护H桥MOSFET直通；外部反相器的边沿
 *               偏差和电机电流反向仍须通过示波器验证，并在应用层限斜率。
 *
 *          参考文档: DRV8870电机驱动项目技术文档.md (TI SLVSCY8B)
 *
 *          @warning 文档审核发现的问题(不影响驱动实现):
 *            - 文档§3.1.1的STM32 HAL代码示例不适用本工程(MSPM0G3507)
 *            - 文档§3.3.3的硬件刹车描述有误(VREF/VM控制不能实现Brake模式)
 *            - 文档§1.2的引脚分配基于其他MCU, 本工程使用PA8/PA9/PB17/PB2
 */
#ifndef BSP_DRV8870_H
#define BSP_DRV8870_H

#ifdef __cplusplus
extern "C" {
#endif

/* ======================== 包含 ======================== */
#include "bsp_common.h"
#include "hal_timer.h"

/* ======================== 类型定义 ======================== */

/** 电机编号枚举 */
typedef enum {
    BSP_DRV8870_A = 0,   /**< 电机A/M1(右后): CC0=PA8 */
    BSP_DRV8870_B,       /**< 电机B/M2(右前): CC1=PA9 */
    BSP_DRV8870_C,       /**< 电机C/M3(左前): CC2=PB17 */
    BSP_DRV8870_D,       /**< 电机D/M4(左后): CC3=PB2 */
    BSP_DRV8870_COUNT    /**< 电机总数 */
} bsp_drv8870_id_t;

/**
 * 电机停止模式
 * @note DRV8870 锁相驱动下, IN2 由硬件反相器驱动, 无法独立控制.
 *       两种模式均通过 50% 占空比实现主动阻尼.
 *       BRAKE 模式保留用于未来硬件扩展(如独立 IN2 控制).
 */
typedef enum {
    BSP_DRV8870_MODE_COAST = 0, /**< 滑行(锁相驱动下等效于主动阻尼) */
    BSP_DRV8870_MODE_BRAKE,     /**< 刹车(主动阻尼, 50%占空比) */
} bsp_drv8870_stop_mode_t;

/**
 * 电机配置结构体 (DRV8870 锁相驱动, 无需方向引脚)
 * @note  deadband_low_percent和deadband_high_percent均为绝对PWM百分比；
 *        low~high(含边界)视为停止死区。zero_duty_offset只微调零命令中性点，
 *        调整后仍必须落在死区内部。
 */
typedef struct {
    uint32_t pwm_ch;                  /**< PWM通道索引(CC0~CC3) */
    int8_t   dir_sign;                /**< 安装方向修正(+1/-1, 0视为+1) */
    int16_t  zero_duty_offset;        /**< 中性compare偏移，单位为timer tick */
    uint8_t  deadband_low_percent;    /**< 反转有效区上边界，合法范围1~98 */
    uint8_t  neutral_percent;         /**< 零命令占空比，必须位于死区内 */
    uint8_t  deadband_high_percent;   /**< 正转有效区下边界，合法范围2~99 */
} bsp_drv8870_config_t;

/* ======================== 函数接口 ======================== */

/**
 * @brief  初始化 DRV8870 电机驱动
 * @param  cfg       电机配置表
 * @param  count     配置数量(<=BSP_DRV8870_COUNT)
 * @param  pwm_timer PWM定时器实例
 * @param  pwm_period PWM周期(占空比最大值)
 * @note   所有通道写入50%中性占空比。PWM 定时器已由
 *         SYSCFG_DL_init()配置并启动；本函数不停止计数器，避免在锁相
 *         硬件中引入依赖定时器输出保持状态的静态驱动风险。
 * @retval BSP_OK 初始化成功
 */
bsp_status_t bsp_drv8870_init(const bsp_drv8870_config_t *cfg,
                               uint32_t count,
                               hal_timer_id_t pwm_timer,
                               uint32_t pwm_period);

/**
 * @brief  Enable the active-high PB19 DRV8870 motor-power gate.
 * @note   All four PWM channels are first restored to 50% neutral. This API
 *         does not block; task context must wait PRJ_DRV8870_POWER_STARTUP_MS
 *         before issuing a non-zero command.
 */
bsp_status_t bsp_drv8870_power_enable(void);

/** Restore all channels to neutral, then drive PB19 low without blocking. */
void bsp_drv8870_power_disable(void);

/** Return true when software state and the PB19 output latch are both ON.
 *  This is commanded-state verification, not physical VIN_OUT feedback. */
bool bsp_drv8870_power_is_enabled(void);

/**
 * @brief  获取有符号速度命令的最大绝对值
 * @retval 命令最大绝对值(PWM_PERIOD/2，未初始化返回0)
 */
uint32_t bsp_drv8870_get_duty_max(void);

/**
 * @brief  占空比百分比换算(绝对duty值): percent(0~100) -> duty值
 * @param  percent 占空比百分比
 * @retval duty值
 * @note   此函数是示波器/诊断使用的原始绝对duty换算，不执行死区补偿:
 *           0%   -> duty=0
 *           40%  -> duty=400 (死区下边界)
 *           50%  -> duty=500 (零命令中性)
 *           55%  -> duty=550 (死区上边界)
 *           100% -> duty=1000
 *         正常运动控制应调用bsp_drv8870_set_speed()。
 */
uint32_t bsp_drv8870_percent_to_absolute_duty(uint32_t percent);
/**
 * @brief Start a persistent oscilloscope test session.
 * @note  Factory-test build only. All four channels are first set to their
 *        exactly 50.0% duty, then PB19 is enabled. Ownership remains held
 *        until bsp_drv8870_hw_scope_stop() or MCU reset.
 */
bsp_status_t bsp_drv8870_hw_scope_start(void);

/** Set one channel to an absolute compare value in an active scope session. */
bsp_status_t bsp_drv8870_hw_scope_set_compare(bsp_drv8870_id_t motor,
                                               uint32_t compare);

/** Set all four channels to one absolute compare value. */
bsp_status_t bsp_drv8870_hw_scope_set_all_compare(uint32_t compare);

/** Read one channel's current compare value. */
bsp_status_t bsp_drv8870_hw_scope_get_compare(bsp_drv8870_id_t motor,
                                               uint32_t *compare);

/** Return true while the factory scope session owns the PWM channels. */
bool bsp_drv8870_hw_scope_is_active(void);

/** Restore neutral PWM, disable PB19, and release the factory scope session. */
bsp_status_t bsp_drv8870_hw_scope_stop(void);

/**
 * @brief  设置电机速度和方向 (锁相驱动)
 * @param  motor  电机编号
 * @param  speed  有符号速度命令(-PWM_PERIOD/2 ~ +PWM_PERIOD/2)
 *               正值=车体前进方向，负值=车体后退方向，0=中性停止。
 * @note   驱动按配置的安装方向和物理死区执行分段线性映射：
 *           speed == 0 -> neutral_percent + zero_duty_offset
 *           speed > 0  -> (deadband_high, 100%]
 *           speed < 0  -> [0%, deadband_low)
 *         默认死区为40%~55%，因此非零命令不会落入机械死区。方向反转时
 *         两个有效区自动交换。该补偿会在跨零时产生占空比跳变，控制层仍
 *         应限制斜率并避免在零点附近高频正反切换。
 * @retval BSP_OK           设置成功
 * @retval BSP_ERR_INVALID_PARAM 电机编号越界
 * @retval BSP_ERR_NOT_READY 电机主电源尚未使能
 */
bsp_status_t bsp_drv8870_set_speed(bsp_drv8870_id_t motor,
                                    int32_t speed);

/**
 * @brief  停止电机（写入平均电压中性命令）
 * @param  motor 电机编号
 * @param  mode  停止模式（现有锁相硬件下均等效为50%中性；不具备真Coast/Brake）
 * @retval BSP_OK           停止成功
 * @retval BSP_ERR_INVALID_PARAM 参数无效
 */
bsp_status_t bsp_drv8870_stop(bsp_drv8870_id_t motor,
                               bsp_drv8870_stop_mode_t mode);

/**
 * @brief  停止所有电机（写入50%平均电压中性）
 */
void bsp_drv8870_stop_all(void);

/* ======================== 兼容宏 ======================== */
/*
 * 应用层历史代码使用 BSP_MOTOR_COUNT 作为数组大小/循环上限,
 * 切换到 DRV8870 驱动后通过此兼容宏保持源码兼容, 避免大面积改名.
 * 值与 BSP_DRV8870_COUNT 一致(=4).
 */
#ifndef BSP_MOTOR_COUNT
#define BSP_MOTOR_COUNT    BSP_DRV8870_COUNT
#endif

#ifdef __cplusplus
}
#endif

#endif /* BSP_DRV8870_H */
