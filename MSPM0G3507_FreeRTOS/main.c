/**
 * @file    main.c
 * @brief   系统入口: 硬件初始化 + 应用层初始化 + 启动调度器
 * @note    启动流程:
 *          1. SYSCFG_DL_init() - SysConfig生成的外设初始化
 *          2. app_main_init()  - 应用层初始化(BSP+PID+任务创建)
 *          3. vTaskStartScheduler() - 启动FreeRTOS调度器
 *
 *          五层架构依赖方向:
 *          main.c → app_main → BSP + OSAL → HAL → SDK
 *
 *          本文件仅包含FreeRTOS静态分配回调和栈溢出钩子,
 *          不包含业务逻辑
 */
#include "FreeRTOS.h"
#include "task.h"
#include "ti_msp_dl_config.h"
#include "app_main.h"
#include "bsp_motor.h"
#include "bsp_timer.h"
#include "test_uart_dma.h"
/* ======================== main入口 ======================== */

int main(void)
{
    /* 第1步: SysConfig生成的硬件初始化(时钟/GPIO/UART/Timer/ADC) */
    SYSCFG_DL_init();

    /* 第2步: 应用层初始化(BSP模块+PID控制器+FreeRTOS任务) */
    app_main_init();

    /* 第3步: 确保UART中断使能(SYSCFG_DL_init只配置外设，未使能NVIC) */
   // NVIC_EnableIRQ(UART0_INT_IRQn);

    /* 初始化TIMER_0为测试提供计时基准 */
    bsp_timer_init();

    /* DMA测试(调度器启动前执行) */
  //  test_uart_dma_run();

    /* 第4步: 启动FreeRTOS调度器,不再返回 */
    vTaskStartScheduler();

    /* 调度器启动失败(内存不足) */
    for (;;) {
    }
}

/* ======================== FreeRTOS静态分配回调 ======================== */

#if (configSUPPORT_STATIC_ALLOCATION == 1)

/**
 * @brief  提供空闲任务的静态内存
 * @note   configSUPPORT_STATIC_ALLOCATION=1时必须实现
 */
void vApplicationGetIdleTaskMemory(
    StaticTask_t **ppxIdleTaskTCBBuffer,
    StackType_t **ppxIdleTaskStackBuffer,
    uint32_t *pulIdleTaskStackSize)
{
    static StaticTask_t s_idle_task_tcb;
    static StackType_t s_idle_task_stack[
        configIDLE_TASK_STACK_DEPTH];

    *ppxIdleTaskTCBBuffer   = &s_idle_task_tcb;
    *ppxIdleTaskStackBuffer = s_idle_task_stack;
    *pulIdleTaskStackSize   = configIDLE_TASK_STACK_DEPTH;
}

#if (configUSE_TIMERS == 1)

/**
 * @brief  提供定时器服务任务的静态内存
 * @note   configUSE_TIMERS=1时必须实现
 */
void vApplicationGetTimerTaskMemory(
    StaticTask_t **ppxTimerTaskTCBBuffer,
    StackType_t **ppxTimerTaskStackBuffer,
    uint32_t *pulTimerTaskStackSize)
{
    static StaticTask_t s_timer_task_tcb;
    static StackType_t s_timer_task_stack[
        configTIMER_TASK_STACK_DEPTH];

    *ppxTimerTaskTCBBuffer   = &s_timer_task_tcb;
    *ppxTimerTaskStackBuffer = s_timer_task_stack;
    *pulTimerTaskStackSize   = configTIMER_TASK_STACK_DEPTH;
}

#endif /* configUSE_TIMERS */

#endif /* configSUPPORT_STATIC_ALLOCATION */

/* ======================== 栈溢出钩子 ======================== */

#if (configCHECK_FOR_STACK_OVERFLOW)

/**
 * @brief  栈溢出检测钩子
 * @note   检测到栈溢出时进入死循环
 *         可在此添加LED快闪等错误指示
 */
#if defined(__IAR_SYSTEMS_ICC__)
#define OSAL_WEAK __weak
#elif defined(__TI_COMPILER_VERSION__)
#define OSAL_WEAK
#pragma WEAK(vApplicationStackOverflowHook)
#elif defined(__GNUC__) || defined(__ti_version__)
#define OSAL_WEAK __attribute__((weak))
#else
#define OSAL_WEAK
#endif
OSAL_WEAK void vApplicationStackOverflowHook(
    TaskHandle_t pxTask, char *pcTaskName)
{
    (void)pxTask;
    (void)pcTaskName;
    for (;;) {
    }
}

#endif /* configCHECK_FOR_STACK_OVERFLOW */
