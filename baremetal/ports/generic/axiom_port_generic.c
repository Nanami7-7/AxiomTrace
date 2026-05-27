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
uint32_t g_axiom_port_simulated_time = 0u;
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

AXIOM_WEAK uint8_t axiom_port_fault_snapshot(uint8_t *buf, uint8_t max_len) {
    (void)buf; (void)max_len;
    return 0;
}

AXIOM_WEAK int axiom_port_flash_erase(uint32_t addr, uint32_t len) {
    (void)addr; (void)len;
    return -1;
}

AXIOM_WEAK int axiom_port_flash_write(uint32_t addr, const uint8_t *data, uint32_t len) {
    (void)addr; (void)data; (void)len;
    return -1;
}
