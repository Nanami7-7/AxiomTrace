/**
 * @file    bsp_key.h
 * @brief   按键驱动和扫描任务。
 * @details 提供本模块的基础功能实现。
 */
#ifndef BSP_KEY_H
#define BSP_KEY_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "ti_msp_dl_config.h"
#include "key.h"

/**
 * @brief   按键驱动和扫描任务。
 * @note   使用时请遵循接口约束。
 */
typedef struct {
    key_config_t key;       /**
 * 按键驱动和扫描任务。
 */
    GPIO_Regs *gpio_port;   /**
 * 按键驱动和扫描任务。
 */
    uint32_t gpio_pin;      /**
 * 按键驱动和扫描任务。
 */
    uint32_t gpio_iomux;    /**
 * 按键驱动和扫描任务。
 */
} bsp_key_config_t;

/**
 * @brief   按键驱动和扫描任务。
 */
typedef struct {
    GPIO_Regs *port;        /**
 * 按键驱动和扫描任务。
 */
    uint32_t pin;           /**
 * 按键驱动和扫描任务。
 */
} bsp_key_gpio_context_t;

/**
 * @brief   按键驱动和扫描任务。
 */
typedef struct {
    key_t key;                       /**
 * 按键驱动和扫描任务。
 */
    bsp_key_gpio_context_t gpio;     /**
 * 按键驱动和扫描任务。
 */
} bsp_key_instance_t;

/**
 * @brief   按键驱动和扫描任务。
 * @note   使用时请遵循接口约束。
 */
typedef struct {
    bsp_key_instance_t *instances;   /**
 * 按键驱动和扫描任务。
 */
    size_t count;                    /**
 * 按键驱动和扫描任务。
 */
    bool initialized;                /**
 * 按键驱动和扫描任务。
 */
    uint32_t error_count;            /**
 * 按键驱动和扫描任务。
 */
    key_status_t last_error;         /**
 * 按键驱动和扫描任务。
 */
} bsp_key_manager_t;

/**
 * @brief   按键驱动和扫描任务。
 * @param  manager 参数说明。
 * @param  instances 参数说明。
 * @param  configs 参数说明。
 * @param  count 参数说明。
 * @param  now_ms 参数说明。
 * @return 返回处理结果。
 */
key_status_t bsp_key_manager_init(bsp_key_manager_t *manager,
                                  bsp_key_instance_t *instances,
                                  const bsp_key_config_t *configs,
                                  size_t count,
                                  uint32_t now_ms);

/**
 * @brief   按键驱动和扫描任务。
 * @param  manager 参数说明。
 * @param  now_ms 参数说明。
 * @return 返回处理结果。
 */
key_status_t bsp_key_manager_poll(bsp_key_manager_t *manager,
                                  uint32_t now_ms);

/**
 * @brief   按键驱动和扫描任务。
 * @param  manager 参数说明。
 * @param  index 参数说明。
 * @return 返回处理结果。
 */
key_t *bsp_key_manager_get(bsp_key_manager_t *manager, size_t index);

#ifdef __cplusplus
}
#endif

#endif /* BSP_KEY_H */
