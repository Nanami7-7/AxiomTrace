/**
 * @file    key.h
 * @brief   通用按键与保持型开关状态机接口。
 * @details 本模块只负责按键状态判定，不依赖 FreeRTOS、具体 MCU 或 GPIO 驱动。
 *          GPIO 电平通过回调函数注入，便于移植和主机侧单元测试。
 *
 * 状态机规则：原始电平连续保持 debounce_ms 后才更新稳定状态；BUTTON
 * 释放时产生 SHORT_PRESS，按住达到 long_press_ms 时产生一次 LONG_PRESS；
 * 按住达到 max_hold_ms 时产生一次 STUCK，用于处理卡键安全逻辑。
 * 应用应以稳定、单调递增的毫秒时间戳周期调用 key_poll()，推荐周期 5～10 ms。
 */
#ifndef KEY_H
#define KEY_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/** @brief 按键库返回状态码。 */
typedef enum {
    KEY_STATUS_OK = 0,                  /**< 操作成功。 */
    KEY_STATUS_INVALID_PARAM = -1,      /**< 参数为空或参数值非法。 */
    KEY_STATUS_NOT_INITIALIZED = -2,    /**< 对象尚未初始化。 */
    KEY_STATUS_DISABLED = -3            /**< 对象当前处于禁用状态。 */
} key_status_t;

/** @brief 按键工作类型。 */
typedef enum {
    KEY_TYPE_BUTTON = 0,                /**< 瞬时按键，支持短按和长按。 */
    KEY_TYPE_SWITCH = 1                 /**< 保持型开关，只报告稳定开关状态。 */
} key_type_t;

/** @brief GPIO 有效电平。 */
typedef enum {
    KEY_LEVEL_LOW = 0,                  /**< 低电平表示有效。 */
    KEY_LEVEL_HIGH = 1                  /**< 高电平表示有效。 */
} key_level_t;

/** @brief 按键稳定逻辑状态。 */
typedef enum {
    KEY_STATE_RELEASED = 0,             /**< 按键处于释放状态。 */
    KEY_STATE_PRESSED = 1               /**< 按键处于按下状态。 */
} key_state_t;

/** @brief 按键事件类型。 */
typedef enum {
    KEY_EVENT_PRESS = 0,                /**< 消抖确认按下。 */
    KEY_EVENT_RELEASE,                  /**< 消抖确认释放。 */
    KEY_EVENT_SHORT_PRESS,              /**< 释放前未达到长按阈值。 */
    KEY_EVENT_LONG_PRESS,               /**< 达到长按阈值，仅触发一次。 */
    KEY_EVENT_SWITCH_ON,                /**< 保持型开关稳定进入有效状态。 */
    KEY_EVENT_SWITCH_OFF,               /**< 保持型开关稳定退出有效状态。 */
    KEY_EVENT_STUCK                     /**< 可选的卡键/超长按告警。 */
} key_event_type_t;

/** @brief 按键事件掩码。 */
typedef enum {
    KEY_EVENT_MASK_NONE = 0U,                           /**< 不上报任何事件。 */
    KEY_EVENT_MASK_PRESS = (1UL << KEY_EVENT_PRESS),    /**< 上报按下事件。 */
    KEY_EVENT_MASK_RELEASE = (1UL << KEY_EVENT_RELEASE),/**< 上报释放事件。 */
    KEY_EVENT_MASK_SHORT = (1UL << KEY_EVENT_SHORT_PRESS), /**< 上报短按事件。 */
    KEY_EVENT_MASK_LONG = (1UL << KEY_EVENT_LONG_PRESS),  /**< 上报长按事件。 */
    KEY_EVENT_MASK_SWITCH = (1UL << KEY_EVENT_SWITCH_ON) |
                            (1UL << KEY_EVENT_SWITCH_OFF), /**< 上报开关事件。 */
    KEY_EVENT_MASK_STUCK = (1UL << KEY_EVENT_STUCK)     /**< 上报卡键事件。 */
} key_event_mask_t;

/** @brief 上报所有已定义事件的掩码。 */
#define KEY_EVENT_MASK_ALL ((uint32_t)0xFFFFFFFFUL)

/** @brief GPIO 原始电平读取函数类型。 */
typedef bool (*key_read_level_fn)(void *user_data);

/** @brief 按键对象的前置声明。 */
typedef struct key_s key_t;

/** @brief 按键事件回调函数类型。 */
typedef void (*key_event_callback_t)(const key_t *key,
                                     key_event_type_t event,
                                     uint32_t timestamp_ms,
                                     uint32_t pressed_duration_ms,
                                     void *user_data);

/**
 * @brief 按键配置参数。
 * @note port 和 pin 仅保存板级标识，具体 GPIO 访问由 read_level 完成。
 */
typedef struct {
    uint32_t id;                        /**< 应用层按键唯一标识。 */
    key_type_t type;                    /**< 按键类型。 */
    uint32_t port;                      /**< GPIO 端口抽象值。 */
    uint32_t pin;                       /**< GPIO 引脚掩码或引脚编号。 */
    key_level_t active_level;           /**< 按下/有效时对应的 GPIO 电平。 */
    uint32_t debounce_ms;               /**< 消抖时间，建议 10～30 ms。 */
    uint32_t long_press_ms;             /**< 长按阈值，BUTTON 类型使用。 */
    uint32_t max_hold_ms;               /**< 最大按住时间，0 表示关闭卡键检测。 */
    uint32_t event_mask;                /**< 需要上报的事件掩码。 */
    bool report_initial_state;           /**< SWITCH 是否在初始化时报告当前状态。 */
    key_read_level_fn read_level;       /**< GPIO 原始电平读取函数。 */
    void *read_user_data;               /**< GPIO 读取函数的用户上下文。 */
    key_event_callback_t callback;      /**< 统一事件回调函数。 */
    void *callback_user_data;           /**< 回调函数的用户上下文。 */
} key_config_t;

/**
 * @brief 按键运行时对象。
 * @note 该结构体由调用者分配，库不使用动态内存。应用可以读取其中的状态字段，
 *       但不得在扫描过程中修改运行时字段。
 */
struct key_s {
    /* 配置字段。 */
    uint32_t id;                        /**< 应用层按键唯一标识。 */
    key_type_t type;                    /**< 按键类型。 */
    uint32_t port;                      /**< GPIO 端口抽象值。 */
    uint32_t pin;                       /**< GPIO 引脚掩码或引脚编号。 */
    key_level_t active_level;           /**< 按下/有效时对应的 GPIO 电平。 */
    uint32_t debounce_ms;               /**< 消抖时间。 */
    uint32_t long_press_ms;             /**< 长按阈值。 */
    uint32_t max_hold_ms;               /**< 最大按住时间，0 表示关闭。 */
    uint32_t event_mask;                /**< 事件掩码。 */
    bool report_initial_state;           /**< 是否上报初始开关状态。 */
    key_read_level_fn read_level;       /**< GPIO 原始电平读取函数。 */
    void *read_user_data;               /**< GPIO 读取函数上下文。 */
    key_event_callback_t callback;      /**< 统一事件回调函数。 */
    void *callback_user_data;           /**< 回调函数上下文。 */

    /*
     * 以下字段用于调试和状态观察，应用层一般通过 key_get_state() 和回调
     * 获取信息，不应直接修改这些运行时字段。
     */
    bool current_gpio_level;            /**< 最近一次读取到的原始 GPIO 电平。 */
    bool last_sample_gpio_level;        /**< 上一次扫描采样到的原始 GPIO 电平。 */
    bool stable_gpio_level;             /**< 消抖确认后的稳定 GPIO 电平。 */
    key_state_t stable_state;           /**< 消抖确认后的逻辑状态。 */
    bool enabled;                       /**< 按键是否参与扫描。 */
    bool initialized;                   /**< 按键是否完成初始化。 */
    bool long_press_reported;           /**< 本次按下是否已经上报长按。 */
    bool stuck_reported;                /**< 本次按下是否已经上报卡键。 */
    uint32_t raw_change_timestamp_ms;   /**< 当前候选原始电平开始稳定的时间。 */
    uint32_t press_timestamp_ms;        /**< 最近一次确认按下的时间。 */
};

/**
 * @brief 多按键管理器。
 * @note keys 指向调用者持有的连续 key_t 数组，数组生命周期必须覆盖管理器使用期。
 */
typedef struct {
    key_t *keys;                        /**< 按键对象数组。 */
    size_t count;                       /**< 数组中的按键数量。 */
    bool initialized;                   /**< 管理器是否完成初始化。 */
} key_manager_t;

/**
 * @brief 初始化单个按键对象。
 * @param key 按键对象指针。
 * @param config 按键配置指针。
 * @param now_ms 当前单调递增毫秒时间戳。
 * @return KEY_STATUS_OK 表示成功，否则返回错误码。
 */
key_status_t key_init(key_t *key, const key_config_t *config, uint32_t now_ms);

/**
 * @brief 重置按键运行时状态并重新同步当前 GPIO 电平。
 * @param key 已初始化的按键对象。
 * @param now_ms 当前毫秒时间戳。
 * @return KEY_STATUS_OK 表示成功，否则返回错误码。
 */
key_status_t key_reset(key_t *key, uint32_t now_ms);

/**
 * @brief 扫描并推进单个按键状态机。
 * @param key 已初始化的按键对象。
 * @param now_ms 当前毫秒时间戳。
 * @return KEY_STATUS_OK 表示成功，否则返回错误码。
 */
key_status_t key_poll(key_t *key, uint32_t now_ms);

/**
 * @brief 启用按键扫描。
 * @param key 已初始化的按键对象。
 * @return KEY_STATUS_OK 表示成功，否则返回错误码。
 */
key_status_t key_enable(key_t *key);

/**
 * @brief 禁用按键扫描并清理未完成的按下计时。
 * @param key 已初始化的按键对象。
 * @return KEY_STATUS_OK 表示成功，否则返回错误码。
 */
key_status_t key_disable(key_t *key);

/**
 * @brief 查询按键当前稳定逻辑状态。
 * @param key 已初始化的按键对象。
 * @return KEY_STATE_PRESSED 表示按下，KEY_STATE_RELEASED 表示释放；无效对象返回释放。
 */
key_state_t key_get_state(const key_t *key);

/**
 * @brief 查询按键最近一次采样到的原始 GPIO 电平。
 * @param key 已初始化的按键对象。
 * @return true 表示高电平，false 表示低电平；无效对象返回 false。
 */
bool key_get_raw_level(const key_t *key);

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
                              uint32_t now_ms);

/**
 * @brief 扫描管理器中的全部按键。
 * @param manager 已初始化的管理器。
 * @param now_ms 当前毫秒时间戳。
 * @return KEY_STATUS_OK 表示全部扫描成功，否则返回第一个错误码。
 */
key_status_t key_manager_poll(key_manager_t *manager, uint32_t now_ms);

#ifdef __cplusplus
}
#endif

#endif /* KEY_H */
