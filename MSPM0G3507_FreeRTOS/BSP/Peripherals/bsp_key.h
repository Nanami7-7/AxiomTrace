/**
 * @file    bsp_key.h
 * @brief   MSPM0 GPIO 按键库适配层接口。
 * @details 将通用 key 状态机与 hal_gpio 连接，但不在本模块实现按键判定。
 */
#ifndef BSP_KEY_H
#define BSP_KEY_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>
#include "hal_gpio.h"
#include "key.h"

/** @brief 板级按键配置。 */
typedef struct {
    key_config_t key;                   /**< 通用按键状态机配置。 */
    hal_gpio_port_t gpio_port;          /**< MSPM0 GPIO 端口。 */
    uint32_t gpio_pin;                  /**< MSPM0 GPIO 引脚掩码。 */
} bsp_key_config_t;

/** @brief 板级 GPIO 读取上下文。 */
typedef struct {
    hal_gpio_port_t port;               /**< GPIO 端口。 */
    uint32_t pin;                       /**< GPIO 引脚掩码。 */
} bsp_key_gpio_context_t;

/** @brief 一个板级按键实例及其 GPIO 上下文。 */
typedef struct {
    key_t key;                          /**< 通用按键状态机对象。 */
    bsp_key_gpio_context_t gpio;        /**< MSPM0 GPIO 读取上下文。 */
} bsp_key_instance_t;

/** @brief 多个板级按键实例的管理对象。 */
typedef struct {
    bsp_key_instance_t *instances;      /**< 调用者提供的按键实例数组。 */
    size_t count;                       /**< 实例数量。 */
    bool initialized;                   /**< 适配层是否完成初始化。 */
    uint32_t error_count;               /**< 扫描异常累计次数。 */
    key_status_t last_error;             /**< 最近一次扫描异常码。 */
} bsp_key_manager_t;

/**
 * @brief 初始化一个 MSPM0 按键管理器。
 * @param manager 管理器对象。
 * @param instances 调用者提供的实例数组。
 * @param configs 板级按键配置数组。
 * @param count 按键数量。
 * @param now_ms 当前毫秒时间戳。
 * @return KEY_STATUS_OK 表示成功，否则返回错误码。
 */
key_status_t bsp_key_manager_init(bsp_key_manager_t *manager,
                                  bsp_key_instance_t *instances,
                                  const bsp_key_config_t *configs,
                                  size_t count,
                                  uint32_t now_ms);

/**
 * @brief 扫描全部 MSPM0 按键。
 * @param manager 已初始化的按键管理器。
 * @param now_ms 当前毫秒时间戳。
 * @return KEY_STATUS_OK 表示成功；若某个实例异常，返回本轮扫描发现的第一个错误码。
 */
key_status_t bsp_key_manager_poll(bsp_key_manager_t *manager,
                                  uint32_t now_ms);

/**
 * @brief 获取管理器中的指定按键。
 * @param manager 已初始化的按键管理器。
 * @param index 按键数组索引。
 * @return 按键对象指针；参数非法时返回 NULL。
 */
key_t *bsp_key_manager_get(bsp_key_manager_t *manager, size_t index);

#ifdef __cplusplus
}
#endif

#endif /* BSP_KEY_H */



