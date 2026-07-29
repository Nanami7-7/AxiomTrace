/**
 * @file    main.c
 * @brief   Generic MSPM0G3507 + FreeRTOS entry point.
 */
#include <stdint.h>
#include <stdio.h>
#include "FreeRTOS.h"
#include "task.h"
#include "ti_msp_dl_config.h"
#include "app_main.h"

int main(void)
{
    SYSCFG_DL_init();

    /* 最早期启动日志：用于区分“没有烧录/串口配置错误”和“应用初始化卡住”。 */
    printf("[BOOT] main entered, UART0=PA10/PA11 9600\r\n");

    (void)app_main_init();
    vTaskStartScheduler();

    for (;;) {
    }
}

#if (configSUPPORT_STATIC_ALLOCATION == 1)
void vApplicationGetIdleTaskMemory(
    StaticTask_t **ppxIdleTaskTCBBuffer,
    StackType_t **ppxIdleTaskStackBuffer,
    uint32_t *pulIdleTaskStackSize)
{
    static StaticTask_t s_idle_task_tcb;
    static StackType_t s_idle_task_stack[configIDLE_TASK_STACK_DEPTH];

    *ppxIdleTaskTCBBuffer = &s_idle_task_tcb;
    *ppxIdleTaskStackBuffer = s_idle_task_stack;
    *pulIdleTaskStackSize = configIDLE_TASK_STACK_DEPTH;
}

#if (configUSE_TIMERS == 1)
void vApplicationGetTimerTaskMemory(
    StaticTask_t **ppxTimerTaskTCBBuffer,
    StackType_t **ppxTimerTaskStackBuffer,
    uint32_t *pulTimerTaskStackSize)
{
    static StaticTask_t s_timer_task_tcb;
    static StackType_t s_timer_task_stack[configTIMER_TASK_STACK_DEPTH];

    *ppxTimerTaskTCBBuffer = &s_timer_task_tcb;
    *ppxTimerTaskStackBuffer = s_timer_task_stack;
    *pulTimerTaskStackSize = configTIMER_TASK_STACK_DEPTH;
}
#endif
#endif

#if (configCHECK_FOR_STACK_OVERFLOW)
#if defined(__GNUC__) || defined(__clang__)
#define OSAL_WEAK __attribute__((weak))
#elif defined(__CC_ARM) || defined(__ARMCC_VERSION)
#define OSAL_WEAK __weak
#else
#define OSAL_WEAK
#endif
/**
 * @brief ?? FreeRTOS ????????
 * @param task ??????????
 * @param name ??????????
 * @return ??
 */
OSAL_WEAK void vApplicationStackOverflowHook(TaskHandle_t task, char *name)
{
    (void)task;
    (void)name;
    for (;;) {
    }
}
#endif
