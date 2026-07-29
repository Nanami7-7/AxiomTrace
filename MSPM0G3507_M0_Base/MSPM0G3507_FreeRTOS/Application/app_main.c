/**
 * @file    app_main.c
 * @brief   Minimal M0 application: PA13 key and software-I2C OLED menu.
 *
 * Short press selects the next mode. Long press enters the selected mode
 * or returns to the mode list. Add business code in app_mode_menu.c.
 */
#include "app_main.h"
#include "app_gateway.h"
#include "app_mode_menu.h"
#include "osal_api.h"
#include "oled.h"
#include "oled_port.h"
#include "bsp_key.h"
#include "bsp_ir.h"
#include "task_key.h"
#include "key_config.h"
#include "ti_msp_dl_config.h"
#include "project_config.h"
#include <stdio.h>

#define APP_KEY_TASK_STACK_WORDS  (TASK_STACK_KEY)
#define APP_KEY_TASK_PRIORITY     (TASK_PRIO_KEY)
#define APP_MENU_TASK_STACK_WORDS (384U)
#define APP_MENU_TASK_PRIORITY    (2U)

/* 应用初始化和任务创建。 */
static bsp_key_manager_t s_key_manager;
static bsp_key_instance_t s_key_instances[PRJ_KEY_COUNT];
static const bsp_key_config_t s_key_configs[PRJ_KEY_COUNT] = {
    PRJ_KEY_CONFIGS
};

/**
 * @brief 初始化函数 app_main_init，完成对应模块的功能处理。
 * @return 函数执行结果。
 */
int32_t app_main_init(void)
{
    OLED_Port_Init();
    DL_Common_delayCycles(CPUCLK_FREQ / 10U);
    OLED_Init();
    /* 红外输入由 SysConfig 配置，BSP 负责保持接口初始化一致。 */
    BSP_IR_Init();

#if (PRJ_KEY_ENABLE != 0U)
    /* 应用初始化和任务创建。 */
    if (bsp_key_manager_init(&s_key_manager,
                             s_key_instances,
                             s_key_configs,
                             PRJ_KEY_COUNT,
                             osal_ticks_to_ms(osal_get_tick_count())) != KEY_STATUS_OK) {
        return -1;
    }

    if (osal_task_create(app_key_task,
                         "key",
                         APP_KEY_TASK_STACK_WORDS,
                         &s_key_manager,
                         APP_KEY_TASK_PRIORITY) == NULL) {
        return -1;
    }
#else
    /* 应用初始化和任务创建。 */
#endif

    if (osal_task_create(app_mode_menu_task,
                         "menu",
                         APP_MENU_TASK_STACK_WORDS,
                         NULL,
                         APP_MENU_TASK_PRIORITY) == NULL) {
        return -1;
    }

    /* 启动 AB 板协议网关；UART1 接 Board A，UART2 独占给 BLE 透明协议。 */
    if (app_gateway_init() != 0) {
        return -2;
    }

    return 0;
}