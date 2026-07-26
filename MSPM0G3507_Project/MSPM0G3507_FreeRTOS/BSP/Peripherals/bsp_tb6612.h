/**
 * @file    bsp_tb6612.h
 * @brief   TB6612 双输入PWM电机驱动后端
 * @note    该文件是芯片后端，不应被应用层直接包含；应用层使用
 *          bsp_motor.h 的统一接口。
 */
#ifndef BSP_TB6612_H
#define BSP_TB6612_H

#ifdef __cplusplus
extern "C" {
#endif

#include "bsp_common.h"
#include "hal_gpio.h"
#include "hal_timer.h"

typedef enum {
    BSP_TB6612_A = 0,
    BSP_TB6612_B,
    BSP_TB6612_C,
    BSP_TB6612_D,
    BSP_TB6612_COUNT
} bsp_tb6612_id_t;

typedef enum {
    BSP_TB6612_MODE_COAST = 0,
    BSP_TB6612_MODE_BRAKE,
} bsp_tb6612_stop_mode_t;

typedef struct {
    uint32_t        pwm_ch;
    hal_gpio_port_t in1_port;
    uint32_t        in1_pin;
    hal_gpio_port_t in2_port;
    uint32_t        in2_pin;
    int8_t          dir_sign;
} bsp_tb6612_config_t;

/** 可选 STBY 控制；use_standby=false 表示硬件已将 STBY 固定为有效。 */
typedef struct {
    bool            use_standby;
    hal_gpio_port_t standby_port;
    uint32_t        standby_pin;
    bool            active_level;
} bsp_tb6612_power_config_t;

bsp_status_t bsp_tb6612_init(const bsp_tb6612_config_t *cfg,
                             uint32_t count,
                             hal_timer_id_t pwm_timer,
                             uint32_t pwm_period,
                             uint32_t command_max,
                             const bsp_tb6612_power_config_t *power_cfg);

bsp_status_t bsp_tb6612_power_enable(void);
void bsp_tb6612_power_disable(void);
bool bsp_tb6612_power_is_enabled(void);
uint32_t bsp_tb6612_get_command_max(void);

bsp_status_t bsp_tb6612_set_speed(bsp_tb6612_id_t motor,
                                  int32_t command);
bsp_status_t bsp_tb6612_stop(bsp_tb6612_id_t motor,
                             bsp_tb6612_stop_mode_t mode);
void bsp_tb6612_stop_all(void);

#ifdef __cplusplus
}
#endif

#endif /* BSP_TB6612_H */