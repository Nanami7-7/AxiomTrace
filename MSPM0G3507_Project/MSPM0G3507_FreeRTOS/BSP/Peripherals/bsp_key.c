#include "bsp_key.h"
#include "ti_msp_dl_config.h"

/*
 * 板级适配层职责：
 * - key.c：通用消抖和事件状态机；
 * - bsp_key.c：把 HAL GPIO 端口/引脚接入 key_t；
 * - app_key_events.c：决定事件对应的业务动作。
 */
#ifndef KEY_key_IOMUX
#define KEY_key_IOMUX   (IOMUX_PINCM14)
#endif
#ifndef KEY_switch_IOMUX
#define KEY_switch_IOMUX (IOMUX_PINCM16)
#endif

/**
 * @brief 初始化 PA7/PB3 为带上拉的数字输入。
 * @details 若 SysConfig 已生成相同配置，重复初始化仍是安全的幂等操作。
 */
static void bsp_key_hw_init(void)
{
    /* 兼容尚未重新生成 PA7/PB3 配置的 ti_msp_dl_config.h。 */
    DL_GPIO_initDigitalInputFeatures(KEY_key_IOMUX,
        DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_PULL_UP,
        DL_GPIO_HYSTERESIS_DISABLE, DL_GPIO_WAKEUP_DISABLE);
    DL_GPIO_initDigitalInputFeatures(KEY_switch_IOMUX,
        DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_PULL_UP,
        DL_GPIO_HYSTERESIS_DISABLE, DL_GPIO_WAKEUP_DISABLE);
}

/** key.c 通过此回调读取实际 GPIO 电平。 */
static bool bsp_key_read_level(void *user_data)
{
    const bsp_key_gpio_context_t *gpio =
        (const bsp_key_gpio_context_t *)user_data;
    return (gpio != NULL) ? hal_gpio_read_pin(gpio->port, gpio->pin) : false;
}

/** 记录扫描错误，便于调试适配层初始化和运行状态。 */
static void bsp_key_manager_record_error(bsp_key_manager_t *manager,
                                         key_status_t status)
{
    if (manager == NULL || status == KEY_STATUS_OK ||
        status == KEY_STATUS_DISABLED) {
        return;
    }
    if (manager->error_count != UINT32_MAX) {
        ++manager->error_count;
    }
    manager->last_error = status;
}

key_status_t bsp_key_manager_init(bsp_key_manager_t *manager,
                                  bsp_key_instance_t *instances,
                                  const bsp_key_config_t *configs,
                                  size_t count,
                                  uint32_t now_ms)
{
    size_t i;

    if (manager == NULL || instances == NULL || configs == NULL || count == 0U) {
        return KEY_STATUS_INVALID_PARAM;
    }

    bsp_key_hw_init();
    manager->instances = instances;
    manager->count = count;
    manager->initialized = false;
    manager->error_count = 0U;
    manager->last_error = KEY_STATUS_OK;

    for (i = 0U; i < count; ++i) {
        key_config_t config = configs[i].key;
        instances[i].gpio.port = configs[i].gpio_port;
        instances[i].gpio.pin = configs[i].gpio_pin;
        config.port = (uint32_t)configs[i].gpio_port;
        config.pin = configs[i].gpio_pin;
        config.read_level = bsp_key_read_level;
        config.read_user_data = &instances[i].gpio;
        if (key_init(&instances[i].key, &config, now_ms) != KEY_STATUS_OK) {
            bsp_key_manager_record_error(manager, KEY_STATUS_INVALID_PARAM);
            return KEY_STATUS_INVALID_PARAM;
        }
    }

    manager->initialized = true;
    return KEY_STATUS_OK;
}

key_status_t bsp_key_manager_poll(bsp_key_manager_t *manager, uint32_t now_ms)
{
    size_t i;
    key_status_t first_error = KEY_STATUS_OK;

    if (manager == NULL || !manager->initialized) {
        return KEY_STATUS_NOT_INITIALIZED;
    }

    for (i = 0U; i < manager->count; ++i) {
        key_status_t status = key_poll(&manager->instances[i].key, now_ms);
        if (status != KEY_STATUS_OK && status != KEY_STATUS_DISABLED) {
            bsp_key_manager_record_error(manager, status);
            if (first_error == KEY_STATUS_OK) {
                first_error = status;
            }
        }
    }
    return first_error;
}

key_t *bsp_key_manager_get(bsp_key_manager_t *manager, size_t index)
{
    if (manager == NULL || !manager->initialized || index >= manager->count) {
        return NULL;
    }
    return &manager->instances[index].key;
}
