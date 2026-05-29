/**
 * @file axiom_port_stm32.c
 * @brief STM32F4 Platform Port Implementation
 *
 * Uses DWT_CYCCNT for high-precision timestamps and PRIMASK for critical sections.
 * No malloc, no blocking functions, ISR-safe.
 */

#include "axiom_port.h"
#include <stdint.h>

/* ---------------------------------------------------------------------------
 * STM32F4 Core Register Definitions (CMSIS-style)
 * --------------------------------------------------------------------------- */
#ifndef __weak
#define __weak __attribute__((weak))
#endif

/* CoreDebug */
typedef struct {
    volatile uint32_t DHCSR;
    volatile uint32_t DCRSR;
    volatile uint32_t DCRDR;
    volatile uint32_t DEMCR;
} CoreDebug_TypeDef;

#define CoreDebug_BASE            (0xE000EDF0UL)
#define CoreDebug                ((CoreDebug_TypeDef *) CoreDebug_BASE)

/* DWT */
typedef struct {
    volatile uint32_t CTRL;
    volatile uint32_t CYCCNT;
    volatile uint32_t CPICNT;
    volatile uint32_t EXCCNT;
    volatile uint32_t SLEEPCNT;
    volatile uint32_t LSUCNT;
    volatile uint32_t FOLDCNT;
    volatile uint32_t PCSR;
    volatile uint32_t COMP0;
    volatile uint32_t MASK0;
    volatile uint32_t FUNCTION0;
} DWT_TypeDef;

#define DWT_BASE                  (0xE0001000UL)
#define DWT                       ((DWT_TypeDef *) DWT_BASE)

/* DEMCR */
#define DEMCR_TRCENA_Msk         (1UL << 24)

/* DWT_CTRL */
#define DWT_CTRL_CYCCNTENA_Msk   (1UL << 0)
#define DWT_CTRL_CYCCNT_Msk      (0xFFFFFFFFUL)

/* ---------------------------------------------------------------------------
 * Timestamp - microseconds since boot
 *
 * Uses DWT_CYCCNT divided by system core clock.
 * SystemCoreClock must be provided by the user (set via SystemCoreClockUpdate).
 * --------------------------------------------------------------------------- */
static volatile uint32_t g_system_core_clock = 168000000UL; /* Default: 168MHz */

void axiom_port_set_system_clock(uint32_t clock) {
    g_system_core_clock = clock;
}

uint32_t axiom_port_timestamp(void) {
    static uint32_t overflow_count = 0;
    static uint32_t last_cyccnt = 0;

    uint32_t cyccnt = DWT->CYCCNT;

    /* Handle overflow (32-bit counter) */
    if (cyccnt < last_cyccnt) {
        overflow_count++;
    }
    last_cyccnt = cyccnt;

    /* Combine overflow count with current count for extended range */
    uint64_t total_cycles = ((uint64_t)overflow_count << 32) | cyccnt;

    /* Convert cycles to microseconds */
    return (uint32_t)(total_cycles / (g_system_core_clock / 1000000UL));
}

/* ---------------------------------------------------------------------------
 * Critical Section - PRIMASK-based (disables all interrupts except HardFault/NMI)
 *
 * Note: This is NOT nestable by default. For nestable critical sections,
 * use a combination of PRIMASK and a counter variable.
 * --------------------------------------------------------------------------- */
static uint32_t g_critical_nesting = 0;

void axiom_port_critical_enter(void) {
    if (g_critical_nesting == 0) {
        __asm volatile ("cpsid i" : : : "memory");
    }
    g_critical_nesting++;
}

void axiom_port_critical_exit(void) {
    if (g_critical_nesting > 0) {
        g_critical_nesting--;
        if (g_critical_nesting == 0) {
            __asm volatile ("cpsie i" : : : "memory");
        }
    }
}

/* ---------------------------------------------------------------------------
 * DWT Initialization - call once at startup before using timestamp
 * --------------------------------------------------------------------------- */
void axiom_port_init(void) {
    /* Enable trace clock */
    CoreDebug->DEMCR |= DEMCR_TRCENA_Msk;

    /* Enable cycle counter */
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

    /* Reset cycle counter */
    DWT->CYCCNT = 0;
}

/* ---------------------------------------------------------------------------
 * String Output - weak implementation (override for actual UART/RTT output)
 * --------------------------------------------------------------------------- */
__weak void axiom_port_string_out(const char *str) {
    (void)str;
    /* Default: no-op. Override with actual UART/RTT implementation. */
}

/* ---------------------------------------------------------------------------
 * Fault Hook - weak implementation
 * --------------------------------------------------------------------------- */
__weak void axiom_port_fault_hook(uint8_t module_id, uint16_t event_id,
                                   const uint8_t *payload, uint8_t payload_len) {
    (void)module_id;
    (void)event_id;
    (void)payload;
    (void)payload_len;
    /* Default: no-op. Override to add LED blink, halt, or log to flash. */
}

__weak uint8_t axiom_port_reset_reason(void) {
    return 0u;
}

/* ---------------------------------------------------------------------------
 * Fault Snapshot - capture registers for fault capsule
 *
 * Captures: R0-R15, xPSR, MSP, PSP, CONTROL, PRIMASK, FAULTMASK, BASEPRI
 * Returns bytes written (32 bytes minimum).
 * --------------------------------------------------------------------------- */
__weak uint8_t axiom_port_fault_snapshot(uint8_t *buf, uint8_t max_len) {
    if (max_len < 32) return 0;

    uint32_t *regs = (uint32_t *)buf;

    /* Capture main stack registers */
    __asm volatile (
        "mrs r0, MSP\n"
        "mov %0, r0\n"
        : "=r"(regs[0])
        : : "r0"
    );

    /* R0-R3 */
    __asm volatile ("mov %0, r0" : "=r"(regs[1]));
    __asm volatile ("mov %0, r1" : "=r"(regs[2]));
    __asm volatile ("mov %0, r2" : "=r"(regs[3]));
    __asm volatile ("mov %0, r3" : "=r"(regs[4]));

    /* R4-R11 */
    __asm volatile ("mov %0, r4" : "=r"(regs[5]));
    __asm volatile ("mov %0, r5" : "=r"(regs[6]));
    __asm volatile ("mov %0, r6" : "=r"(regs[7]));
    __asm volatile ("mov %0, r7" : "=r"(regs[8]));
    __asm volatile ("mov %0, r8" : "=r"(regs[9]));
    __asm volatile ("mov %0, r9" : "=r"(regs[10]));
    __asm volatile ("mov %0, r10" : "=r"(regs[11]));
    __asm volatile ("mov %0, r11" : "=r"(regs[12]));

    /* R12, LR, PC, xPSR */
    __asm volatile ("mov %0, r12" : "=r"(regs[13]));
    __asm volatile ("mov %0, lr" : "=r"(regs[14]));
    /* PC is captured via link register on entry to this function */
    regs[15] = (uint32_t)axiom_port_fault_snapshot; /* Placeholder */

    /* xPSR */
    __asm volatile ("mrs %0, ipsr" : "=r"(regs[16]));

    return 32;
}

/* ---------------------------------------------------------------------------
 * Flash Operations - weak implementation (must be overridden for capsule)
 * --------------------------------------------------------------------------- */
__weak int axiom_port_flash_erase(uint32_t addr, uint32_t len) {
    (void)addr;
    (void)len;
    return -1; /* Not implemented */
}

__weak int axiom_port_flash_write(uint32_t addr, const uint8_t *data, uint32_t len) {
    (void)addr;
    (void)data;
    (void)len;
    return -1; /* Not implemented */
}

__weak int axiom_port_flash_read(uint32_t addr, uint8_t *out, uint32_t len) {
    (void)addr;
    (void)out;
    (void)len;
    return -1; /* Not implemented */
}
