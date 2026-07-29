/**
 * @file    bsp_key.c
 * @brief   按键驱动和扫描任务。
 * @warning 不要在此处执行长时间阻塞操作。
 */
#include "bsp_key.h"

/**
 * @brief   按键驱动和扫描任务。
 */
static void bsp_key_hw_init(const bsp_key_config_t *configs, size_t count)
{
    size_t i;

    for (i = 0U; i < count; ++i) {
        DL_GPIO_initDigitalInputFeatures(
            configs[i].gpio_iomux,
            DL_GPIO_INVERSION_DISABLE,
            DL_GPIO_RESISTOR_PULL_UP,
            DL_GPIO_HYSTERESIS_DISABLE,
            DL_GPIO_WAKEUP_DISABLE);
    }
}

/**
 * @brief   按键驱动和扫描任务。
 * @param  user_data 参数说明。
 * @return 返回处理结果。
 */
static bool bsp_key_read_level(void *user_data)
{
    const bsp_key_gpio_context_t *gpio =
        (const bsp_key_gpio_context_t *)user_data;

    if (gpio == NULL || gpio->port == NULL) {
        return false;
    }

    return (DL_GPIO_readPins(gpio->port, gpio->pin) != 0U);
}

/**
 * @brief   按键驱动和扫描任务。
 */
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

/**
 * @brief 初始化函数 bsp_key_manager_init，完成对应模块的功能处理。
 * @param manager 函数参数 manager。
 * @param instances 函数参数 instances。
 * @param configs 函数参数 configs。
 * @param count 函数参数 count。
 * @param now_ms 函数参数 now_ms。
 * @return 函数执行结果。
 */
key_status_t bsp_key_manager_init(bsp_key_manager_t *manager,
                                  bsp_key_instance_t *instances,
                                  const bsp_key_config_t *configs,
                                  size_t count,
                                  uint32_t now_ms)
{
    size_t i;

    if (manager == NULL || instances == NULL ||
        configs == NULL || count == 0U) {
        return KEY_STATUS_INVALID_PARAM;
    }

    /* 按键驱动和扫描任务。 */
    bsp_key_hw_init(configs, count);

    manager->instances = instances;
    manager->count = count;
    manager->initialized = false;
    manager->error_count = 0U;
    manager->last_error = KEY_STATUS_OK;

    for (i = 0U; i < count; ++i) {
        key_config_t config = configs[i].key;

        /* 按键驱动和扫描任务。 */
        instances[i].gpio.port = configs[i].gpio_port;
        instances[i].gpio.pin = configs[i].gpio_pin;

        /* 按键驱动和扫描任务。 */
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

/**
 * @brief 轮询函数 bsp_key_manager_poll，完成对应模块的功能处理。
 * @param manager 函数参数 manager。
 * @param now_ms 函数参数 now_ms。
 * @return 函数执行结果。
 */
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

/**
 * @brief 获取函数 bsp_key_manager_get，完成对应模块的功能处理。
 * @param manager 函数参数 manager。
 * @param index 函数参数 index。
 * @return 函数执行结果。
 */
key_t *bsp_key_manager_get(bsp_key_manager_t *manager, size_t index)
{
    if (manager == NULL || !manager->initialized ||
        index >= manager->count) {
        return NULL;
    }

    return &manager->instances[index].key;
}
