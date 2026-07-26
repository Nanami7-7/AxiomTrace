/**
 * @file    bsp_tb6612.c
 * @brief   TB6612 双输入方向 + 单路PWM电机驱动后端实现
 * @note    正反向切换前先撤销PWM，并插入至少一个上层控制调用周期的
 *          高阻滑行间隔。TB6612 的单路PWM低电平对应短制动；真正的
 *          单通道滑行要求 IN1=IN2=0 且 PWM 保持高电平。
 */
#include "bsp_tb6612.h"
#include "osal_api.h"
#include <limits.h>

#define BSP_TB6612_PWM_CHANNEL_COUNT (4U)

static const bsp_tb6612_config_t *s_cfg = NULL;
static uint32_t s_cfg_count = 0U;
static hal_timer_id_t s_pwm_timer;
static uint32_t s_pwm_period = 0U;
static uint32_t s_command_max = 0U;
static bsp_tb6612_power_config_t s_power_cfg;
static int8_t s_prev_dir[BSP_TB6612_COUNT] = {0};
static bool s_inited = false;
static bool s_power_enabled = false;

static bool tb6612_pin_valid(hal_gpio_port_t port, uint32_t pin)
{
    return (((uint32_t)port < (uint32_t)HAL_GPIO_PORT_COUNT) &&
            (pin != 0U) && ((pin & (pin - 1U)) == 0U));
}

static bool tb6612_same_pin(hal_gpio_port_t lhs_port, uint32_t lhs_pin,
                            hal_gpio_port_t rhs_port, uint32_t rhs_pin)
{
    return ((lhs_port == rhs_port) && (lhs_pin == rhs_pin));
}

static void tb6612_reset_state(void)
{
    s_cfg = NULL;
    s_cfg_count = 0U;
    s_pwm_period = 0U;
    s_command_max = 0U;
    s_power_cfg.use_standby = false;
    s_power_cfg.standby_port = HAL_GPIO_PORT_A;
    s_power_cfg.standby_pin = 0U;
    s_power_cfg.active_level = true;
    for (uint32_t i = 0U; i < BSP_TB6612_COUNT; i++) {
        s_prev_dir[i] = 0;
    }
    s_inited = false;
    s_power_enabled = false;
}

static bsp_status_t tb6612_write_pair(bsp_tb6612_id_t motor,
                                       bool in1, bool in2)
{
    const bsp_tb6612_config_t *cfg = &s_cfg[motor];

    if (hal_gpio_write_pin(cfg->in1_port, cfg->in1_pin, in1) != HAL_OK) {
        return BSP_ERR_HW_FAULT;
    }
    if (hal_gpio_write_pin(cfg->in2_port, cfg->in2_pin, in2) != HAL_OK) {
        return BSP_ERR_HW_FAULT;
    }
    return BSP_OK;
}

static bsp_status_t tb6612_set_pwm(bsp_tb6612_id_t motor,
                                    uint32_t duty)
{
    return (hal_timer_set_pwm_duty(s_pwm_timer, s_cfg[motor].pwm_ch,
                                   duty) == HAL_OK)
        ? BSP_OK : BSP_ERR_HW_FAULT;
}

static bsp_status_t tb6612_set_direction(bsp_tb6612_id_t motor,
                                          int8_t direction)
{
    return (direction > 0)
        ? tb6612_write_pair(motor, true, false)
        : tb6612_write_pair(motor, false, true);
}

/*
 * TB6612 单路PWM真值表要求：IN1=IN2=0、PWM=1 才是高阻 Stop。
 * 先写 PWM=0 进入短制动，再改变方向脚，最后写满周期进入高阻，
 * 可避免方向脚切换时产生意外驱动脉冲。
 */
static bsp_status_t tb6612_set_coast(bsp_tb6612_id_t motor)
{
    if (tb6612_set_pwm(motor, 0U) != BSP_OK) {
        return BSP_ERR_HW_FAULT;
    }
    if (tb6612_write_pair(motor, false, false) != BSP_OK) {
        return BSP_ERR_HW_FAULT;
    }
    return tb6612_set_pwm(motor, s_pwm_period);
}

static bsp_status_t tb6612_set_brake(bsp_tb6612_id_t motor)
{
    if (tb6612_set_pwm(motor, 0U) != BSP_OK) {
        return BSP_ERR_HW_FAULT;
    }
    return tb6612_write_pair(motor, true, true);
}

static bsp_status_t tb6612_force_safe_all(void)
{
    bsp_status_t status = BSP_OK;

    /* 先撤销全部有效驱动；PWM低电平使各H桥进入短制动。 */
    for (uint32_t i = 0U; i < s_cfg_count; i++) {
        if (tb6612_set_pwm((bsp_tb6612_id_t)i, 0U) != BSP_OK) {
            status = BSP_ERR_HW_FAULT;
        }
    }

    /* 再把方向脚统一置低。任一步失败时保持PWM=0，不进入驱动态。 */
    for (uint32_t i = 0U; i < s_cfg_count; i++) {
        if (tb6612_write_pair((bsp_tb6612_id_t)i,
                               false, false) != BSP_OK) {
            status = BSP_ERR_HW_FAULT;
        }
        s_prev_dir[i] = 0;
    }

    /* 全部GPIO成功后才以PWM高电平完成真正的高阻滑行。 */
    if (status == BSP_OK) {
        for (uint32_t i = 0U; i < s_cfg_count; i++) {
            if (tb6612_set_pwm((bsp_tb6612_id_t)i,
                                s_pwm_period) != BSP_OK) {
                status = BSP_ERR_HW_FAULT;
            }
        }
    }
    return status;
}

static uint32_t tb6612_command_to_duty(uint32_t magnitude)
{
    uint64_t numerator = (uint64_t)magnitude * (uint64_t)s_pwm_period;
    return (uint32_t)((numerator + s_command_max - 1U) / s_command_max);
}

bsp_status_t bsp_tb6612_init(const bsp_tb6612_config_t *cfg,
                              uint32_t count,
                              hal_timer_id_t pwm_timer,
                              uint32_t pwm_period,
                              uint32_t command_max,
                              const bsp_tb6612_power_config_t *power_cfg)
{
    uint32_t channel_mask = 0U;
    bool timer_started = false;

    if (s_inited) {
        return BSP_OK;
    }
    if ((cfg == NULL) || (power_cfg == NULL) || (count == 0U) ||
        (count > BSP_TB6612_COUNT) ||
        ((uint32_t)pwm_timer >= (uint32_t)HAL_TIMER_COUNT) ||
        (pwm_period == 0U) || (pwm_period > (uint32_t)INT32_MAX) ||
        (command_max == 0U) || (command_max > (uint32_t)INT32_MAX)) {
        return BSP_ERR_INVALID_PARAM;
    }

    for (uint32_t i = 0U; i < count; i++) {
        uint32_t bit;

        if ((cfg[i].pwm_ch >= BSP_TB6612_PWM_CHANNEL_COUNT) ||
            !tb6612_pin_valid(cfg[i].in1_port, cfg[i].in1_pin) ||
            !tb6612_pin_valid(cfg[i].in2_port, cfg[i].in2_pin) ||
            ((cfg[i].dir_sign != -1) && (cfg[i].dir_sign != 1)) ||
            tb6612_same_pin(cfg[i].in1_port, cfg[i].in1_pin,
                             cfg[i].in2_port, cfg[i].in2_pin)) {
            return BSP_ERR_INVALID_PARAM;
        }

        bit = 1UL << cfg[i].pwm_ch;
        if ((channel_mask & bit) != 0U) {
            return BSP_ERR_INVALID_PARAM;
        }
        channel_mask |= bit;

        for (uint32_t j = 0U; j < i; j++) {
            if (tb6612_same_pin(cfg[i].in1_port, cfg[i].in1_pin,
                                 cfg[j].in1_port, cfg[j].in1_pin) ||
                tb6612_same_pin(cfg[i].in1_port, cfg[i].in1_pin,
                                 cfg[j].in2_port, cfg[j].in2_pin) ||
                tb6612_same_pin(cfg[i].in2_port, cfg[i].in2_pin,
                                 cfg[j].in1_port, cfg[j].in1_pin) ||
                tb6612_same_pin(cfg[i].in2_port, cfg[i].in2_pin,
                                 cfg[j].in2_port, cfg[j].in2_pin)) {
                return BSP_ERR_INVALID_PARAM;
            }
        }
    }

    if (power_cfg->use_standby) {
        if (!tb6612_pin_valid(power_cfg->standby_port,
                               power_cfg->standby_pin)) {
            return BSP_ERR_INVALID_PARAM;
        }
        for (uint32_t i = 0U; i < count; i++) {
            if (tb6612_same_pin(power_cfg->standby_port,
                                 power_cfg->standby_pin,
                                 cfg[i].in1_port, cfg[i].in1_pin) ||
                tb6612_same_pin(power_cfg->standby_port,
                                 power_cfg->standby_pin,
                                 cfg[i].in2_port, cfg[i].in2_pin)) {
                return BSP_ERR_INVALID_PARAM;
            }
        }
    }

    s_cfg = cfg;
    s_cfg_count = count;
    s_pwm_timer = pwm_timer;
    s_pwm_period = pwm_period;
    s_command_max = command_max;
    s_power_cfg = *power_cfg;

    if (s_power_cfg.use_standby &&
        (hal_gpio_write_pin(s_power_cfg.standby_port,
                            s_power_cfg.standby_pin,
                            !s_power_cfg.active_level) != HAL_OK)) {
        tb6612_reset_state();
        return BSP_ERR_HW_FAULT;
    }

    if (hal_timer_start(s_pwm_timer) != HAL_OK) {
        tb6612_reset_state();
        return BSP_ERR_HW_FAULT;
    }
    timer_started = true;

    if (tb6612_force_safe_all() != BSP_OK) {
        if (s_power_cfg.use_standby) {
            (void)hal_gpio_write_pin(s_power_cfg.standby_port,
                                     s_power_cfg.standby_pin,
                                     !s_power_cfg.active_level);
            if (timer_started) {
                (void)hal_timer_stop(s_pwm_timer);
            }
        }
        /* 无STBY时保留定时器运行，以维持最后一次成功写入的安全电平。 */
        tb6612_reset_state();
        return BSP_ERR_HW_FAULT;
    }

    s_power_enabled = false;
    s_inited = true;
    return BSP_OK;
}

bsp_status_t bsp_tb6612_power_enable(void)
{
    bsp_status_t status = BSP_OK;

    if (!s_inited) {
        return BSP_ERR_NOT_INIT;
    }

    OSAL_CRITICAL_SECTION {
        status = tb6612_force_safe_all();
        if ((status == BSP_OK) && s_power_cfg.use_standby) {
            if ((hal_gpio_write_pin(s_power_cfg.standby_port,
                                     s_power_cfg.standby_pin,
                                     s_power_cfg.active_level) != HAL_OK) ||
                (hal_gpio_read_output_latch(s_power_cfg.standby_port,
                                             s_power_cfg.standby_pin) !=
                 s_power_cfg.active_level)) {
                status = BSP_ERR_HW_FAULT;
            }
        }
        if (status == BSP_OK) {
            s_power_enabled = true;
        } else {
            if (s_power_cfg.use_standby) {
                (void)hal_gpio_write_pin(s_power_cfg.standby_port,
                                         s_power_cfg.standby_pin,
                                         !s_power_cfg.active_level);
            }
            s_power_enabled = false;
        }
    }
    return status;
}

void bsp_tb6612_power_disable(void)
{
    OSAL_CRITICAL_SECTION {
        if (s_inited) {
            (void)tb6612_force_safe_all();
        }
        if (s_power_cfg.use_standby) {
            (void)hal_gpio_write_pin(s_power_cfg.standby_port,
                                     s_power_cfg.standby_pin,
                                     !s_power_cfg.active_level);
        }
        s_power_enabled = false;
    }
}

bool bsp_tb6612_power_is_enabled(void)
{
    bool enabled;

    OSAL_CRITICAL_SECTION {
        enabled = s_power_enabled;
        if (enabled && s_power_cfg.use_standby) {
            enabled = (hal_gpio_read_output_latch(s_power_cfg.standby_port,
                                                   s_power_cfg.standby_pin) ==
                       s_power_cfg.active_level);
        }
    }
    return enabled;
}

uint32_t bsp_tb6612_get_command_max(void)
{
    return s_inited ? s_command_max : 0U;
}

bsp_status_t bsp_tb6612_set_speed(bsp_tb6612_id_t motor,
                                   int32_t command)
{
    bsp_status_t status = BSP_OK;
    int32_t adjusted;
    int8_t direction;
    uint32_t magnitude;
    uint32_t duty;

    if (!s_inited) {
        return BSP_ERR_NOT_INIT;
    }
    if ((uint32_t)motor >= s_cfg_count) {
        return BSP_ERR_INVALID_PARAM;
    }

    if (command > (int32_t)s_command_max) {
        command = (int32_t)s_command_max;
    } else if (command < -(int32_t)s_command_max) {
        command = -(int32_t)s_command_max;
    }

    adjusted = (s_cfg[motor].dir_sign < 0) ? -command : command;
    if (adjusted == 0) {
        return bsp_tb6612_stop(motor, BSP_TB6612_MODE_BRAKE);
    }

    direction = (adjusted > 0) ? 1 : -1;
    magnitude = (uint32_t)((adjusted > 0) ? adjusted : -adjusted);
    duty = tb6612_command_to_duty(magnitude);

    OSAL_CRITICAL_SECTION {
        if (!s_power_enabled) {
            status = BSP_ERR_NOT_READY;
        } else if ((s_prev_dir[motor] != 0) &&
                   (s_prev_dir[motor] != direction)) {
            /* 跨向先进入真正高阻滑行；下一个控制周期才允许新方向。 */
            status = tb6612_set_coast(motor);
            s_prev_dir[motor] = 0;
        } else {
            if (s_prev_dir[motor] == 0) {
                if ((tb6612_set_pwm(motor, 0U) != BSP_OK) ||
                    (tb6612_set_direction(motor, direction) != BSP_OK)) {
                    status = BSP_ERR_HW_FAULT;
                }
            }
            if ((status == BSP_OK) &&
                (tb6612_set_pwm(motor, duty) != BSP_OK)) {
                status = BSP_ERR_HW_FAULT;
            }
            if (status == BSP_OK) {
                s_prev_dir[motor] = direction;
            }
        }
    }

    if (status == BSP_ERR_HW_FAULT) {
        bsp_tb6612_power_disable();
    }
    return status;
}

bsp_status_t bsp_tb6612_stop(bsp_tb6612_id_t motor,
                              bsp_tb6612_stop_mode_t mode)
{
    bsp_status_t status = BSP_OK;

    if (!s_inited) {
        return BSP_ERR_NOT_INIT;
    }
    if (((uint32_t)motor >= s_cfg_count) ||
        ((mode != BSP_TB6612_MODE_COAST) &&
         (mode != BSP_TB6612_MODE_BRAKE))) {
        return BSP_ERR_INVALID_PARAM;
    }

    OSAL_CRITICAL_SECTION {
        status = (mode == BSP_TB6612_MODE_BRAKE)
            ? tb6612_set_brake(motor)
            : tb6612_set_coast(motor);
        s_prev_dir[motor] = 0;
    }

    if (status == BSP_ERR_HW_FAULT) {
        bsp_tb6612_power_disable();
    }
    return status;
}

void bsp_tb6612_stop_all(void)
{
    if (!s_inited) {
        return;
    }
    for (uint32_t i = 0U; i < s_cfg_count; i++) {
        if (bsp_tb6612_stop((bsp_tb6612_id_t)i,
                             BSP_TB6612_MODE_BRAKE) != BSP_OK) {
            break;
        }
    }
}