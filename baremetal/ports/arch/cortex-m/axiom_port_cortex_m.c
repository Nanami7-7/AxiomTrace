#include "axiom_port.h"

/* ============================================================================
 * Cortex-M Architecture Port
 * ============================================================================
 *
 * ARM Cortex-M specific implementations using CoreDebug/DWT registers.
 * Works on Cortex-M3, M4, M7, M33, etc. Not M0 (no DWT_CYCCNT).
 *
 * Default weak implementations - override by providing strong symbols.
 * ============================================================================ */

#if defined(__ARM_ARCH_7M__) || defined(__ARM_ARCH_7EM__) || \
    defined(__ARM_ARCH_8M_BASE__) || defined(__ARM_ARCH_8M_MAIN__)
#define CORTEX_M_HAS_DWT 1
#else
#define CORTEX_M_HAS_DWT 0
#endif

#if CORTEX_M_HAS_DWT
/* DWT_CYCCNT 寄存器地址 (Cortex-M7 example, varies by core) */
static volatile uint32_t * const DWT_CTRL = (uint32_t *)0xE0001000;
static volatile uint32_t * const DWT_CYCCNT = (uint32_t *)0xE0001004;
static volatile uint32_t * const SCB_DEMCR = (uint32_t *)0xE000EDFC;
#endif

uint32_t axiom_port_timestamp(void) {
#if CORTEX_M_HAS_DWT
    /* 确保 DWT_CYCCNT 已启用 */
    if ((*DWT_CTRL) == 0) {
        /* 启用 trace, 先解锁 DWT */
        *SCB_DEMCR = 0x01000000;  /* TRCENA bit */
        *DWT_CTRL = 0x40000001;   /* CYCCNTENA bit */
    }
    return (uint32_t)*DWT_CYCCNT;
#else
    /* 降级方案: 使用 SysTick 或返回 0 */
    return 0u;
#endif
}

void axiom_port_critical_enter(void) {
    /* 方法1: 使用 PRIMASK (最简单, 禁用所有中断) */
    __asm volatile ("cpsid i" : : : "memory");

    /* 方法2: 使用 BASEPRI (可选, 保留高优先级中断)
     * 适用于需要保留 NMI 或 HardFault 的场景
     * __asm volatile ("msr basepri, %0" : : "r" (0x10) : "memory");
     */
}

void axiom_port_critical_exit(void) {
    __asm volatile ("cpsie i" : : : "memory");

    /* 对应 BASEPRI 方法:
     * __asm volatile ("msr basepri, %0" : : "r" (0) : "memory");
     */
}

void axiom_port_string_out(const char *str) {
    /* 默认空实现 - 由具体 SoC/Board 实现提供 */
    (void)str;
}

void axiom_port_fault_hook(uint8_t module_id, uint16_t event_id,
                           const uint8_t *payload, uint8_t payload_len) {
    (void)module_id;
    (void)event_id;
    (void)payload;
    (void)payload_len;
    /* 可在此处添加硬件调试器集成 */
}

uint8_t axiom_port_reset_reason(void) {
    return 0u;
}

uint8_t axiom_port_fault_snapshot(uint8_t *buf, uint8_t max_len) {
    (void)buf;
    (void)max_len;
    return 0;
}

int axiom_port_flash_erase(uint32_t addr, uint32_t len) {
    (void)addr;
    (void)len;
    return -1;  /* 需要 SoC 实现 */
}

int axiom_port_flash_write(uint32_t addr, const uint8_t *data, uint32_t len) {
    (void)addr;
    (void)data;
    (void)len;
    return -1;  /* 需要 SoC 实现 */
}

int axiom_port_flash_read(uint32_t addr, uint8_t *out, uint32_t len) {
    (void)addr;
    (void)out;
    (void)len;
    return -1;  /* 需要 SoC 实现 */
}
