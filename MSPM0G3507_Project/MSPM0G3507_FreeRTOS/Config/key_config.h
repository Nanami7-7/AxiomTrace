#ifndef KEY_CONFIG_H
#define KEY_CONFIG_H

#include "project_config.h"
#include "bsp_key.h"
#include "app_key_events.h"
#include "ti_msp_dl_config.h"

/* 按键功能开关、扫描周期和消抖/长按时间配置。 */
#ifndef PRJ_KEY_ENABLE
#define PRJ_KEY_ENABLE               (1U)
#endif
#define PRJ_KEY_COUNT                (2U)
#define PRJ_KEY_SCAN_PERIOD_MS       (10U)
#define PRJ_KEY_BUTTON_DEBOUNCE_MS   (20U)
#define PRJ_KEY_BUTTON_LONG_PRESS_MS (800U)
#define PRJ_KEY_BUTTON_MAX_HOLD_MS   (5000U)
#define PRJ_KEY_SWITCH_DEBOUNCE_MS   (20U)

/* 首次联调用的安全运动参数，短按/长按动作会读取这些配置。 */
#define PRJ_KEY_FORWARD_RPM          (200.0f)
#define PRJ_KEY_FORWARD_TIMEOUT_MS   (3000U)
#define PRJ_KEY_TURN_TARGET_DEG      (90.0f)
#define PRJ_KEY_TURN_CRUISE_RPM      (120.0f)
#define PRJ_KEY_TURN_TIMEOUT_MS      (5000U)
#define PRJ_KEY_TURN_SETTLE_MS       (100U)

#ifndef TASK_PRIO_KEY
#define TASK_PRIO_KEY                (3U)
#endif
#ifndef TASK_STACK_KEY
#define TASK_STACK_KEY               (256U)
#endif

/*
 * 兼容尚未重新生成 PA7/PB3 的 SysConfig 头文件：
 * 如果生成头中已有这些宏，优先使用生成值；否则使用已核对的引脚映射。
 */
#ifndef KEY_key_PORT
#define KEY_key_PORT                 (GPIOA)
#endif
#ifndef KEY_key_PIN
#define KEY_key_PIN                  (DL_GPIO_PIN_7)
#endif
#ifndef KEY_key_IOMUX
#define KEY_key_IOMUX                (IOMUX_PINCM14)
#endif
#ifndef KEY_switch_PORT
#define KEY_switch_PORT              (GPIOB)
#endif
#ifndef KEY_switch_PIN
#define KEY_switch_PIN               (DL_GPIO_PIN_3)
#endif
#ifndef KEY_switch_IOMUX
#define KEY_switch_IOMUX             (IOMUX_PINCM16)
#endif

#define PRJ_KEY_CONFIGS \
    { \
        .key = { \
            .id = 0U, \
            .type = KEY_TYPE_BUTTON, \
            .port = HAL_GPIO_PORT_A, \
            .pin = KEY_key_PIN, \
            .active_level = KEY_LEVEL_LOW, \
            .debounce_ms = PRJ_KEY_BUTTON_DEBOUNCE_MS, \
            .long_press_ms = PRJ_KEY_BUTTON_LONG_PRESS_MS, \
            .max_hold_ms = PRJ_KEY_BUTTON_MAX_HOLD_MS, \
            .event_mask = KEY_EVENT_MASK_PRESS | KEY_EVENT_MASK_RELEASE | \
                          KEY_EVENT_MASK_SHORT | KEY_EVENT_MASK_LONG | \
                          KEY_EVENT_MASK_STUCK, \
            .report_initial_state = false, \
            .read_level = NULL, \
            .read_user_data = NULL, \
            .callback = app_key_event_callback, \
            .callback_user_data = NULL \
        }, \
        .gpio_port = HAL_GPIO_PORT_A, \
        .gpio_pin = KEY_key_PIN \
    }, \
    { \
        .key = { \
            .id = 1U, \
            .type = KEY_TYPE_SWITCH, \
            .port = HAL_GPIO_PORT_B, \
            .pin = KEY_switch_PIN, \
            .active_level = KEY_LEVEL_LOW, \
            .debounce_ms = PRJ_KEY_SWITCH_DEBOUNCE_MS, \
            .long_press_ms = 0U, \
            .max_hold_ms = 0U, \
            .event_mask = KEY_EVENT_MASK_SWITCH, \
            .report_initial_state = true, \
            .read_level = NULL, \
            .read_user_data = NULL, \
            .callback = app_key_event_callback, \
            .callback_user_data = NULL \
        }, \
        .gpio_port = HAL_GPIO_PORT_B, \
        .gpio_pin = KEY_switch_PIN \
    }

#endif /* KEY_CONFIG_H */
