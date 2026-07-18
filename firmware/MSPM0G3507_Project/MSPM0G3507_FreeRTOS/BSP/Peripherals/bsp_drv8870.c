/**
 * @file    bsp_drv8870.c
 * @brief   DRV8870 电机驱动实现（现有单 PWM 锁相硬件拓扑）
 * @note    MCU PWM 直连 IN1，S8050 反相后驱动 IN2。
 *          50% 占空比仅表示平均电压中性；该硬件拓扑不能产生受控的
 *          IN1=IN2=0 Coast 或 IN1=IN2=1 Brake 状态。
 */
#include "bsp_drv8870.h"
#include "hal_gpio.h"
#include "hal_timer.h"
#include "osal_api.h"
#include "project_config.h"
#include <limits.h>

/* ======================== 私有常量 ======================== */

/** TIMA0 当前用于四路电机 PWM 的 CC 数量。 */
#define BSP_DRV8870_PWM_CHANNEL_COUNT       (4U)

/* ======================== 私有变量 ======================== */

/** 电机配置表（由应用层传入，生命周期必须覆盖驱动运行期）。 */
static const bsp_drv8870_config_t *s_drv8870_cfg = NULL;
static uint32_t s_drv8870_cfg_count = 0U;
static hal_timer_id_t s_drv8870_pwm_timer;
static uint32_t s_drv8870_pwm_period = 0U;
static uint32_t s_drv8870_deadband_low[BSP_DRV8870_COUNT];
static uint32_t s_drv8870_neutral[BSP_DRV8870_COUNT];
static uint32_t s_drv8870_deadband_high[BSP_DRV8870_COUNT];
static bool s_drv8870_inited = false;
static bool s_drv8870_power_enabled = false;

/**
 * 工厂脉冲测试独占标志。
 * 正常 set_speed 在短临界区内检查此标志，避免控制任务覆盖测试脉冲。
 * stop/stop_all 仍允许执行，因安全停车必须能打断测试。
 */
#if (PRJ_DRV8870_FACTORY_TEST_ENABLE != 0U)
static volatile bool s_drv8870_hw_test_active = false;
#endif

/* ======================== 私有函数 ======================== */

static inline int32_t clamp_int32(int32_t val, int32_t min_val,
                                  int32_t max_val)
{
    if (val < min_val) {
        return min_val;
    }
    if (val > max_val) {
        return max_val;
    }
    return val;
}

/** 百分比换算为compare，使用四舍五入并避免乘法溢出。 */
static uint32_t drv8870_percent_to_compare(uint32_t period,
                                            uint32_t percent)
{
    uint64_t scaled = (uint64_t)period * (uint64_t)percent;
    return (uint32_t)((scaled + 50U) / 100U);
}

/** ceil(value * span / full_scale)，非零输入保证至少跨过1个compare。 */
static uint32_t drv8870_scale_ceil(uint32_t value, uint32_t span,
                                    uint32_t full_scale)
{
    uint64_t numerator = (uint64_t)value * (uint64_t)span;
    return (uint32_t)((numerator + full_scale - 1U) / full_scale);
}

/** 返回已在初始化阶段验证过的零命令中性compare。 */
static uint32_t drv8870_neutral_duty(bsp_drv8870_id_t motor)
{
    return s_drv8870_neutral[motor];
}

static uint32_t speed_to_duty(bsp_drv8870_id_t motor, int32_t speed)
{
    int8_t dir_sign = s_drv8870_cfg[motor].dir_sign;
    int32_t command_max = (int32_t)(s_drv8870_pwm_period / 2U);
    uint32_t magnitude;
    uint32_t delta;

    if (dir_sign == 0) {
        dir_sign = 1;
    }

    /* 先限幅，再取方向，避免 INT32_MIN × -1 的有符号溢出。 */
    speed = clamp_int32(speed, -command_max, command_max);
    if (dir_sign < 0) {
        speed = -speed;
    }

    if (speed == 0) {
        return drv8870_neutral_duty(motor);
    }

    if (speed > 0) {
        uint32_t high = s_drv8870_deadband_high[motor];
        uint32_t span = s_drv8870_pwm_period - high;

        magnitude = (uint32_t)speed;
        delta = drv8870_scale_ceil(magnitude, span,
                                    (uint32_t)command_max);
        return high + delta; /* 严格大于死区上边界。 */
    }

    magnitude = (uint32_t)(-speed);
    delta = drv8870_scale_ceil(magnitude,
                                s_drv8870_deadband_low[motor],
                                (uint32_t)command_max);
    return s_drv8870_deadband_low[motor] - delta;
}

/** 调用方必须已经完成初始化和电机编号检查。 */
static bsp_status_t drv8870_apply_speed(bsp_drv8870_id_t motor,
                                        int32_t speed)
{
    hal_status_t ret = hal_timer_set_pwm_duty(s_drv8870_pwm_timer,
                                               s_drv8870_cfg[motor].pwm_ch,
                                               speed_to_duty(motor, speed));
    return (ret == HAL_OK) ? BSP_OK : BSP_ERR_HW_FAULT;
}

/* Caller provides concurrency protection. */
static bsp_status_t drv8870_apply_neutral_all(void)
{
    for (uint32_t i = 0U; i < s_drv8870_cfg_count; i++) {
        if (drv8870_apply_speed((bsp_drv8870_id_t)i, 0) != BSP_OK) {
            return BSP_ERR_HW_FAULT;
        }
    }
    return BSP_OK;
}

#if (PRJ_DRV8870_FACTORY_TEST_ENABLE != 0U)
static bool drv8870_try_enter_factory_test(void)
{
    bool entered = false;

    OSAL_CRITICAL_SECTION {
        if (!s_drv8870_hw_test_active) {
            s_drv8870_hw_test_active = true;
            entered = true;
        }
    }

    return entered;
}

static void drv8870_leave_factory_test(void)
{
    OSAL_CRITICAL_SECTION {
        s_drv8870_hw_test_active = false;
    }
}
#endif

/* ======================== 公共函数实现 ======================== */

bsp_status_t bsp_drv8870_init(const bsp_drv8870_config_t *cfg,
                               uint32_t count,
                               hal_timer_id_t pwm_timer,
                               uint32_t pwm_period)
{
    uint32_t channel_mask = 0U;
    uint32_t deadband_low[BSP_DRV8870_COUNT];
    uint32_t neutral[BSP_DRV8870_COUNT];
    uint32_t deadband_high[BSP_DRV8870_COUNT];

    if (s_drv8870_inited) {
        return BSP_OK;
    }

    /* Fail-safe default: keep VIN_OUT off during all validation/setup. */
    if (hal_gpio_write_pin(PRJ_DRV8870_POWER_PORT,
                           PRJ_DRV8870_POWER_PIN,
                           PRJ_DRV8870_POWER_OFF_LEVEL) != HAL_OK) {
        return BSP_ERR_HW_FAULT;
    }
    s_drv8870_power_enabled = false;

    if ((cfg == NULL) || (count == 0U) ||
        (count > BSP_DRV8870_COUNT) || (pwm_period < 2U) ||
        (pwm_period > (uint32_t)INT32_MAX)) {
        return BSP_ERR_INVALID_PARAM;
    }

    for (uint32_t i = 0U; i < count; i++) {
        uint32_t bit;
        int64_t adjusted_neutral;

        if (cfg[i].pwm_ch >= BSP_DRV8870_PWM_CHANNEL_COUNT) {
            return BSP_ERR_INVALID_PARAM;
        }
        if ((cfg[i].dir_sign < -1) || (cfg[i].dir_sign > 1)) {
            return BSP_ERR_INVALID_PARAM;
        }
        if ((cfg[i].deadband_low_percent == 0U) ||
            (cfg[i].deadband_high_percent >= 100U) ||
            (cfg[i].deadband_low_percent >=
             cfg[i].deadband_high_percent) ||
            (cfg[i].neutral_percent < cfg[i].deadband_low_percent) ||
            (cfg[i].neutral_percent > cfg[i].deadband_high_percent)) {
            return BSP_ERR_INVALID_PARAM;
        }

        deadband_low[i] = drv8870_percent_to_compare(
            pwm_period, cfg[i].deadband_low_percent);
        neutral[i] = drv8870_percent_to_compare(
            pwm_period, cfg[i].neutral_percent);
        deadband_high[i] = drv8870_percent_to_compare(
            pwm_period, cfg[i].deadband_high_percent);
        adjusted_neutral = (int64_t)neutral[i] +
                           (int64_t)cfg[i].zero_duty_offset;

        if ((deadband_low[i] == 0U) ||
            (deadband_high[i] >= pwm_period) ||
            (deadband_low[i] >= deadband_high[i]) ||
            (adjusted_neutral < (int64_t)deadband_low[i]) ||
            (adjusted_neutral > (int64_t)deadband_high[i])) {
            return BSP_ERR_INVALID_PARAM;
        }
        neutral[i] = (uint32_t)adjusted_neutral;

        bit = (1UL << cfg[i].pwm_ch);
        if ((channel_mask & bit) != 0U) {
            return BSP_ERR_INVALID_PARAM;
        }
        channel_mask |= bit;
    }

    s_drv8870_cfg = cfg;
    s_drv8870_cfg_count = count;
    s_drv8870_pwm_timer = pwm_timer;
    s_drv8870_pwm_period = pwm_period;
    for (uint32_t i = 0U; i < count; i++) {
        s_drv8870_deadband_low[i] = deadband_low[i];
        s_drv8870_neutral[i] = neutral[i];
        s_drv8870_deadband_high[i] = deadband_high[i];
    }

    /*
     * SysConfig 已在 PWM 输出接通前写入 50% 比较值；此处只重写中性值，
     * 不停止计数器。对于锁相硬件，停止计数器后的引脚静态电平取决于
     * Timer 输出保持行为，不能假定它仍是安全中性状态。
     *
     * 此顺序不能消除 MCU 复位到初始化之间的上电风险；该风险只能由
     * PCB 级 EN/VM 门控解决。
     */
    if (drv8870_apply_neutral_all() != BSP_OK) {
        return BSP_ERR_HW_FAULT;
    }

    s_drv8870_inited = true;
    return BSP_OK;
}

bsp_status_t bsp_drv8870_power_enable(void)
{
    bsp_status_t status = BSP_OK;

    if (!s_drv8870_inited) {
        return BSP_ERR_NOT_INIT;
    }

    OSAL_CRITICAL_SECTION {
        /* Neutral first, then power: never energize a stale non-zero compare. */
        status = drv8870_apply_neutral_all();
        if (status == BSP_OK) {
            if ((hal_gpio_write_pin(PRJ_DRV8870_POWER_PORT,
                                    PRJ_DRV8870_POWER_PIN,
                                    PRJ_DRV8870_POWER_ON_LEVEL) == HAL_OK) &&
                (hal_gpio_read_output_latch(PRJ_DRV8870_POWER_PORT,
                                            PRJ_DRV8870_POWER_PIN) ==
                 PRJ_DRV8870_POWER_ON_LEVEL)) {
                s_drv8870_power_enabled = true;
            } else {
                status = BSP_ERR_HW_FAULT;
            }
        }
        if (status != BSP_OK) {
            /* A failed precondition must leave the physical power gate OFF. */
            (void)hal_gpio_write_pin(PRJ_DRV8870_POWER_PORT,
                                     PRJ_DRV8870_POWER_PIN,
                                     PRJ_DRV8870_POWER_OFF_LEVEL);
            s_drv8870_power_enabled = false;
        }
    }

    return status;
}

void bsp_drv8870_power_disable(void)
{
    OSAL_CRITICAL_SECTION {
        if (s_drv8870_inited) {
            (void)drv8870_apply_neutral_all();
        }
        (void)hal_gpio_write_pin(PRJ_DRV8870_POWER_PORT,
                                 PRJ_DRV8870_POWER_PIN,
                                 PRJ_DRV8870_POWER_OFF_LEVEL);
        s_drv8870_power_enabled = false;
    }
}

bool bsp_drv8870_power_is_enabled(void)
{
    bool enabled;

    OSAL_CRITICAL_SECTION {
        enabled = s_drv8870_power_enabled &&
                  (hal_gpio_read_output_latch(PRJ_DRV8870_POWER_PORT,
                                              PRJ_DRV8870_POWER_PIN) ==
                   PRJ_DRV8870_POWER_ON_LEVEL);
    }
    return enabled;
}

bsp_status_t bsp_drv8870_set_speed(bsp_drv8870_id_t motor, int32_t speed)
{
    bsp_status_t status = BSP_OK;

    if (!s_drv8870_inited) {
        return BSP_ERR_NOT_INIT;
    }
    if ((uint32_t)motor >= s_drv8870_cfg_count) {
        return BSP_ERR_INVALID_PARAM;
    }

    /* Serialize command writes against factory test and power transitions. */
    OSAL_CRITICAL_SECTION {
        if (!s_drv8870_power_enabled) {
            status = BSP_ERR_NOT_READY;
        } else {
#if (PRJ_DRV8870_FACTORY_TEST_ENABLE != 0U)
            if (s_drv8870_hw_test_active) {
                status = BSP_ERR_BUSY;
            } else {
                status = drv8870_apply_speed(motor, speed);
            }
#else
            status = drv8870_apply_speed(motor, speed);
#endif
        }
    }

    return status;
}

bsp_status_t bsp_drv8870_stop(bsp_drv8870_id_t motor,
                               bsp_drv8870_stop_mode_t mode)
{
    bsp_status_t status = BSP_OK;

    if (!s_drv8870_inited) {
        return BSP_ERR_NOT_INIT;
    }
    if ((uint32_t)motor >= s_drv8870_cfg_count) {
        return BSP_ERR_INVALID_PARAM;
    }

    /*
     * 现有单PWM锁相拓扑无法独立控制IN1/IN2；两种mode都只能写入
     * 配置的中性占空比。保留枚举仅用于兼容上层接口和未来硬件版本。
     */
    (void)mode;
    OSAL_CRITICAL_SECTION {
#if (PRJ_DRV8870_FACTORY_TEST_ENABLE != 0U)
        if (s_drv8870_hw_test_active) {
            status = BSP_ERR_BUSY;
        } else
#endif
        {
            hal_status_t ret = hal_timer_set_pwm_duty(
                s_drv8870_pwm_timer,
                s_drv8870_cfg[motor].pwm_ch,
                drv8870_neutral_duty(motor));
            status = (ret == HAL_OK) ? BSP_OK : BSP_ERR_HW_FAULT;
        }
    }
    return status;
}

void bsp_drv8870_stop_all(void)
{
    for (uint32_t i = 0U; i < s_drv8870_cfg_count; i++) {
        (void)bsp_drv8870_stop((bsp_drv8870_id_t)i,
                               BSP_DRV8870_MODE_BRAKE);
    }
}

uint32_t bsp_drv8870_get_duty_max(void)
{
    return s_drv8870_pwm_period / 2U;
}

uint32_t bsp_drv8870_percent_to_absolute_duty(uint32_t percent)
{
    if (s_drv8870_pwm_period == 0U) {
        return 0U;
    }
    if (percent > 100U) {
        percent = 100U;
    }

    /* 原始绝对duty，不执行40%~55%死区补偿。 */
    return drv8870_percent_to_compare(s_drv8870_pwm_period, percent);
}

bsp_status_t bsp_drv8870_hw_scope_start(void)
{
#if (PRJ_DRV8870_FACTORY_TEST_ENABLE == 0U)
    return BSP_ERR_UNSUPPORTED;
#else
    bsp_status_t status = BSP_OK;

    if (!s_drv8870_inited) {
        return BSP_ERR_NOT_INIT;
    }
    if (bsp_drv8870_hw_scope_is_active()) {
        return BSP_OK;
    }
    if (!drv8870_try_enter_factory_test()) {
        return BSP_ERR_BUSY;
    }

    /* Exact requested sequence: all compares=50.0%, then PB19 rises. */
    bsp_drv8870_power_disable();
    osal_task_delay_ms(PRJ_DRV8870_POWER_SETTLE_MS);
    OSAL_CRITICAL_SECTION {
        uint32_t half_period = s_drv8870_pwm_period / 2U;

        for (uint32_t i = 0U; i < s_drv8870_cfg_count; i++) {
            if (hal_timer_set_pwm_duty(
                    s_drv8870_pwm_timer,
                    s_drv8870_cfg[i].pwm_ch,
                    half_period) != HAL_OK) {
                status = BSP_ERR_HW_FAULT;
                break;
            }
        }
        if (status == BSP_OK) {
            if ((hal_gpio_write_pin(PRJ_DRV8870_POWER_PORT,
                                    PRJ_DRV8870_POWER_PIN,
                                    PRJ_DRV8870_POWER_ON_LEVEL) == HAL_OK) &&
                (hal_gpio_read_output_latch(PRJ_DRV8870_POWER_PORT,
                                            PRJ_DRV8870_POWER_PIN) ==
                 PRJ_DRV8870_POWER_ON_LEVEL)) {
                s_drv8870_power_enabled = true;
            } else {
                status = BSP_ERR_HW_FAULT;
                s_drv8870_power_enabled = false;
            }
        }
    }
    if (status == BSP_OK) {
        osal_task_delay_ms(PRJ_DRV8870_POWER_STARTUP_MS);
        if (!bsp_drv8870_power_is_enabled()) {
            status = BSP_ERR_HW_FAULT;
        }
    }

    if (status == BSP_OK) {
        uint32_t expected = s_drv8870_pwm_period / 2U;
        for (uint32_t i = 0U; i < s_drv8870_cfg_count; i++) {
            uint32_t actual = hal_timer_get_capture_value(
                s_drv8870_pwm_timer, s_drv8870_cfg[i].pwm_ch);
            if (actual != expected) {
                status = BSP_ERR_HW_FAULT;
                break;
            }
        }
    }

    if (status != BSP_OK) {
        /* A failed start must never leave VM energized or ownership latched. */
        bsp_drv8870_power_disable();
        drv8870_leave_factory_test();
    }
    return status;
#endif
}

bsp_status_t bsp_drv8870_hw_scope_set_compare(bsp_drv8870_id_t motor,
                                               uint32_t compare)
{
#if (PRJ_DRV8870_FACTORY_TEST_ENABLE == 0U)
    (void)motor;
    (void)compare;
    return BSP_ERR_UNSUPPORTED;
#else
    bsp_status_t status = BSP_OK;

    if (!s_drv8870_inited) {
        return BSP_ERR_NOT_INIT;
    }
    if (((uint32_t)motor >= s_drv8870_cfg_count) ||
        (compare > s_drv8870_pwm_period)) {
        return BSP_ERR_INVALID_PARAM;
    }

    OSAL_CRITICAL_SECTION {
        if (!s_drv8870_hw_test_active) {
            status = BSP_ERR_NOT_READY;
        } else if (!s_drv8870_power_enabled) {
            status = BSP_ERR_NOT_READY;
        } else if (hal_gpio_read_output_latch(PRJ_DRV8870_POWER_PORT,
                                               PRJ_DRV8870_POWER_PIN) !=
                   PRJ_DRV8870_POWER_ON_LEVEL) {
            status = BSP_ERR_HW_FAULT;
        } else if (hal_timer_set_pwm_duty(
                       s_drv8870_pwm_timer,
                       s_drv8870_cfg[motor].pwm_ch,
                       compare) != HAL_OK) {
            status = BSP_ERR_HW_FAULT;
        } else if (hal_timer_get_capture_value(
                       s_drv8870_pwm_timer,
                       s_drv8870_cfg[motor].pwm_ch) != compare) {
            status = BSP_ERR_HW_FAULT;
        }
    }

    if (status == BSP_ERR_HW_FAULT) {
        (void)bsp_drv8870_hw_scope_stop();
    }
    return status;
#endif
}

bsp_status_t bsp_drv8870_hw_scope_set_all_compare(uint32_t compare)
{
#if (PRJ_DRV8870_FACTORY_TEST_ENABLE == 0U)
    (void)compare;
    return BSP_ERR_UNSUPPORTED;
#else
    bsp_status_t status = BSP_OK;

    if (!s_drv8870_inited) {
        return BSP_ERR_NOT_INIT;
    }
    if (compare > s_drv8870_pwm_period) {
        return BSP_ERR_INVALID_PARAM;
    }

    OSAL_CRITICAL_SECTION {
        if (!s_drv8870_hw_test_active) {
            status = BSP_ERR_NOT_READY;
        } else if (!s_drv8870_power_enabled) {
            status = BSP_ERR_NOT_READY;
        } else if (hal_gpio_read_output_latch(PRJ_DRV8870_POWER_PORT,
                                               PRJ_DRV8870_POWER_PIN) !=
                   PRJ_DRV8870_POWER_ON_LEVEL) {
            status = BSP_ERR_HW_FAULT;
        } else {
            for (uint32_t i = 0U; i < s_drv8870_cfg_count; i++) {
                if (hal_timer_set_pwm_duty(
                        s_drv8870_pwm_timer,
                        s_drv8870_cfg[i].pwm_ch,
                        compare) != HAL_OK) {
                    status = BSP_ERR_HW_FAULT;
                    break;
                }
            }
            if (status == BSP_OK) {
                for (uint32_t i = 0U; i < s_drv8870_cfg_count; i++) {
                    if (hal_timer_get_capture_value(
                            s_drv8870_pwm_timer,
                            s_drv8870_cfg[i].pwm_ch) != compare) {
                        status = BSP_ERR_HW_FAULT;
                        break;
                    }
                }
            }
        }
    }

    if (status == BSP_ERR_HW_FAULT) {
        (void)bsp_drv8870_hw_scope_stop();
    }
    return status;
#endif
}

bsp_status_t bsp_drv8870_hw_scope_get_compare(bsp_drv8870_id_t motor,
                                               uint32_t *compare)
{
#if (PRJ_DRV8870_FACTORY_TEST_ENABLE == 0U)
    (void)motor;
    if (compare != NULL) {
        *compare = 0U;
    }
    return BSP_ERR_UNSUPPORTED;
#else
    if (compare == NULL) {
        return BSP_ERR_INVALID_PARAM;
    }
    *compare = 0U;
    if (!s_drv8870_inited) {
        return BSP_ERR_NOT_INIT;
    }
    if ((uint32_t)motor >= s_drv8870_cfg_count) {
        return BSP_ERR_INVALID_PARAM;
    }

    OSAL_CRITICAL_SECTION {
        *compare = hal_timer_get_capture_value(
            s_drv8870_pwm_timer, s_drv8870_cfg[motor].pwm_ch);
    }
    return BSP_OK;
#endif
}

bool bsp_drv8870_hw_scope_is_active(void)
{
#if (PRJ_DRV8870_FACTORY_TEST_ENABLE == 0U)
    return false;
#else
    bool active;

    OSAL_CRITICAL_SECTION {
        active = s_drv8870_hw_test_active;
    }
    return active;
#endif
}

bsp_status_t bsp_drv8870_hw_scope_stop(void)
{
#if (PRJ_DRV8870_FACTORY_TEST_ENABLE == 0U)
    return BSP_ERR_UNSUPPORTED;
#else
    if (!s_drv8870_inited) {
        drv8870_leave_factory_test();
        return BSP_ERR_NOT_INIT;
    }

    /* Explicit safe exit: neutral all channels before removing VM. */
    bsp_drv8870_power_disable();
    drv8870_leave_factory_test();
    return BSP_OK;
#endif
}
