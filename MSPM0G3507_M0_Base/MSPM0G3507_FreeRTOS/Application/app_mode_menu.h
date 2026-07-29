#ifndef APP_MODE_MENU_H
#define APP_MODE_MENU_H

#include <stdbool.h>
#include <stdint.h>
#include "key.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 将 PA13 产生的短按/长按事件投递给菜单任务。
 * @note  按键回调只调用本接口，不在回调中直接执行 OLED 或耗时业务。
 */
void app_mode_menu_on_key_event(key_event_type_t event);

/** @brief 菜单 FreeRTOS 任务入口。 */
void app_mode_menu_task(void *param);

/**
 * @brief 获取当前模式编号。
 * @return 0~9，分别对应 USER01~USER10。
 */
uint8_t app_mode_menu_get_current(void);

/**
 * @brief 判断当前是否已经进入某个 USER 模式。
 * @return true 表示正在运行模式，false 表示仍在模式列表。
 */
bool app_mode_menu_is_running(void);

/* ========================================================================== */
/* USER01~USER10 二次开发接口                                                 */
/*                                                                            */
/* enter(): 菜单长按进入该模式时调用一次。                                   */
/* run():   已进入该模式后每 100 ms 调用一次。                                */
/* short(): 已进入该模式后 PA13 短按时调用一次。                              */
/*                                                                            */
/* 直接修改对应 .c 文件中的函数体即可，不需要修改模式表和按键分发代码。        */
/* ========================================================================== */

void app_mode_user01_enter(void);
void app_mode_user01_run(void);
void app_mode_user01_short(void);

void app_mode_user02_enter(void);
void app_mode_user02_run(void);
void app_mode_user02_short(void);

void app_mode_user03_enter(void);
void app_mode_user03_run(void);
void app_mode_user03_short(void);

void app_mode_user04_enter(void);
void app_mode_user04_run(void);
void app_mode_user04_short(void);

void app_mode_user05_enter(void);
void app_mode_user05_run(void);
void app_mode_user05_short(void);

void app_mode_user06_enter(void);
void app_mode_user06_run(void);
void app_mode_user06_short(void);

void app_mode_user07_enter(void);
void app_mode_user07_run(void);
void app_mode_user07_short(void);

void app_mode_user08_enter(void);
void app_mode_user08_run(void);
void app_mode_user08_short(void);

void app_mode_user09_enter(void);
void app_mode_user09_run(void);
void app_mode_user09_short(void);

void app_mode_user10_enter(void);
void app_mode_user10_run(void);
void app_mode_user10_short(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_MODE_MENU_H */
