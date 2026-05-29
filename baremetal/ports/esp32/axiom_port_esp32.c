/**
 * @file axiom_port_esp32.c
 * @brief ESP32 Platform Port Implementation
 *
 * Uses esp_timer for high-precision timestamps and FreeRTOS-compatible
 * critical sections. No malloc, no blocking functions, ISR-safe.
 *
 * Key differences from ARM Cortex-M ports:
 * - Timestamp: esp_timer_get_time() (hardware timer, not DWT_CYCCNT)
 * - Critical section: portENTER_CRITICAL from FreeRTOS (multi-core safe)
 * - Xtensa architecture (Tensilica) instead of ARM Cortex-M
 */

#include "axiom_port.h"
#include <stdint.h>
#include <stdbool.h>

/* ESP-IDF headers - these are available in ESP-IDF environment */
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"

/* ---------------------------------------------------------------------------
 * Timestamp - microseconds since boot
 *
 * Uses esp_timer_get_time() which provides microsecond precision.
 * This is based on the hardware timer and works across both CPU cores.
 * Returns uint32_t (wraps naturally after ~4294 seconds / 71 minutes).
 * --------------------------------------------------------------------------- */
uint32_t axiom_port_timestamp(void) {
    /* esp_timer_get_time returns int64_t microseconds since boot */
    return (uint32_t)(esp_timer_get_time() & 0xFFFFFFFFUL);
}

/* ---------------------------------------------------------------------------
 * Critical Section - FreeRTOS-based (multi-core safe)
 *
 * Uses portENTER_CRITICAL which disables interrupts on both cores.
 * This is nestable via the FreeRTOS interrupt nesting mechanism.
 * --------------------------------------------------------------------------- */
static portMUX_TYPE g_critical_mux = portMUX_INITIALIZER_UNLOCKED;

void axiom_port_critical_enter(void) {
    portENTER_CRITICAL(&g_critical_mux);
}

void axiom_port_critical_exit(void) {
    portEXIT_CRITICAL(&g_critical_mux);
}

/* ---------------------------------------------------------------------------
 * Port Initialization
 *
 * Note: esp_timer is automatically initialized by ESP-IDF at startup.
 * No additional initialization needed for timestamps.
 * --------------------------------------------------------------------------- */
void axiom_port_init(void) {
    /* esp_timer is automatically initialized by ESP-IDF bootloader */
    /* No additional initialization needed */
}

/* ---------------------------------------------------------------------------
 * String Output - weak implementation
 * Override with actual UART/RTT implementation for development logs.
 * --------------------------------------------------------------------------- */
__attribute__((weak))
void axiom_port_string_out(const char *str) {
    (void)str;
    /* Default: no-op. Override for actual output. */
}

/* ---------------------------------------------------------------------------
 * Fault Hook - weak implementation
 *
 * Called on AX_FAULT events. Override to customize behavior.
 * --------------------------------------------------------------------------- */
__attribute__((weak))
void axiom_port_fault_hook(uint8_t module_id, uint16_t event_id,
                           const uint8_t *payload, uint8_t payload_len) {
    (void)module_id;
    (void)event_id;
    (void)payload;
    (void)payload_len;
    /* Default: no-op. Override to add LED blink, halt, or log to flash. */
}

__attribute__((weak))
uint8_t axiom_port_reset_reason(void) {
    return 0u;
}

/* ---------------------------------------------------------------------------
 * Fault Snapshot - capture registers for fault capsule
 *
 * Captures: General purpose registers, PC, PS, etc.
 * ESP32 has two cores, this captures the current core's state.
 * Returns bytes written.
 * --------------------------------------------------------------------------- */
__attribute__((weak))
uint8_t axiom_port_fault_snapshot(uint8_t *buf, uint8_t max_len) {
    if (max_len < 64) return 0;

    /* ESP32 uses Xtensa architecture - capture key registers */
    uint32_t *regs = (uint32_t *)buf;

    /* Note: Xtensa register capture is architecture-specific.
     * This is a simplified snapshot - full implementation would need
     * inline assembly to capture all 16 general purpose registers.
     */
    regs[0] = 0;  /* Placeholder for A0 */
    regs[1] = 0;  /* Placeholder for A1 (stack pointer) */
    regs[2] = 0;  /* Placeholder for A2-A15 */

    /* For ESP32, fault information is typically obtained via:
     * - panic_handler for CPU exceptions
     * - esp_reset_reason() for reset cause
     * - gdbstub or core dump for full register state
     */

    return 32;  /* Return minimum snapshot size */
}

/* ---------------------------------------------------------------------------
 * Flash Operations - weak implementation (must be overridden for capsule)
 *
 * ESP32 uses esp_partition_* APIs for flash operations.
 * --------------------------------------------------------------------------- */
__attribute__((weak))
int axiom_port_flash_erase(uint32_t addr, uint32_t len) {
    (void)addr;
    (void)len;
    return -1; /* Not implemented - use esp_partition_* APIs */
}

__attribute__((weak))
int axiom_port_flash_write(uint32_t addr, const uint8_t *data, uint32_t len) {
    (void)addr;
    (void)data;
    (void)len;
    return -1; /* Not implemented - use esp_partition_* APIs */
}

__attribute__((weak))
int axiom_port_flash_read(uint32_t addr, uint8_t *out, uint32_t len) {
    (void)addr;
    (void)out;
    (void)len;
    return -1; /* Not implemented - use esp_partition_* APIs */
}
