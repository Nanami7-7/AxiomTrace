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

/**
 * @brief 删除函数 osal_task_delete，完成对应模块的功能处理。
 * @param handle 函数参数 handle。
 * @return 函数执行结果。
 */
void osal_task_delete(osal_task_handle_t handle)
{
    vTaskDelete(handle);
}

/**
 * @brief 获取函数 osal_task_get_current，完成对应模块的功能处理。
 * @return 函数执行结果。
 */
osal_task_handle_t osal_task_get_current(void)
{
    return xTaskGetCurrentTaskHandle();
}

/**
 * @brief 执行任务函数 osal_task_delay_ms，完成对应模块的功能处理。
 * @param ms 函数参数 ms。
 * @return 函数执行结果。
 */
void osal_task_delay_ms(uint32_t ms)
{
    /* 将毫秒转换为FreeRTOS tick数 */
    vTaskDelay(pdMS_TO_TICKS(ms));
}

/**
 * @brief 获取函数 osal_get_tick_count，完成对应模块的功能处理。
 * @return 函数执行结果。
 */
uint32_t osal_get_tick_count(void)
{
    return (uint32_t)xTaskGetTickCount();
}

/**
 * @brief 执行周期处理函数 osal_ms_to_ticks，完成对应模块的功能处理。
 * @param ms 函数参数 ms。
 * @return 函数执行结果。
 */
uint32_t osal_ms_to_ticks(uint32_t ms)
{
    return pdMS_TO_TICKS(ms);
}

/**
 * @brief 执行周期处理函数 osal_ticks_to_ms，完成对应模块的功能处理。
 * @param ticks 函数参数 ticks。
 * @return 函数执行结果。
 */
uint32_t osal_ticks_to_ms(uint32_t ticks)
{
    return ticks * portTICK_PERIOD_MS;
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

/**
 * @brief 删除函数 osal_task_delete，完成对应模块的功能处理。
 * @param handle 函数参数 handle。
 * @return 函数执行结果。
 */
void osal_task_delete(osal_task_handle_t handle)
{
    (void)handle;
    /* 裸机模式: 无动态删除，标记为无效即可 */
}

/**
 * @brief 获取函数 osal_task_get_current，完成对应模块的功能处理。
 * @return 函数执行结果。
 */
osal_task_handle_t osal_task_get_current(void)
{
    return NULL;
}

/**
 * @brief 执行任务函数 osal_task_delay_ms，完成对应模块的功能处理。
 * @param ms 函数参数 ms。
 * @return 函数执行结果。
 */
void osal_task_delay_ms(uint32_t ms)
{
    /* 裸机模式: 忙等延时 */
    osal_delay_ms(ms);
}

/**
 * @brief 获取函数 osal_get_tick_count，完成对应模块的功能处理。
 * @return 函数执行结果。
 */
uint32_t osal_get_tick_count(void)
{
    /* 裸机模式: 返回自维护的毫秒计数器 */
    extern volatile uint32_t g_osal_tick_ms;
    return g_osal_tick_ms;
}

/**
 * @brief 执行周期处理函数 osal_ms_to_ticks，完成对应模块的功能处理。
 * @param ms 函数参数 ms。
 * @return 函数执行结果。
 */
uint32_t osal_ms_to_ticks(uint32_t ms)
{
    /* 裸机模式: 1 tick = 1 ms */
    return ms;
}

/**
 * @brief 执行周期处理函数 osal_ticks_to_ms，完成对应模块的功能处理。
 * @param ticks 函数参数 ticks。
 * @return 函数执行结果。
 */
uint32_t osal_ticks_to_ms(uint32_t ticks)
{
    /* 裸机模式: 1 tick = 1 ms */
    return ticks;
}

#endif /* USE_FREERTOS */
