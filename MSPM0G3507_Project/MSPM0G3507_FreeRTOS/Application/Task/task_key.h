/**
 * @file    task_key.h
 * @brief   FreeRTOS 按键扫描任务接口。
 * @details 任务只按固定周期调用 BSP 扫描，不在任务中实现短按/长按业务逻辑。
 */
#ifndef TASK_KEY_H
#define TASK_KEY_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 按键扫描任务入口。
 * @param param 指向 bsp_key_manager_t 管理器对象。
 */
void app_key_task(void *param);

#ifdef __cplusplus
}
#endif

#endif /* TASK_KEY_H */
