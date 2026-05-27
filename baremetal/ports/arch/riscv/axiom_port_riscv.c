#include "axiom_port.h"

/* ============================================================================
 * RISC-V Architecture Port
 * ============================================================================
 *
 * RISC-V specific implementations using machine-mode registers.
 * Works on RV32I/RV64I cores with standard mtime/mcycle.
 *
 * Default weak implementations - override by providing strong symbols.
 * ============================================================================ */

#if defined(__riscv)

/* RISC-V CSR 地址 */
#define CSR_MSTATUS 0x300
#define CSR_MIE     0x304
#define CSR_MTVEC   0x305

/* mtime 寄存器 (需要根据具体 SoC 确定地址) */
static volatile uint64_t * const MTIME = (uint64_t *)0x2000000;
static volatile uint64_t * const MTIMECMP = (uint64_t *)0x2000008;

/* 读取 mcycle CSR */
static inline uint64_t __riscv_read_mcycle(void) {
    uint64_t val;
    __asm volatile ("csrr %0, mcycle" : "=r"(val));
    return val;
}

#endif

uint32_t axiom_port_timestamp(void) {
#if defined(__riscv)
    /* 返回低 32 位 mcycle */
    return (uint32_t)__riscv_read_mcycle();
#else
    return 0u;
#endif
}

#define MSTATUS_MIE 0x8u

static uint32_t g_critical_nesting = 0u;
static uint32_t g_mstatus_state = 0u;

void axiom_port_critical_enter(void) {
#if defined(__riscv)
    /* Atomically save mstatus and clear MIE before touching nesting state. */
    uint32_t mstatus;
    const uint32_t mie = MSTATUS_MIE;
    __asm volatile ("csrrc %0, mstatus, %1" : "=r"(mstatus) : "r"(mie) : "memory");
    if (g_critical_nesting == 0u) {
        g_mstatus_state = mstatus;
    }
    g_critical_nesting++;
#endif
}

void axiom_port_critical_exit(void) {
#if defined(__riscv)
    if (g_critical_nesting > 0u) {
        g_critical_nesting--;
        if (g_critical_nesting == 0u && (g_mstatus_state & MSTATUS_MIE) != 0u) {
            const uint32_t mie = MSTATUS_MIE;
            __asm volatile ("csrs mstatus, %0" : : "r"(mie) : "memory");
        }
    }
#endif
}

void axiom_port_string_out(const char *str) {
    (void)str;
    /* 默认空实现 - 由具体 SoC/Board 实现提供 */
}

void axiom_port_fault_hook(uint8_t module_id, uint16_t event_id,
                           const uint8_t *payload, uint8_t payload_len) {
    (void)module_id;
    (void)event_id;
    (void)payload;
    (void)payload_len;
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
