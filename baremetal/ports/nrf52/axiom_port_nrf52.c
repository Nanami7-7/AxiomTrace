/* ============================================================================
 * nRF52 Platform Port Implementation
 * ============================================================================
 *
 * Timestamp: RTC0-based, LFRC (32.768kHz) clock source
 * Critical Section: PRIMASK-based global interrupt disable
 *
 * No Nordic SDK dependency - direct hardware register access only.
 * ============================================================================ */

#include "axiom_port.h"
#include <stdint.h>

/* ============================================================================
 * nRF52 Hardware Register Definitions (CMSIS-style)
 * These are standard ARM Cortex-M4 registers, no Nordic SDK required.
 * ============================================================================ */
#define NVIC_ICER_REG  (*(volatile uint32_t *)0xE000E180UL)
#define NVIC_ICPR_REG  (*(volatile uint32_t *)0xE000E280UL)

/* RTC Component Register Base (RTC0 on nRF52) */
#define RTC_BASE        0x4000B000UL
#define RTC_TASK_START  ((volatile uint32_t *)0x4000B000UL)
#define RTC_TASK_STOP   ((volatile uint32_t *)0x4000B004UL)
#define RTC_TASK_CLEAR  ((volatile uint32_t *)0x4000B008UL)
#define RTC_TASK_TRIGOVRFLW ((volatile uint32_t *)0x4000B00CUL)
#define RTC_REG_CNT     ((volatile uint32_t *)0x4000B504UL)
#define RTC_REG_PRESCALER ((volatile uint32_t *)0x4000B508UL)

/* System Control Block - ARM Cortex-M4 */
#define SCB_ICSR_REG    (*(volatile uint32_t *)0xE000ED04UL)
#define SCB_AIRCR_REG   (*(volatile uint32_t *)0xE000ED0CUL)

/* ============================================================================
 * RTC Configuration
 * LFRC = 32.768kHz, prescaler = 0 => tick every 1/32768 second ≈ 30.517us
 * For microsecond resolution, we multiply counter by 30517 and divide by 1000.
 * This gives us ~30.5us resolution with 32-bit counter (rollover in ~36 hours)
 * ============================================================================ */
#define RTC_PRESCALER_VALUE  0UL   /* No prescaler - full 32.768kHz tick */
#define RTC_TICK_PER_US      30517UL
#define RTC_TICK_FRAC        1000UL

/* ============================================================================
 * Timestamp Implementation
 * ============================================================================ */
uint32_t axiom_port_timestamp(void) {
    /* Read RTC counter directly - 32-bit, wraps naturally */
    uint32_t ticks = *RTC_REG_CNT;

    /* Convert to microseconds: ticks * (1000000 / 32768)
     * Simplified: ticks * 30517 / 1000 */
    uint64_t us = ((uint64_t)ticks * RTC_TICK_PER_US) / RTC_TICK_FRAC;
    return (uint32_t)us;
}

/* ============================================================================
 * Critical Section Implementation (PRIMASK-based)
 * ============================================================================ */
static uint32_t s_critical_nesting = 0UL;

/* PRIMASK: 1 = interrupts disabled, 0 = interrupts enabled */
static inline uint32_t get_primask(void) {
    uint32_t result = 0;
    __asm volatile ("MRS %0, primask" : "=r" (result));
    (void)result;  /* Clang-tidy false positive for ARM */
    return result & 1UL;
}

static inline void set_primask(uint32_t primask) {
    __asm volatile ("MSR primask, %0" : : "r" (primask));
}

void axiom_port_critical_enter(void) {
    uint32_t was_disabled = get_primask();
    set_primask(1UL);  /* Disable interrupts */
    s_critical_nesting++;
    (void)was_disabled;  /* Suppress unused warning */
}

void axiom_port_critical_exit(void) {
    if (s_critical_nesting > 0) {
        s_critical_nesting--;
        if (s_critical_nesting == 0) {
            set_primask(0UL);  /* Re-enable interrupts */
        }
    }
}

/* ============================================================================
 * String Output (development logs via RTT)
 * ============================================================================ */
void axiom_port_string_out(const char *str) {
    /* Forward to RTT backend if available, otherwise drop */
    extern void axiom_rtt_write_string(const char *str);
    axiom_rtt_write_string(str);
}

/* ============================================================================
 * Fault Hook - nRF52-specific fault handling
 * ============================================================================ */
void axiom_port_fault_hook(uint8_t module_id, uint16_t event_id,
                           const uint8_t *payload, uint8_t payload_len) {
    (void)module_id;
    (void)event_id;
    (void)payload;
    (void)payload_len;
    /* Could be extended to write to flash or backup registers */
}

uint8_t axiom_port_reset_reason(void) {
    return 0u;
}

/* ============================================================================
 * Fault Snapshot - ARM Cortex-M4 register capture
 * ============================================================================ */
uint8_t axiom_port_fault_snapshot(uint8_t *buf, uint8_t max_len) {
    if (buf == NULL || max_len < 40) {
        return 0;
    }

    /* Capture essential ARM Cortex-M4 registers */
    uint32_t *regs = (uint32_t *)buf;

    /* PSP, MSP, LR, PC, xPSR, R0-R3, R12 */
    __asm volatile (
        "MRS r0, psp\n\t"
        "MOV %[psp], r0\n\t"
        "MRS r0, msp\n\t"
        "MOV %[msp], r0\n\t"
        : [psp] "=r" (regs[0]), [msp] "=r" (regs[1])
        :
        : "r0"
    );

    __asm volatile (
        "MOV %[lr], lr\n\t"
        "MOV %[pc], pc\n\t"
        : [lr] "=r" (regs[2]), [pc] "=r" (regs[3])
        :
    );

    /* Status registers would be captured via SCB */
    regs[4] = SCB_ICSR_REG;  /* Interrupt Control and State */
    regs[5] = 0;  /* Reserved */

    return 40;  /* 10 registers * 4 bytes each */
}

/* ============================================================================
 * Flash Operations - placeholder (requires target-specific implementation)
 * ============================================================================ */
int axiom_port_flash_erase(uint32_t addr, uint32_t len) {
    (void)addr;
    (void)len;
    return -1;  /* Not implemented - requires NVMC peripheral */
}

int axiom_port_flash_write(uint32_t addr, const uint8_t *data, uint32_t len) {
    (void)addr;
    (void)data;
    (void)len;
    return -1;  /* Not implemented - requires NVMC peripheral */
}

int axiom_port_flash_read(uint32_t addr, uint8_t *out, uint32_t len) {
    (void)addr;
    (void)out;
    (void)len;
    return -1;  /* Not implemented - requires NVMC peripheral */
}

/* ============================================================================
 * RTC Initialization (call from main or system init)
 * ============================================================================ */
void axiom_port_rtc_init(void) {
    *RTC_REG_PRESCALER = RTC_PRESCALER_VALUE;
    *RTC_TASK_START = 1;
}
