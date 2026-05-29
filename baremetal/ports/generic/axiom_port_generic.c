#include "axiom_port.h"

/* ============================================================================
 * Generic host/default port implementations
 * ============================================================================
 *
 * On ELF targets (Linux, most embedded GCC toolchains), these symbols are
 * marked weak so the user can override them by providing strong-symbol
 * implementations in another translation unit.
 *
 * On Windows/PE targets (MinGW, MSVC), weak symbol support is limited or
 * non-existent. We therefore provide regular (strong) defaults.  If you need
 * to override on Windows, either:
 *   1. Edit this file directly, or
 *   2. Provide an object file with the same symbols **before** this one in
 *      the link command.
 * ============================================================================ */

#if (defined(__GNUC__) || defined(__clang__)) && !defined(_WIN32)
#define AXIOM_WEAK __attribute__((weak))
#else
#define AXIOM_WEAK
#endif

#if defined(AXIOM_HOST_TESTING)
#include <string.h>

uint32_t g_axiom_port_simulated_time = 0u;
uint8_t g_axiom_port_reset_reason = 0u;
uint8_t g_axiom_port_fault_snapshot_data[64];
uint8_t g_axiom_port_fault_snapshot_len = 0u;
uint8_t g_axiom_port_flash_storage[AXIOM_HOST_FLASH_SIZE];
uint32_t g_axiom_port_flash_erase_calls = 0u;
uint32_t g_axiom_port_flash_write_calls = 0u;
uint32_t g_axiom_port_flash_read_calls = 0u;
uint32_t g_axiom_port_flash_write_limit = AXIOM_HOST_FLASH_SIZE;
int g_axiom_port_flash_fail_erase = 0;
int g_axiom_port_flash_fail_write = 0;
int g_axiom_port_flash_fail_read = 0;

AXIOM_WEAK uint32_t axiom_port_timestamp(void) { return g_axiom_port_simulated_time; }
#else
AXIOM_WEAK uint32_t axiom_port_timestamp(void) { return 0u; }
#endif

AXIOM_WEAK void axiom_port_critical_enter(void) { }

AXIOM_WEAK void axiom_port_critical_exit(void) { }

AXIOM_WEAK void axiom_port_string_out(const char *str) {
    (void)str;
}

AXIOM_WEAK void axiom_port_fault_hook(uint8_t module_id, uint16_t event_id,
                                       const uint8_t *payload, uint8_t payload_len) {
    (void)module_id; (void)event_id; (void)payload; (void)payload_len;
}

AXIOM_WEAK uint8_t axiom_port_reset_reason(void) {
#if defined(AXIOM_HOST_TESTING)
    return g_axiom_port_reset_reason;
#else
    return 0u;
#endif
}

AXIOM_WEAK uint8_t axiom_port_fault_snapshot(uint8_t *buf, uint8_t max_len) {
#if defined(AXIOM_HOST_TESTING)
    if (!buf || max_len == 0u) {
        return 0u;
    }
    uint8_t n = g_axiom_port_fault_snapshot_len < max_len ? g_axiom_port_fault_snapshot_len : max_len;
    memcpy(buf, g_axiom_port_fault_snapshot_data, n);
    return n;
#else
    (void)buf; (void)max_len;
    return 0;
#endif
}

AXIOM_WEAK int axiom_port_flash_erase(uint32_t addr, uint32_t len) {
#if defined(AXIOM_HOST_TESTING)
    g_axiom_port_flash_erase_calls++;
    if (g_axiom_port_flash_fail_erase || addr > AXIOM_HOST_FLASH_SIZE || len > AXIOM_HOST_FLASH_SIZE - addr) {
        return -1;
    }
    memset(g_axiom_port_flash_storage + addr, 0xFF, len);
    return 0;
#else
    (void)addr; (void)len;
    return -1;
#endif
}

AXIOM_WEAK int axiom_port_flash_write(uint32_t addr, const uint8_t *data, uint32_t len) {
#if defined(AXIOM_HOST_TESTING)
    g_axiom_port_flash_write_calls++;
    if (g_axiom_port_flash_fail_write || !data ||
        addr > AXIOM_HOST_FLASH_SIZE || len > AXIOM_HOST_FLASH_SIZE - addr) {
        return -1;
    }
    uint32_t writable = len;
    if (writable > g_axiom_port_flash_write_limit) {
        writable = g_axiom_port_flash_write_limit;
    }
    memcpy(g_axiom_port_flash_storage + addr, data, writable);
    if (writable != len) {
        return -1;
    }
    return 0;
#else
    (void)addr; (void)data; (void)len;
    return -1;
#endif
}

AXIOM_WEAK int axiom_port_flash_read(uint32_t addr, uint8_t *out, uint32_t len) {
#if defined(AXIOM_HOST_TESTING)
    g_axiom_port_flash_read_calls++;
    if (g_axiom_port_flash_fail_read || !out ||
        addr > AXIOM_HOST_FLASH_SIZE || len > AXIOM_HOST_FLASH_SIZE - addr) {
        return -1;
    }
    memcpy(out, g_axiom_port_flash_storage + addr, len);
    return 0;
#else
    (void)addr; (void)out; (void)len;
    return -1;
#endif
}

#if defined(AXIOM_HOST_TESTING)
void axiom_port_host_flash_reset(void) {
    memset(g_axiom_port_flash_storage, 0xFF, sizeof(g_axiom_port_flash_storage));
    memset(g_axiom_port_fault_snapshot_data, 0, sizeof(g_axiom_port_fault_snapshot_data));
    g_axiom_port_reset_reason = 0u;
    g_axiom_port_fault_snapshot_len = 0u;
    g_axiom_port_flash_erase_calls = 0u;
    g_axiom_port_flash_write_calls = 0u;
    g_axiom_port_flash_read_calls = 0u;
    g_axiom_port_flash_write_limit = AXIOM_HOST_FLASH_SIZE;
    g_axiom_port_flash_fail_erase = 0;
    g_axiom_port_flash_fail_write = 0;
    g_axiom_port_flash_fail_read = 0;
}
#endif
