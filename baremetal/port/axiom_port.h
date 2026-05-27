#ifndef AXIOM_PORT_H
#define AXIOM_PORT_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------------------------------------------------------------------
 * Port layer — override by providing strong-symbol implementations.
 * Default weak implementations are in axiom_port_generic.c.
 * --------------------------------------------------------------------------- */

/* Monotonic timestamp in microseconds (wraps naturally) */
extern uint32_t axiom_port_timestamp(void);

#if defined(AXIOM_HOST_TESTING)
/* Mutable generic-port time source used only by host regression tests. */
extern uint32_t g_axiom_port_simulated_time;
#endif

/* Critical section: must be nestable or simply disable/enable IRQs */
extern void axiom_port_critical_enter(void);
extern void axiom_port_critical_exit(void);

/* String output for development logs (AX_LOG). May be UART/RTT/stdout. */
extern void axiom_port_string_out(const char *str);

/* Fault hook: called on AX_FAULT events. Override to customize behavior. */
extern void axiom_port_fault_hook(uint8_t module_id, uint16_t event_id,
                                   const uint8_t *payload, uint8_t payload_len);

/* Register snapshot for fault capsule. Returns bytes written. */
extern uint8_t axiom_port_fault_snapshot(uint8_t *buf, uint8_t max_len);

/* Flash operations for capsule backend (non-ISR only) */
extern int axiom_port_flash_erase(uint32_t addr, uint32_t len);
extern int axiom_port_flash_write(uint32_t addr, const uint8_t *data, uint32_t len);

#ifdef __cplusplus
}
#endif

#endif /* AXIOM_PORT_H */
