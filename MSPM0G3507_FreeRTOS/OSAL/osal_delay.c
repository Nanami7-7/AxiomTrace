/**
 * @file    osal_delay.c
 * @brief   OSAL延时实现
 * @note    提供忙等延时函数，基于SysTick倒计数(80MHz)
 *          osal_delay_ms/osal_delay_us: 忙等延时，可用于ISR和初始化
 *          osal_task_delay_ms: 任务延时(让出CPU)，在osal_task.c中实现
 *
 *          80MHz CPUCLK = 12.5ns/cycle
 *          1us = 80 cycles
 *          1ms = 80000 cycles
 *
 *          SysTick使用策略:
 *          - FreeRTOS模式: 调度器启动时自动配置SysTick(1ms周期)
 *          - 裸机模式: 需在main()中调用osal_delay_init()手动配置
 *            配置后SysTick以1ms周期倒计数，不使能中断
 */
#include "osal_api.h"
#include <ti/devices/msp/msp.h>

/* CPU时钟频率(80MHz) */
#define CPUCLK_HZ    80000000UL
/* 每微秒的CPU周期数 */
#define CYCLES_PER_US  (CPUCLK_HZ / 1000000UL)
/* 1ms对应的周期数(用于裸机SysTick LOAD值) */
#define CYCLES_PER_MS  (CPUCLK_HZ / 1000UL)

/* ======================== 裸机模式SysTick初始化 ======================== */

#if !USE_FREERTOS

/** 裸机模式SysTick初始化标志 */
static volatile bool s_systick_inited = false;

/**
 * @brief  初始化SysTick(裸机模式专用)
 * @note   配置SysTick为1ms周期倒计数，不使能中断
 *         必须在首次调用osal_delay_us/ms之前调用
 *         FreeRTOS模式下无需调用(调度器自动配置)
 *
 *         SysTick寄存器说明(SYST_CSR):
 *         bit2 CLKSOURCE=1: 使用CPU时钟(80MHz)
 *         bit1 TICKINT=0:   不产生中断(裸机仅用计数器)
 *         bit0 ENABLE=1:    使能计数器
 */
void osal_delay_init(void)
{
    if (s_systick_inited) {
        return;
    }

    /* 停止并复位SysTick */
    SysTick->CTRL = 0U;
    SysTick->VAL  = 0U;

    /* 设置重载值: 1ms周期 = 80000 - 1 */
    SysTick->LOAD = CYCLES_PER_MS - 1U;

    /* 使能SysTick: CLKSOURCE=CPU时钟, TICKINT=关, ENABLE=开 */
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk
                  | SysTick_CTRL_ENABLE_Msk;

    s_systick_inited = true;
}

/**
 * @brief  检查SysTick是否已初始化(裸机模式内部使用)
 * @retval true  已初始化
 * @retval false 未初始化
 */
bool osal_delay_is_inited(void)
{
    return s_systick_inited;
}

#endif /* !USE_FREERTOS */

/* ======================== 忙等延时实现 ======================== */

/**
 * @brief  忙等延时(微秒)
 * @note   基于SysTick->VAL倒计数实现，精度约1us
 *         SysTick为24位倒计数器，80MHz下最大约209ms
 *
 *         FreeRTOS模式: 调度器启动后SysTick已使能
 *         裸机模式: 需先调用osal_delay_init()初始化SysTick
 *
 * @param  us 延时微秒数(0~209000)
 */
void osal_delay_us(uint32_t us)
{
    if (0U == us) {
        return;
    }

#if !USE_FREERTOS
    /* 裸机模式安全检查: SysTick未初始化则直接返回 */
    if (!s_systick_inited) {
        return;
    }
#endif

    /*
     * 利用SysTick当前值寄存器(SysTick->VAL)做精确短延时
     * SysTick以1ms周期倒计数(LOAD=79999)
     * VAL范围: 0 ~ LOAD, 每次VAL经过一个完整周期 = 1ms
     *
     * VAL倒计数行为:
     * LOAD → LOAD-1 → ... → 1 → 0 → LOAD(重载) → ...
     * 当VAL从大值跳到更大值时，说明发生了重载
     */
    uint32_t total_cycles = us * CYCLES_PER_US;

    while (total_cycles > 0U) {
        uint32_t start_val = SysTick->VAL;
        uint32_t load_val  = SysTick->LOAD;

        /* 本轮等待周期数(不超过一个SysTick周期) */
        uint32_t wait_cycles = total_cycles;
        if (wait_cycles > load_val) {
            wait_cycles = load_val;
        }

        /* 等待VAL倒计数过wait_cycles个周期 */
        uint32_t elapsed;
        do {
            uint32_t current_val = SysTick->VAL;
            if (current_val <= start_val) {
                /* 正常倒计数: elapsed = start - current */
                elapsed = start_val - current_val;
            } else {
                /* VAL回绕: 经过了LOAD→0的重载
                 * elapsed = (start → 0) + (LOAD → current)
                 *         = start + (LOAD - current + 1)
                 */
                elapsed = start_val + (load_val - current_val) + 1U;
            }
        } while (elapsed < wait_cycles);

        total_cycles -= wait_cycles;
    }
}

/**
 * @brief  忙等延时(毫秒)
 * @note   基于循环调用osal_delay_us实现
 *         可用于ISR、初始化阶段等非任务上下文
 *         FreeRTOS任务中优先使用osal_task_delay_ms(让出CPU)
 * @param  ms 延时毫秒数
 */
void osal_delay_ms(uint32_t ms)
{
    if (0U == ms) {
        return;
    }

    while (ms > 0U) {
        osal_delay_us(1000U);
        ms--;
    }
}
