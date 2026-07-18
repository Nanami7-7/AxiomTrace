/**
 * @file    platform_mspm0.c
 * @brief   MSPM0G3507 平台接口实现
 *
 * 实现 platform.h 定义的平台抽象接口：
 *   - delay_ms / delay_us — 延时函数
 *   - get_tick_us — 微秒级时间戳（TimerG8）
 *   - debug_printf — UART 调试输出
 *
 * 系统配置（从 ti_msp_dl_config.h 自动获取）：
 *   - 系统时钟：CPUCLK_FREQ (80MHz, HFXT 40MHz → SYSPLL ×2)
 *   - 计时器：TimerG8 (TIMER_0_INST)，500kHz（2us 分辨率，×2 返回 us）
 *   - 调试串口：UART0，115200 baud
 */

#include "platform.h"
#include "ti_msp_dl_config.h"
#include "project_config.h" /* PRJ_SYS_TICK_US_PER_TICK_X1000 */
#include <stdarg.h>
#include <stdio.h>

/* 从 SysConfig 生成的时钟频率自动计算延时常数 */
#define CYCLES_PER_MS   (CPUCLK_FREQ / 1000)        /* 80000 @ 80MHz */
#define CYCLES_PER_US   (CPUCLK_FREQ / 1000000)     /* 80 @ 80MHz */

/* ============================================================
 * 延时函数
 * ============================================================ */

/**
 * @brief 毫秒延时（忙等待）
 * @param ms 延时毫秒数
 *
 * 使用 DL_Common_delayCycles 实现，精度取决于系统时钟。
 * 时钟频率从 CPUCLK_FREQ (ti_msp_dl_config.h) 自动获取。
 */
static void mspm0_delay_ms(uint32_t ms)
{
    /* 分段延时避免溢出：CYCLES_PER_MS * ms 在 ms > 53687 时 uint32_t 溢出
     * 每段最大 50000ms（4G cycles），安全余量充足 */
    while (ms > 50000) {
        DL_Common_delayCycles(CYCLES_PER_MS * 50000UL);
        ms -= 50000;
    }
    DL_Common_delayCycles(CYCLES_PER_MS * ms);
}

/**
 * @brief 微秒延时（忙等待）
 * @param us 延时微秒数
 *
 * 时钟频率从 CPUCLK_FREQ (ti_msp_dl_config.h) 自动获取。
 * 注意：函数调用开销约 1-2us，实际延时略长
 */
static void mspm0_delay_us(uint32_t us)
{
    DL_Common_delayCycles(CYCLES_PER_US * us);
}

/* ============================================================
 * 计时函数
 * ============================================================ */

/**
 * @brief 获取微秒级时间戳
 * @return 自 TimerG8 启动以来的微秒数
 *
 * TimerG8 (TIMER_0_INST) 配置为 500kHz（2us 分辨率），16 位计数器。
 * LOAD_VALUE = 32767，溢出周期 = (32767+1) / 500kHz ≈ 65.5ms
 *
 * 每个 tick = 2us，乘以 2 返回微秒数。
 * 使用 TIMER_0_INST_LOAD_VALUE 确保与 SysConfig 配置一致。
 *
 * 使用方式：
 * @code
 *   uint32_t t0 = g_platform->get_tick_us();
 *   // ... 执行操作 ...
 *   uint32_t t1 = g_platform->get_tick_us();
 *   uint32_t elapsed_us = t1 - t0;  // uint32_t 减法自动处理溢出
 * @endcode
 */
static uint32_t mspm0_get_tick_us(void)
{
    /* TimerG8 @ 500kHz: 每个 tick = 2us
     * PRJ_SYS_TICK_US_PER_TICK_X1000 = 2000 (×1000 扩大精度避免浮点)
     * 实际 us = count * 2000 / 1000 = count * 2
     * 使用 uint64_t 中间运算防止 count * 2000 溢出（count 最大 65535，安全但防御性编程） */
    return (uint32_t)((uint64_t)DL_Timer_getTimerCount(TIMER_0_INST)
                      * PRJ_SYS_TICK_US_PER_TICK_X1000 / 1000UL);
}

/* ============================================================
 * 调试输出
 * ============================================================ */

/**
 * @brief 调试输出函数（UART0）
 * @param fmt 格式字符串（printf 兼容）
 * @param ... 可变参数
 * @return 输出字符数
 *
 * 使用阻塞方式发送每个字符，确保输出完整性。
 * 波特率 115200 时，发送 1 字节约 87us。
 */
static int mspm0_debug_printf(const char *fmt, ...)
{
    char buf[128];
    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    if (len <= 0) return 0;
    if (len > (int)sizeof(buf) - 1) len = (int)sizeof(buf) - 1;

    for (int i = 0; i < len; i++) {
        DL_UART_transmitDataBlocking(UART0, (uint8_t)buf[i]);
    }

    return len;
}

/* ============================================================
 * 平台实例
 * ============================================================ */

/**
 * @brief MSPM0G3507 平台实例
 *
 * 存储在 Flash 中（const 修饰），包含所有平台函数指针。
 */
const platform_t mspm0_platform = {
    .delay_ms       = mspm0_delay_ms,
    .delay_us       = mspm0_delay_us,
    .get_tick_us    = mspm0_get_tick_us,
    .system_clock_hz = CPUCLK_FREQ,  /* 从 ti_msp_dl_config.h 自动获取 */
    .debug_printf   = mspm0_debug_printf,
};

/**
 * @brief 全局平台指针
 *
 * 指向 MSPM0 平台实例。
 * 在 main() 中设置：g_platform = &mspm0_platform;
 *
 * @note 使用非 const 指针，允许运行时切换（如测试时使用 mock）
 */
const platform_t *g_platform = &mspm0_platform;

/* ============================================================
 * 平台初始化辅助函数
 * ============================================================ */

/**
 * @brief 初始化 TimerG8 为微秒计时器
 *
 * 在 SYSCFG_DL_init() 之后调用此函数启动计时器。
 * TimerG8 (TIMER_0_INST) 已在 ti_msp_dl_config.c 中配置为 500kHz（2us 分辨率）周期计时器。
 */
void platform_timer_init(void)
{
    /* TimerG8 (TIMER_0_INST) 已由 SYSCFG_DL_init() 配置 */
    /* 此函数仅启动计时器 */
    DL_Timer_startCounter(TIMER_0_INST);
}
