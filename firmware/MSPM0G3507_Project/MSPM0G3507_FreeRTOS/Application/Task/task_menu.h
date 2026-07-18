/**
 * @file    task_menu.h
 * @brief   菜单任务接口
 * @note    串口交互菜单: 选电机/设RPM/调PID/启停电机
 */
#ifndef TASK_MENU_H
#define TASK_MENU_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/**
 * @brief  菜单任务函数
 * @note   串口交互菜单: 选电机/设RPM/调PID/启停电机
 * @param  param 任务参数(app_shared_ctx_t指针)
 */
void app_menu_task(void *param);

#ifdef __cplusplus
}
#endif

#endif /* TASK_MENU_H */