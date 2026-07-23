#include "axiomtrace.h"

uint32_t axiom_port_timestamp(void) { return 1u; }
void axiom_port_critical_enter(void) { }
void axiom_port_critical_exit(void) { }
void axiom_port_string_out(const char *str) { (void)str; }
void axiom_port_fault_hook(uint8_t module_id, uint16_t event_id,
                           const uint8_t *payload, uint8_t payload_len) {
    (void)module_id;
    (void)event_id;
    (void)payload;
    (void)payload_len;
}
uint8_t axiom_port_reset_reason(void) { return 0u; }
uint8_t axiom_port_fault_snapshot(uint8_t *buf, uint8_t max_len) {
    (void)buf;
    (void)max_len;
    return 0u;
}
int axiom_port_flash_erase(uint32_t addr, uint32_t len) {
    (void)addr;
    (void)len;
    return -1;
}
int axiom_port_flash_write(uint32_t addr, const uint8_t *data, uint32_t len) {
    (void)addr;
    (void)data;
    (void)len;
    return -1;
}
int axiom_port_flash_read(uint32_t addr, uint8_t *out, uint32_t len) {
    (void)addr;
    (void)out;
    (void)len;
    return -1;
}
