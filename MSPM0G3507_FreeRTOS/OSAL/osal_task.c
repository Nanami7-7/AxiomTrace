/**
 * @file    osal_task.c
 * @brief   OSAL任务管理实现
 * @note    根据USE_FREERTOS宏条件编译:
 *          - FreeRTOS模式: 封装xTaskCreate/vTaskDelay等
 *          - 裸机模式: 仅保存函数指针,由主循环轮询调用
 */
#include "osal_api.h"

#if USE_FREERTOS
/* ======================== FreeRTOS模式实现 ======================== */

osal_task_handle_t osal_task_create(osal_task_func_t func,
    const char *name, uint16_t stack_size, void *param,
    uint32_t priority)
{
    osal_task_handle_t handle = NULL;

    /* 创建FreeRTOS任务 */
    BaseType_t ret = xTaskCreate(func, name, stack_size, param,
        (UBaseType_t)priority, &handle);

    if (pdPASS != ret) {
        return NULL;
    }

    return handle;
}

void osal_task_delete(osal_task_handle_t handle)
{
    vTaskDelete(handle);
}

osal_task_handle_t osal_task_get_current(void)
{
    return xTaskGetCurrentTaskHandle();
}

void osal_task_delay_ms(uint32_t ms)
{
    /* 将毫秒转换为FreeRTOS tick数 */
    vTaskDelay(pdMS_TO_TICKS(ms));
}

#else /* !USE_FREERTOS */
/* ======================== 裸机模式实现 ======================== */

osal_task_handle_t osal_task_create(osal_task_func_t func,
    const char *name, uint16_t stack_size, void *param,
    uint32_t priority)
{
    (void)name;
    (void)stack_size;
    (void)priority;

    /* 裸机模式: 仅保存函数指针和参数，由主循环调用 */
    static osal_task_handle_t s_task_slots[4U];
    static uint32_t s_task_count = 0U;

    if (s_task_count >= 4U) {
        return NULL;
    }

    s_task_slots[s_task_count].func  = func;
    s_task_slots[s_task_count].param = param;
    s_task_count++;

    return &s_task_slots[s_task_count - 1U];
}

void osal_task_delete(osal_task_handle_t handle)
{
    (void)handle;
    /* 裸机模式: 无动态删除，标记为无效即可 */
}

osal_task_handle_t osal_task_get_current(void)
{
    return NULL;
}

void osal_task_delay_ms(uint32_t ms)
{
    /* 裸机模式: 忙等延时 */
    osal_delay_ms(ms);
}

#endif /* USE_FREERTOS */
