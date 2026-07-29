/**
 * @file    key.c
 * @brief   通用按键与保持型开关状态机实现。
 */
#include "key.h"

/*
 * 通用按键状态机，不依赖 MSPM0、FreeRTOS 或具体 GPIO。
 * key_poll() 是唯一的推进入口；GPIO 通过 read_level 回调读取，事件通过
 * callback 回调交给应用层。
 */
/** @brief 计算两个无符号毫秒时间戳之间的经过时间，支持 32 位回绕。 */
static uint32_t key_elapsed_ms(uint32_t now_ms, uint32_t start_ms)
{
    return now_ms - start_ms;
}

/** @brief 判断事件是否被配置掩码允许上报。 */
static bool key_event_enabled(const key_t *key, key_event_type_t event)
{
    if (key == NULL || (uint32_t)event >= 32U) {
        return false;
    }
    return (key->event_mask & (1UL << (uint32_t)event)) != 0U;
}

/** @brief 根据 GPIO 电平转换为逻辑按下状态。 */
/** 把 GPIO 电平转换成逻辑状态，考虑高/低有效配置。 */
static key_state_t key_state_from_level(const key_t *key, bool level)
{
    return ((level ? KEY_LEVEL_HIGH : KEY_LEVEL_LOW) == key->active_level)
        ? KEY_STATE_PRESSED : KEY_STATE_RELEASED;
}

/** @brief 向应用派发一个事件，回调为空时安全忽略。 */
static void key_emit(const key_t *key,
                     key_event_type_t event,
                     uint32_t now_ms,
                     uint32_t duration_ms)
{
    if (!key_event_enabled(key, event) || key->callback == NULL) {
        return;
    }
    key->callback(key, event, now_ms, duration_ms, key->callback_user_data);
}

/** @brief 检查按键配置是否合法。 */
static bool key_config_valid(const key_config_t *config)
{
    if (config == NULL || config->read_level == NULL) {
        return false;
    }
    if (config->type != KEY_TYPE_BUTTON && config->type != KEY_TYPE_SWITCH) {
        return false;
    }
    if (config->active_level != KEY_LEVEL_LOW &&
        config->active_level != KEY_LEVEL_HIGH) {
        return false;
    }
    if (config->type == KEY_TYPE_BUTTON && config->long_press_ms == 0U) {
        return false;
    }
    return true;
}

/** @brief 初始化运行时字段，并同步当前 GPIO 电平。 */
static key_status_t key_sync_initial_state(key_t *key, uint32_t now_ms)
{
    bool level;

    if (key == NULL || key->read_level == NULL) {
        return KEY_STATUS_INVALID_PARAM;
    }
    level = key->read_level(key->read_user_data);
    key->current_gpio_level = level;
    key->last_sample_gpio_level = level;
    key->stable_gpio_level = level;
    key->stable_state = key_state_from_level(key, level);
    key->raw_change_timestamp_ms = now_ms;
    key->press_timestamp_ms = now_ms;
    key->long_press_reported = false;
    key->stuck_reported = false;

    /*
     * BUTTON 上电时不制造虚假的按下/长按/卡键事件。若设备启动时按键
     * 已经处于有效电平，则只同步状态，直到释放后再次按下才开始完整计时。
     * 这样可以避免复位、掉电恢复或下载程序时误触发业务回调。
     */
    if (key->type == KEY_TYPE_BUTTON &&
        key->stable_state == KEY_STATE_PRESSED) {
        key->long_press_reported = true;
        key->stuck_reported = true;
    }

    key->enabled = true;
    key->initialized = true;

    /* SWITCH 可按配置报告当前状态。 */
    if (key->type == KEY_TYPE_SWITCH && key->report_initial_state) {
        key_emit(key,
                 key->stable_state == KEY_STATE_PRESSED
                     ? KEY_EVENT_SWITCH_ON : KEY_EVENT_SWITCH_OFF,
                 now_ms,
                 0U);
    }
    return KEY_STATUS_OK;
}

/**
 * @brief 初始化单个按键对象。
 * @param key 按键对象指针。
 * @param config 按键配置指针。
 * @param now_ms 当前单调递增毫秒时间戳。
 * @return KEY_STATUS_OK 表示成功，否则返回错误码。
 */
key_status_t key_init(key_t *key, const key_config_t *config, uint32_t now_ms)
{
    if (key == NULL || !key_config_valid(config)) {
        return KEY_STATUS_INVALID_PARAM;
    }

    key->id = config->id;
    key->type = config->type;
    key->port = config->port;
    key->pin = config->pin;
    key->active_level = config->active_level;
    key->debounce_ms = config->debounce_ms;
    key->long_press_ms = config->long_press_ms;
    key->max_hold_ms = config->max_hold_ms;
    key->event_mask = config->event_mask;
    key->report_initial_state = config->report_initial_state;
    key->read_level = config->read_level;
    key->read_user_data = config->read_user_data;
    key->callback = config->callback;
    key->callback_user_data = config->callback_user_data;

    return key_sync_initial_state(key, now_ms);
}

/**
 * @brief 重置按键运行时状态并重新同步当前 GPIO 电平。
 * @param key 已初始化的按键对象。
 * @param now_ms 当前毫秒时间戳。
 * @return KEY_STATUS_OK 表示成功，否则返回错误码。
 */
key_status_t key_reset(key_t *key, uint32_t now_ms)
{
    if (key == NULL || !key->initialized) {
        return KEY_STATUS_NOT_INITIALIZED;
    }
    return key_sync_initial_state(key, now_ms);
}

/** @brief 处理消抖确认后的稳定状态变化。 */
/** 消抖完成后提交稳定状态，并产生按下/释放/开关事件。 */
static void key_commit_state(key_t *key, key_state_t new_state, uint32_t now_ms)
{
    key_state_t old_state = key->stable_state;
    uint32_t duration_ms = 0U;

    if (old_state == new_state) {
        return;
    }

    key->stable_state = new_state;
    key->stable_gpio_level =
        (new_state == KEY_STATE_PRESSED)
            ? (key->active_level == KEY_LEVEL_HIGH)
            : (key->active_level == KEY_LEVEL_LOW);

    if (new_state == KEY_STATE_PRESSED) {
        key->press_timestamp_ms = now_ms;
        key->long_press_reported = false;
        key->stuck_reported = false;
        if (key->type == KEY_TYPE_BUTTON) {
            key_emit(key, KEY_EVENT_PRESS, now_ms, 0U);
        } else {
            key_emit(key, KEY_EVENT_SWITCH_ON, now_ms, 0U);
        }
        return;
    }

    duration_ms = key_elapsed_ms(now_ms, key->press_timestamp_ms);
    if (key->type == KEY_TYPE_BUTTON) {
        if (!key->long_press_reported) {
            key_emit(key, KEY_EVENT_SHORT_PRESS, now_ms, duration_ms);
        }
        key_emit(key, KEY_EVENT_RELEASE, now_ms, duration_ms);
    } else {
        key_emit(key, KEY_EVENT_SWITCH_OFF, now_ms, duration_ms);
    }
    key->long_press_reported = false;
    key->stuck_reported = false;
}

/** @brief 处理按住期间的长按和卡键事件。 */
/** 在保持按下期间检查长按和卡键阈值，每次只上报一次。 */
static void key_process_hold(key_t *key, uint32_t now_ms)
{
    uint32_t duration_ms;

    if (key->stable_state != KEY_STATE_PRESSED) {
        return;
    }
    duration_ms = key_elapsed_ms(now_ms, key->press_timestamp_ms);

    if (key->type == KEY_TYPE_BUTTON &&
        !key->long_press_reported &&
        duration_ms >= key->long_press_ms) {
        key->long_press_reported = true;
        key_emit(key, KEY_EVENT_LONG_PRESS, now_ms, duration_ms);
    }

    if (key->max_hold_ms != 0U &&
        !key->stuck_reported &&
        duration_ms >= key->max_hold_ms) {
        key->stuck_reported = true;
        key_emit(key, KEY_EVENT_STUCK, now_ms, duration_ms);
    }
}

/**
 * @brief 扫描并推进单个按键状态机。
 * @param key 已初始化的按键对象。
 * @param now_ms 当前单调递增毫秒时间戳。
 * @return KEY_STATUS_OK 表示成功，否则返回错误码。
 */
/** 读取一次 GPIO，推进消抖和保持计时。 */
key_status_t key_poll(key_t *key, uint32_t now_ms)
{
    bool raw_level;
    key_state_t candidate_state;

    if (key == NULL || !key->initialized) {
        return KEY_STATUS_NOT_INITIALIZED;
    }
    if (!key->enabled) {
        return KEY_STATUS_DISABLED;
    }

    raw_level = key->read_level(key->read_user_data);
    key->current_gpio_level = raw_level;

    if (raw_level != key->last_sample_gpio_level) {
        key->raw_change_timestamp_ms = now_ms;
        key->last_sample_gpio_level = raw_level;
    }

    /* 使用独立候选电平计时，避免原始电平短暂抖动直接改变稳定状态。 */
    if (raw_level != key->stable_gpio_level) {
        candidate_state = key_state_from_level(key, raw_level);
        if (key_elapsed_ms(now_ms, key->raw_change_timestamp_ms) >=
            key->debounce_ms) {
            key->stable_gpio_level = raw_level;
            key_commit_state(key, candidate_state, now_ms);
        }
    } else {
        key->raw_change_timestamp_ms = now_ms;
    }

    key_process_hold(key, now_ms);
    return KEY_STATUS_OK;
}

/**
 * @brief 启用按键扫描。
 * @param key 已初始化的按键对象。
 * @return KEY_STATUS_OK 表示成功，否则返回错误码。
 */
key_status_t key_enable(key_t *key)
{
    if (key == NULL || !key->initialized) {
        return KEY_STATUS_NOT_INITIALIZED;
    }
    key->enabled = true;
    return KEY_STATUS_OK;
}

/**
 * @brief 禁用按键扫描并清理未完成的按下计时。
 * @param key 已初始化的按键对象。
 * @return KEY_STATUS_OK 表示成功，否则返回错误码。
 */
key_status_t key_disable(key_t *key)
{
    if (key == NULL || !key->initialized) {
        return KEY_STATUS_NOT_INITIALIZED;
    }
    key->enabled = false;
    key->long_press_reported = false;
    key->stuck_reported = false;
    return KEY_STATUS_OK;
}

/**
 * @brief 查询按键当前稳定逻辑状态。
 * @param key 已初始化的按键对象。
 * @return KEY_STATE_PRESSED 表示按下，KEY_STATE_RELEASED 表示释放；无效对象返回释放。
 */
key_state_t key_get_state(const key_t *key)
{
    if (key == NULL || !key->initialized) {
        return KEY_STATE_RELEASED;
    }
    return key->stable_state;
}

/**
 * @brief 查询按键最近一次采样到的原始 GPIO 电平。
 * @param key 已初始化的按键对象。
 * @return true 表示高电平，false 表示低电平；无效对象返回 false。
 */
bool key_get_raw_level(const key_t *key)
{
    if (key == NULL || !key->initialized) {
        return false;
    }
    return key->current_gpio_level;
}

/**
 * @brief 初始化多按键管理器。
 * @param manager 管理器对象。
 * @param keys 调用者分配的按键数组。
 * @param count 按键数量。
 * @param now_ms 当前毫秒时间戳。
 * @return KEY_STATUS_OK 表示成功，否则返回错误码。
 */
key_status_t key_manager_init(key_manager_t *manager,
                              key_t *keys,
                              size_t count,
                              uint32_t now_ms)
{
    size_t i;

    if (manager == NULL || keys == NULL || count == 0U) {
        return KEY_STATUS_INVALID_PARAM;
    }
    manager->keys = keys;
    manager->count = count;
    manager->initialized = false;
    for (i = 0U; i < count; ++i) {
        if (!keys[i].initialized) {
            return KEY_STATUS_INVALID_PARAM;
        }
        (void)now_ms;
    }
    manager->initialized = true;
    return KEY_STATUS_OK;
}

/**
 * @brief 扫描管理器中的全部按键。
 * @param manager 已初始化的管理器。
 * @param now_ms 当前毫秒时间戳。
 * @return KEY_STATUS_OK 表示全部扫描成功，否则返回第一个错误码。
 */
/** 依次扫描管理器中的全部按键。 */
key_status_t key_manager_poll(key_manager_t *manager, uint32_t now_ms)
{
    size_t i;
    key_status_t status;

    if (manager == NULL || !manager->initialized) {
        return KEY_STATUS_NOT_INITIALIZED;
    }
    for (i = 0U; i < manager->count; ++i) {
        status = key_poll(&manager->keys[i], now_ms);
        if (status != KEY_STATUS_OK && status != KEY_STATUS_DISABLED) {
            return status;
        }
    }
    return KEY_STATUS_OK;
}
