/* ============================================================================
 * AxiomTrace Full Example
 * ============================================================================
 * Demonstrates: init, backend registration with new Backend API,
 * filter, multiple event types, and flush pattern.
 *
 * Build with CMake (recommended):
 *   cmake -B build -DAXIOM_PLATFORM=host
 *   cmake --build build --target example_full
 * ============================================================================ */

#include "axiomtrace.h"
#include <stdio.h>

/* Memory buffer for capturing events (host demonstration) */
static uint8_t g_memory_buf[256];
static axiom_memory_backend_ctx_t g_memory_ctx;

/* Simple stdout backend for real-time output */
static int stdout_write(const uint8_t *buf, uint16_t len, void *ctx) {
    (void)ctx;
    printf("[FRAME %u bytes] ", len);
    for (uint16_t i = 0; i < len; ++i) {
        printf("%02X ", buf[i]);
    }
    printf("\n");
    return 0;
}

static int stdout_ready(void *ctx) {
    (void)ctx;
    return 1;
}

int main(void) {
    axiom_init();

    /* 使用 AXIOM_BACKEND_INIT 宏初始化后端，自动填充 size 字段
     * 确保前向兼容的结构体演进机制正常工作 */
    axiom_backend_t memory_be = axiom_backend_memory(
        "memory", g_memory_buf, sizeof(g_memory_buf), &g_memory_ctx);
    axiom_backend_register(&memory_be);

    axiom_backend_t stdout_be = AXIOM_BACKEND_INIT(
        .name = "stdout",
        .write = stdout_write,
        .ready = stdout_ready,
    );
    axiom_backend_register(&stdout_be);

    /* Normal events */
    AX_EVT(INFO,  0x01, 0x01, (uint16_t)100);
    AX_EVT(WARN,  0x01, 0x02, (uint32_t)0xDEADBEEF);
    AX_EVT(ERROR, 0x02, 0x01, (int16_t)(-123), (float)3.14f);

    /* Key-Value event (hashed keys) */
    AX_KV(INFO, 0x03, 0x01, "temp", (int16_t)850, "humidity", (uint16_t)65);

    /* Development log (no-op in PROD profile) */
    AX_LOG("System initialized");

    /* Probe (no-op in PROD profile) */
    AX_PROBE("adc", (uint16_t)4095);

    /* Flush to ensure all events are dispatched */
    axiom_flush();

    printf("Full example complete.\n");
    return 0;
}
