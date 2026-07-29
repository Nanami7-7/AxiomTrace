/**
 * @file    key_config.h
 * @brief   M0_Base 按键配置文件
 *
 * @details
 * 本文件集中管理按键数量、按键参数以及按键 GPIO 配置。
 * 新增或修改按键时，建议只修改本文件和对应的业务回调文件。
 * 1. 修改按键数量和扫描参数；
 * 2. 配置按键 ID、GPIO 端口和引脚；
 * 3. 在 PRJ_KEY_CONFIGS 中注册按键配置；
 * 4. 同步修改 PRJ_KEY_COUNT；
 * 5. 在 Application/app_key_actions.c 中编写按键业务处理代码。
 *
 * 当前工程仅使用 PA13 作为菜单按键，按下电平为低电平。
 */
#ifndef KEY_CONFIG_H
#define KEY_CONFIG_H

#include "hal_common.h"
#include "bsp_key.h"
#include "app_key_events.h"
#include "ti_msp_dl_config.h"

/* ==================== 按键通用参数 ==================== */

/** 按键功能开关：0=关闭，1=开启 */
#define PRJ_KEY_ENABLE              (1U)

/** 按键数量，必须与 PRJ_KEY_CONFIGS 中注册的按键数量一致 */
#define PRJ_KEY_COUNT               (1U)

/** 按键扫描周期，单位：ms。建议使用 5~20 ms */
#define PRJ_KEY_SCAN_PERIOD_MS      (5U)

/** 按键消抖时间，单位：ms。建议使用 10~30 ms */
#define PRJ_KEY_BUTTON_DEBOUNCE_MS  (20U)

/** 长按判定时间，单位：ms。当前配置为 800 ms */
#define PRJ_KEY_BUTTON_LONG_MS      (800U)

/** 最大按住时间，单位：ms；0 表示不限制最大按住时间 */
#define PRJ_KEY_BUTTON_MAX_HOLD_MS  (0U)

/** 按键任务优先级和任务栈大小 */
#define TASK_PRIO_KEY               (3U)
#define TASK_STACK_KEY              (256U)

/* ==================== 按键 ID 定义 ==================== */

/**
 * @brief 菜单按键 ID
 *
 * 按键 ID 从 0 开始编号。新增按键时，请为每个按键分配唯一 ID。
 */
#define PRJ_KEY_ID_MENU             (0U) /**< PA13 菜单按键 */

/* ==================== 按键 GPIO 配置 ==================== */

/** PA13 引脚号，通常由 SysConfig 生成；如需修改，请保持与硬件一致 */
#ifndef KEY_PA13_PIN
#define KEY_PA13_PIN                (DL_GPIO_PIN_13)
#endif

/** PA13 的 IOMUX 配置，通常由 SysConfig 生成 */
#ifndef KEY_PA13_IOMUX
#define KEY_PA13_IOMUX              (IOMUX_PINCM35)
#endif

/**
 * @brief PA13 菜单按键配置
 * @note 该按键使用 GPIOA、PA13，低电平表示按下。
 *       按键事件统一回调到 app_key_event_callback。
 */
#define PRJ_KEY_CONFIG_MENU \
    { \
        .key = { \
            .id = PRJ_KEY_ID_MENU, \
            .type = KEY_TYPE_BUTTON, \
            .port = HAL_GPIO_PORT_A, \
            .pin = KEY_PA13_PIN, \
            .active_level = KEY_LEVEL_LOW, \
            .debounce_ms = PRJ_KEY_BUTTON_DEBOUNCE_MS, \
            .long_press_ms = PRJ_KEY_BUTTON_LONG_MS, \
            .max_hold_ms = PRJ_KEY_BUTTON_MAX_HOLD_MS, \
            .event_mask = KEY_EVENT_MASK_SHORT | KEY_EVENT_MASK_LONG, \
            .report_initial_state = false, \
            .read_level = NULL, \
            .read_user_data = NULL, \
            .callback = app_key_event_callback, \
            .callback_user_data = NULL \
        }, \
        .gpio_port = GPIOA, \
        .gpio_pin = KEY_PA13_PIN, \
        .gpio_iomux = KEY_PA13_IOMUX \
    }

/**
 * @brief 提供给 app_main.c 的按键配置表
 *
 * 新增按键时，请按以下方式扩展：
 *   1. 新增一个 PRJ_KEY_CONFIG_xxx 宏；
 *   2. 在这里用逗号分隔注册，例如：
 *      PRJ_KEY_CONFIG_MENU, \
 *      PRJ_KEY_CONFIG_USER
 *   3. 将 PRJ_KEY_COUNT 修改为 2U。
 */
#define PRJ_KEY_CONFIGS             PRJ_KEY_CONFIG_MENU

#endif /* KEY_CONFIG_H */