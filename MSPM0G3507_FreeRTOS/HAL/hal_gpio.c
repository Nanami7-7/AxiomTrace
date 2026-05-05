/**
 * @file    hal_gpio.c
 * @brief   HAL GPIO硬件抽象实现
 * @note    封装DL_GPIO_xxx调用，通过hal_gpio_port_t枚举映射到
 *          GPIO_Regs*指针，上层代码不直接引用SDK头文件
 */
#include "hal_gpio.h"
#include "ti_msp_dl_config.h"

/* ======================== 私有映射表 ======================== */

/**
 * @brief GPIO端口寄存器指针映射表
 * @note  通过hal_gpio_port_t索引获取对应的GPIO_Regs*
 *        顺序必须与hal_gpio_port_t枚举一致
 */
static GPIO_Regs *const s_gpio_port_map[HAL_GPIO_PORT_COUNT] = {
    GPIOA,  /**< HAL_GPIO_PORT_A -> GPIOA */
    GPIOB,  /**< HAL_GPIO_PORT_B -> GPIOB */
};

/* ======================== 内联辅助函数 ======================== */

/**
 * @brief  检查GPIO端口枚举是否合法
 * @param  port GPIO端口枚举值
 * @retval true  合法
 * @retval false 越界
 */
static inline bool is_port_valid(hal_gpio_port_t port)
{
    return ((uint32_t)port < HAL_GPIO_PORT_COUNT);
}

/**
 * @brief  将HAL端口枚举转换为SDK GPIO_Regs指针
 * @param  port HAL端口枚举值
 * @retval GPIO_Regs* 指针，越界返回NULL
 */
static inline GPIO_Regs *port_to_regs(hal_gpio_port_t port)
{
    if (!is_port_valid(port)) {
        return NULL;
    }
    return s_gpio_port_map[port];
}

/**
 * @brief  将HAL上拉枚举转换为SDK上拉配置
 * @param  pull HAL上拉枚举值
 * @retval DL_GPIO_RESISTOR_xxx 对应值
 */
static inline uint32_t pull_to_dl_resistor(hal_gpio_pull_t pull)
{
    switch (pull) {
    case HAL_GPIO_PULL_UP:
        return DL_GPIO_RESISTOR_PULL_UP;
    case HAL_GPIO_PULL_DOWN:
        return DL_GPIO_RESISTOR_PULL_DOWN;
    default:
        return DL_GPIO_RESISTOR_NONE;
    }
}

/* ======================== 公共函数实现 ======================== */

hal_status_t hal_gpio_init_output(const hal_gpio_pin_config_t *cfg)
{
    /* 参数校验 */
    if (cfg == NULL) {
        return HAL_ERR_INVALID_PARAM;
    }
    if (!is_port_valid(cfg->port)) {
        return HAL_ERR_INVALID_PARAM;
    }

    /*
     * 初始化GPIO为数字输出模式
     * 参数：IOMUX引脚号，默认低电平，无上下拉，低驱动强度，禁止高阻
     */
    DL_GPIO_initDigitalOutputFeatures(cfg->iomux,
        DL_GPIO_INVERSION_DISABLE,
        DL_GPIO_RESISTOR_PULL_DOWN,
        DL_GPIO_DRIVE_STRENGTH_LOW,
        DL_GPIO_HIZ_DISABLE);

    /* 设置初始输出低电平并使能输出 */
    DL_GPIO_clearPins(port_to_regs(cfg->port), cfg->pin);
    DL_GPIO_enableOutput(port_to_regs(cfg->port), cfg->pin);

    return HAL_OK;
}

hal_status_t hal_gpio_init_input(const hal_gpio_pin_config_t *cfg,
                                  hal_gpio_pull_t pull)
{
    /* 参数校验 */
    if (cfg == NULL) {
        return HAL_ERR_INVALID_PARAM;
    }
    if (!is_port_valid(cfg->port)) {
        return HAL_ERR_INVALID_PARAM;
    }

    /*
     * 初始化GPIO为数字输入模式
     * 参数：IOMUX引脚号，无反转，上下拉配置，禁止迟滞，禁止唤醒
     */
    DL_GPIO_initDigitalInputFeatures(cfg->iomux,
        DL_GPIO_INVERSION_DISABLE,
        pull_to_dl_resistor(pull),
        DL_GPIO_HYSTERESIS_DISABLE,
        DL_GPIO_WAKEUP_DISABLE);

    return HAL_OK;
}

hal_status_t hal_gpio_set_pin(hal_gpio_port_t port, uint32_t pin)
{
    GPIO_Regs *regs = port_to_regs(port);
    if (regs == NULL) {
        return HAL_ERR_INVALID_PARAM;
    }

    DL_GPIO_setPins(regs, pin);
    return HAL_OK;
}

hal_status_t hal_gpio_clear_pin(hal_gpio_port_t port, uint32_t pin)
{
    GPIO_Regs *regs = port_to_regs(port);
    if (regs == NULL) {
        return HAL_ERR_INVALID_PARAM;
    }

    DL_GPIO_clearPins(regs, pin);
    return HAL_OK;
}

hal_status_t hal_gpio_write_pin(hal_gpio_port_t port, uint32_t pin,
                                 bool high)
{
    GPIO_Regs *regs = port_to_regs(port);
    if (regs == NULL) {
        return HAL_ERR_INVALID_PARAM;
    }

    if (high) {
        DL_GPIO_setPins(regs, pin);
    } else {
        DL_GPIO_clearPins(regs, pin);
    }
    return HAL_OK;
}

bool hal_gpio_read_pin(hal_gpio_port_t port, uint32_t pin)
{
    GPIO_Regs *regs = port_to_regs(port);
    if (regs == NULL) {
        return false;
    }

    /* 读取引脚输入状态，返回true表示高电平 */
    return (DL_GPIO_readPins(regs, pin) != 0U);
}

hal_status_t hal_gpio_toggle_pin(hal_gpio_port_t port, uint32_t pin)
{
    GPIO_Regs *regs = port_to_regs(port);
    if (regs == NULL) {
        return HAL_ERR_INVALID_PARAM;
    }

    DL_GPIO_togglePins(regs, pin);
    return HAL_OK;
}

hal_status_t hal_gpio_set_direction(const hal_gpio_pin_config_t *cfg,
                                     hal_gpio_dir_t dir)
{
    /* 参数校验 */
    if (cfg == NULL) {
        return HAL_ERR_INVALID_PARAM;
    }
    if (!is_port_valid(cfg->port)) {
        return HAL_ERR_INVALID_PARAM;
    }

    /*
     * 动态切换GPIO方向，主要用于软件I2C的SDA引脚
     * 输出模式：使能输出驱动器
     * 输入模式：禁用输出驱动器(高阻输入)
     */
    if (dir == HAL_GPIO_DIR_OUTPUT) {
        DL_GPIO_enableOutput(port_to_regs(cfg->port), cfg->pin);
    } else {
        DL_GPIO_disableOutput(port_to_regs(cfg->port), cfg->pin);
    }

    return HAL_OK;
}
