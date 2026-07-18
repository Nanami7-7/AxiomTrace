/**
 * @file    bsp_motor.h
 * @brief   电机驱动统一门面
 * @note    应用层只依赖本文件；具体后端由 project_config.h 的
 *          PRJ_MOTOR_DRIVER 在编译期选择。默认后端为 DRV8870，
 *          TB6612 作为备用板级配置保留。
 */
#ifndef BSP_MOTOR_H
#define BSP_MOTOR_H

#ifdef __cplusplus
extern "C" {
#endif

#include "bsp_common.h"

/** 统一电机编号；顺序固定为板上 M1~M4。 */
typedef enum {
    BSP_MOTOR_A = 0, /**< M1 / 右后轮 */
    BSP_MOTOR_B,     /**< M2 / 右前轮 */
    BSP_MOTOR_C,     /**< M3 / 左前轮 */
    BSP_MOTOR_D,     /**< M4 / 左后轮 */
    BSP_MOTOR_COUNT
} bsp_motor_id_t;

/** 编译期可选的电机驱动后端。 */
typedef enum {
    BSP_MOTOR_DRIVER_DRV8870 = 1,
    BSP_MOTOR_DRIVER_TB6612  = 2,
} bsp_motor_driver_t;

/** 统一停止语义；不支持的模式由后端安全降级。 */
typedef enum {
    BSP_MOTOR_MODE_COAST = 0,
    BSP_MOTOR_MODE_BRAKE,
} bsp_motor_stop_mode_t;

/** 后端能力位。 */
typedef enum {
    BSP_MOTOR_CAP_POWER_GATE       = (1UL << 0),
    BSP_MOTOR_CAP_TRUE_COAST       = (1UL << 1),
    BSP_MOTOR_CAP_TRUE_BRAKE       = (1UL << 2),
    BSP_MOTOR_CAP_LOCKED_ANTIPHASE = (1UL << 3),
    BSP_MOTOR_CAP_DIRECTION_GPIO   = (1UL << 4),
} bsp_motor_capability_t;

/**
 * @brief 初始化已选择的电机后端，并保持所有电机处于安全停止状态。
 * @note  本函数不会自动使能电机功率；必须显式调用
 *        bsp_motor_power_enable() 并等待 PRJ_MOTOR_POWER_STARTUP_MS。
 */
bsp_status_t bsp_motor_init(void);

/** 以安全停止输出为前置条件，使能所选后端的电机功率/待机控制。 */
bsp_status_t bsp_motor_power_enable(void);

/** 先停止全部电机，再关闭所选后端的电机功率/待机控制。 */
void bsp_motor_power_disable(void);

/** 返回软件命令状态；有反馈的后端同时检查输出锁存器。 */
bool bsp_motor_power_is_enabled(void);

/**
 * @brief 设置有符号电机命令。
 * @param motor M1~M4 对应 BSP_MOTOR_A~D。
 * @param command 范围为 -bsp_motor_get_command_max() 到正最大值；
 *                正值统一表示车体前进方向，负值表示后退，0表示停止。
 */
bsp_status_t bsp_motor_set_speed(bsp_motor_id_t motor, int32_t command);

/** 停止单个电机。DRV8870 锁相硬件会把两种模式降级为中性主动阻尼。 */
bsp_status_t bsp_motor_stop(bsp_motor_id_t motor,
                            bsp_motor_stop_mode_t mode);

/** 使用 BRAKE 语义停止全部电机。 */
void bsp_motor_stop_all(void);

/** 返回统一业务命令最大绝对值；未初始化时返回0。 */
uint32_t bsp_motor_get_command_max(void);

/** 将0~100百分比换算为统一业务命令幅值。 */
uint32_t bsp_motor_percent_to_command(uint32_t percent);

/** 返回当前编译选择的驱动后端。 */
bsp_motor_driver_t bsp_motor_get_driver(void);

/** 返回稳定的英文后端名称："DRV8870" 或 "TB6612"。 */
const char *bsp_motor_get_driver_name(void);

/** 返回当前后端能力位集合。 */
uint32_t bsp_motor_get_capabilities(void);

#ifdef __cplusplus
}
#endif

#endif /* BSP_MOTOR_H */