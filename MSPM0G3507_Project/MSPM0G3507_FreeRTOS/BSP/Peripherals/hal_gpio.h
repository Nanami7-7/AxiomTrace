/**
 * @file    hal_gpio.h
 * @brief   HAL GPIO硬件抽象接口
 *
 * @note    封装DL_GPIO_xxx调用，提供统一的GPIO操作接口
 *          SYSCFG_DL_init()已配置的引脚无需再调init，直接使用
 *          read/write/toggle；未配置的引脚(如软件I2C)需先调init
 */
#ifndef HAL_GPIO_H
#define HAL_GPIO_H

#ifdef __cplusplus
extern "C" {
#endif

/* ======================== 包含 ======================== */
#include "hal_common.h"

/* ======================== 函数接口 ======================== */

/**
 * @brief  初始化GPIO引脚为数字输出模式
 * @note   仅用于SysConfig未配置的引脚(如软件I2C的SCL/SDA)
 *         SysConfig已配置的引脚无需调用此函数
 * @param  cfg  引脚配置(端口/引脚/IOMUX)
 * @retval HAL_OK           初始化成功
 * @retval HAL_ERR_INVALID_PARAM 参数为空
 */
hal_status_t hal_gpio_init_output(const hal_gpio_pin_config_t *cfg);

/**
 * @brief  初始化GPIO引脚为数字输入模式
 * @note   仅用于SysConfig未配置的引脚
 * @param  cfg   引脚配置(端口/引脚/IOMUX)
 * @param  pull  上下拉配置
 * @retval HAL_OK           初始化成功
 * @retval HAL_ERR_INVALID_PARAM 参数为空
 */
hal_status_t hal_gpio_init_input(const hal_gpio_pin_config_t *cfg,
                                  hal_gpio_pull_t pull);

/**
 * @brief  设置GPIO引脚输出高电平
 * @param  port GPIO端口
 * @param  pin  引脚编号
 * @retval HAL_OK           设置成功
 * @retval HAL_ERR_INVALID_PARAM 端口越界
 */
hal_status_t hal_gpio_set_pin(hal_gpio_port_t port, uint32_t pin);

/**
 * @brief  设置GPIO引脚输出低电平
 * @param  port GPIO端口
 * @param  pin  引脚编号
 * @retval HAL_OK           设置成功
 * @retval HAL_ERR_INVALID_PARAM 端口越界
 */
hal_status_t hal_gpio_clear_pin(hal_gpio_port_t port, uint32_t pin);

/**
 * @brief  写GPIO引脚电平
 * @param  port GPIO端口
 * @param  pin  引脚编号
 * @param  high true=高电平, false=低电平
 * @retval HAL_OK           写入成功
 * @retval HAL_ERR_INVALID_PARAM 端口越界
 */
hal_status_t hal_gpio_write_pin(hal_gpio_port_t port, uint32_t pin,
                                 bool high);

/**
 * @brief  读取GPIO引脚输入电平
 * @param  port GPIO端口
 * @param  pin  引脚编号
 * @retval true  引脚为高电平
 * @retval false 引脚为低电平
 */
bool hal_gpio_read_pin(hal_gpio_port_t port, uint32_t pin);

/**
 * @brief  读取GPIO输出锁存器状态
 * @note   读取DOUT而不是DIN；只表示MCU已命令的输出状态，不能代替
 *         对PCB引脚、电源开关或VIN_OUT的物理电压测量。
 * @param  port GPIO端口
 * @param  pin  引脚编号
 * @retval true  输出锁存器为高
 * @retval false 输出锁存器为低，或端口参数无效
 */
bool hal_gpio_read_output_latch(hal_gpio_port_t port, uint32_t pin);

/**
 * @brief  翻转GPIO引脚电平
 * @param  port GPIO端口
 * @param  pin  引脚编号
 * @retval HAL_OK           翻转成功
 * @retval HAL_ERR_INVALID_PARAM 端口越界
 */
hal_status_t hal_gpio_toggle_pin(hal_gpio_port_t port, uint32_t pin);

/**
 * @brief  设置GPIO引脚方向
 * @note   用于软件I2C的SDA方向动态切换
 * @param  cfg 引脚配置
 * @param  dir 引脚方向(输入/输出)
 * @retval HAL_OK           设置成功
 * @retval HAL_ERR_INVALID_PARAM 参数无效
 */
hal_status_t hal_gpio_set_direction(const hal_gpio_pin_config_t *cfg,
                                     hal_gpio_dir_t dir);

#ifdef __cplusplus
}
#endif

#endif /* HAL_GPIO_H */
