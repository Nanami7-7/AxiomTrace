/**
 * @file    bsp_motor.c
 * @brief   电机驱动统一门面实现
 * @note    本文件只负责后端选择、板级配置装配和统一类型转换；
 *          芯片特有时序与保护逻辑分别位于 bsp_drv8870.c、
 *          bsp_tb6612.c。
 */
#include "bsp_motor.h"
#include "project_config.h"

#if (PRJ_MOTOR_DRIVER == PRJ_MOTOR_DRIVER_DRV8870)
#include "bsp_drv8870.h"
#elif (PRJ_MOTOR_DRIVER == PRJ_MOTOR_DRIVER_TB6612)
#include "bsp_tb6612.h"
#else
#error "Unsupported PRJ_MOTOR_DRIVER"
#endif

/*
 * The facade intentionally casts the common motor ID to the selected backend
 * ID. Keep that zero-cost mapping compile-time checked so a future enum edit
 * cannot silently route M1~M4 to the wrong physical channel.
 */
#if (PRJ_MOTOR_DRIVER == PRJ_MOTOR_DRIVER_DRV8870)
_Static_assert((int)BSP_MOTOR_A == (int)BSP_DRV8870_A,
               "motor A/DRV8870 ID mapping changed");
_Static_assert((int)BSP_MOTOR_B == (int)BSP_DRV8870_B,
               "motor B/DRV8870 ID mapping changed");
_Static_assert((int)BSP_MOTOR_C == (int)BSP_DRV8870_C,
               "motor C/DRV8870 ID mapping changed");
_Static_assert((int)BSP_MOTOR_D == (int)BSP_DRV8870_D,
               "motor D/DRV8870 ID mapping changed");
_Static_assert((int)BSP_MOTOR_COUNT == (int)BSP_DRV8870_COUNT,
               "motor/DRV8870 channel count changed");
#else
_Static_assert((int)BSP_MOTOR_A == (int)BSP_TB6612_A,
               "motor A/TB6612 ID mapping changed");
_Static_assert((int)BSP_MOTOR_B == (int)BSP_TB6612_B,
               "motor B/TB6612 ID mapping changed");
_Static_assert((int)BSP_MOTOR_C == (int)BSP_TB6612_C,
               "motor C/TB6612 ID mapping changed");
_Static_assert((int)BSP_MOTOR_D == (int)BSP_TB6612_D,
               "motor D/TB6612 ID mapping changed");
_Static_assert((int)BSP_MOTOR_COUNT == (int)BSP_TB6612_COUNT,
               "motor/TB6612 channel count changed");
#endif

#if (PRJ_MOTOR_DRIVER == PRJ_MOTOR_DRIVER_TB6612) && \
    (PRJ_TB6612_BOARD_CONFIG_AVAILABLE == 0U)
/*
 * project_config.h 已发出唯一、可操作的板级配置错误。
 * 此处故意不展开TB6612配置对象和门面实现，避免编译器在主错误之后
 * 继续报告PRJ_TB6612_CONFIGS等无意义的级联未定义标识符。
 */
#else

#if (PRJ_MOTOR_DRIVER == PRJ_MOTOR_DRIVER_DRV8870)
static const bsp_drv8870_config_t s_drv8870_cfg[BSP_MOTOR_COUNT] =
    PRJ_DRV8870_CONFIGS;
#elif (PRJ_MOTOR_DRIVER == PRJ_MOTOR_DRIVER_TB6612)
static const bsp_tb6612_config_t s_tb6612_cfg[BSP_MOTOR_COUNT] =
    PRJ_TB6612_CONFIGS;
static const bsp_tb6612_power_config_t s_tb6612_power_cfg =
    PRJ_TB6612_POWER_CONFIG;
#else
#error "Unsupported PRJ_MOTOR_DRIVER"
#endif

static bool s_motor_inited = false;

bsp_status_t bsp_motor_init(void)
{
    bsp_status_t status;

    if (s_motor_inited) {
        return BSP_OK;
    }

#if (PRJ_MOTOR_DRIVER == PRJ_MOTOR_DRIVER_DRV8870)
    status = bsp_drv8870_init(s_drv8870_cfg, BSP_MOTOR_COUNT,
                              PRJ_DRV8870_PWM_TIMER,
                              PRJ_DRV8870_PWM_PERIOD);
#else
    status = bsp_tb6612_init(s_tb6612_cfg, BSP_MOTOR_COUNT,
                             PRJ_TB6612_PWM_TIMER,
                             PRJ_TB6612_PWM_PERIOD,
                             PRJ_MOTOR_COMMAND_MAX,
                             &s_tb6612_power_cfg);
#endif

    if (status == BSP_OK) {
        s_motor_inited = true;
    }
    return status;
}

bsp_status_t bsp_motor_power_enable(void)
{
    if (!s_motor_inited) {
        return BSP_ERR_NOT_INIT;
    }
#if (PRJ_MOTOR_DRIVER == PRJ_MOTOR_DRIVER_DRV8870)
    return bsp_drv8870_power_enable();
#else
    return bsp_tb6612_power_enable();
#endif
}

void bsp_motor_power_disable(void)
{
#if (PRJ_MOTOR_DRIVER == PRJ_MOTOR_DRIVER_DRV8870)
    bsp_drv8870_power_disable();
#else
    bsp_tb6612_power_disable();
#endif
}

bool bsp_motor_power_is_enabled(void)
{
    if (!s_motor_inited) {
        return false;
    }
#if (PRJ_MOTOR_DRIVER == PRJ_MOTOR_DRIVER_DRV8870)
    return bsp_drv8870_power_is_enabled();
#else
    return bsp_tb6612_power_is_enabled();
#endif
}

bsp_status_t bsp_motor_set_speed(bsp_motor_id_t motor, int32_t command)
{
    if (!s_motor_inited) {
        return BSP_ERR_NOT_INIT;
    }
    if ((uint32_t)motor >= (uint32_t)BSP_MOTOR_COUNT) {
        return BSP_ERR_INVALID_PARAM;
    }
#if (PRJ_MOTOR_DRIVER == PRJ_MOTOR_DRIVER_DRV8870)
    return bsp_drv8870_set_speed((bsp_drv8870_id_t)motor, command);
#else
    return bsp_tb6612_set_speed((bsp_tb6612_id_t)motor, command);
#endif
}

bsp_status_t bsp_motor_stop(bsp_motor_id_t motor,
                            bsp_motor_stop_mode_t mode)
{
    if (!s_motor_inited) {
        return BSP_ERR_NOT_INIT;
    }
    if (((uint32_t)motor >= (uint32_t)BSP_MOTOR_COUNT) ||
        ((mode != BSP_MOTOR_MODE_COAST) &&
         (mode != BSP_MOTOR_MODE_BRAKE))) {
        return BSP_ERR_INVALID_PARAM;
    }
#if (PRJ_MOTOR_DRIVER == PRJ_MOTOR_DRIVER_DRV8870)
    return bsp_drv8870_stop((bsp_drv8870_id_t)motor,
        (mode == BSP_MOTOR_MODE_BRAKE) ? BSP_DRV8870_MODE_BRAKE
                                       : BSP_DRV8870_MODE_COAST);
#else
    return bsp_tb6612_stop((bsp_tb6612_id_t)motor,
        (mode == BSP_MOTOR_MODE_BRAKE) ? BSP_TB6612_MODE_BRAKE
                                       : BSP_TB6612_MODE_COAST);
#endif
}

void bsp_motor_stop_all(void)
{
#if (PRJ_MOTOR_DRIVER == PRJ_MOTOR_DRIVER_DRV8870)
    bsp_drv8870_stop_all();
#else
    bsp_tb6612_stop_all();
#endif
}

uint32_t bsp_motor_get_command_max(void)
{
    if (!s_motor_inited) {
        return 0U;
    }
#if (PRJ_MOTOR_DRIVER == PRJ_MOTOR_DRIVER_DRV8870)
    return bsp_drv8870_get_duty_max();
#else
    return bsp_tb6612_get_command_max();
#endif
}

uint32_t bsp_motor_percent_to_command(uint32_t percent)
{
    uint32_t command_max = bsp_motor_get_command_max();
    uint64_t scaled;

    if (percent > 100U) {
        percent = 100U;
    }
    scaled = (uint64_t)command_max * (uint64_t)percent;
    return (uint32_t)((scaled + 50U) / 100U);
}

bsp_motor_driver_t bsp_motor_get_driver(void)
{
#if (PRJ_MOTOR_DRIVER == PRJ_MOTOR_DRIVER_DRV8870)
    return BSP_MOTOR_DRIVER_DRV8870;
#else
    return BSP_MOTOR_DRIVER_TB6612;
#endif
}

const char *bsp_motor_get_driver_name(void)
{
#if (PRJ_MOTOR_DRIVER == PRJ_MOTOR_DRIVER_DRV8870)
    return "DRV8870";
#else
    return "TB6612";
#endif
}

uint32_t bsp_motor_get_capabilities(void)
{
#if (PRJ_MOTOR_DRIVER == PRJ_MOTOR_DRIVER_DRV8870)
    return BSP_MOTOR_CAP_POWER_GATE |
           BSP_MOTOR_CAP_LOCKED_ANTIPHASE;
#else
    uint32_t caps = BSP_MOTOR_CAP_TRUE_COAST |
                    BSP_MOTOR_CAP_TRUE_BRAKE |
                    BSP_MOTOR_CAP_DIRECTION_GPIO;
#if (PRJ_TB6612_STANDBY_CONTROL_ENABLE != 0U)
    caps |= BSP_MOTOR_CAP_POWER_GATE;
#endif
    return caps;
#endif
}

#endif /* selected backend board configuration available */
