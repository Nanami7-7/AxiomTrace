#ifndef AXIOM_BACKEND_H
#define AXIOM_BACKEND_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------------------------------------------------------------------
 * Backend Capability Flags
 * Used by the backend registry to advertise transport capabilities.
 * Query via backend.caps at registration or discovery time.
 * --------------------------------------------------------------------------- */
#define AXIOM_BACKEND_CAP_MEMORY  (1u << 0) /* In-memory ring capture           */
#define AXIOM_BACKEND_CAP_UART    (1u << 1) /* Serial UART transport            */
#define AXIOM_BACKEND_CAP_USB     (1u << 2) /* USB CDC / bulk transport          */
#define AXIOM_BACKEND_CAP_RTT     (1u << 3) /* SEGGER RTT transport             */
#define AXIOM_BACKEND_CAP_SWO     (1u << 4) /* ARM SWO stimulus port            */
#define AXIOM_BACKEND_CAP_CANFD   (1u << 5) /* CAN-FD transport                 */
#define AXIOM_BACKEND_CAP_CAPSULE (1u << 6) /* Fault capsule (flash-persistent) */

/* ---------------------------------------------------------------------------
 * Backend Contract
 * The first field `size` enables forward-compatible struct evolution:
 * - Source-compatible zero-initialized descriptors have size==0.
 * - New backends set size via AXIOM_BACKEND_INIT() so the registry can
 *   detect the layout version and safely access fields added in future minor
 *   releases after recompilation. This is not a binary-layout guarantee.
 * --------------------------------------------------------------------------- */
typedef struct {
    uint16_t size;              /* sizeof(this struct) at compile time */
    const char *name;
    uint32_t caps;

    int  (*write)(const uint8_t *buf, uint16_t len, void *ctx);
    int  (*ready)(void *ctx);
    int  (*flush)(void *ctx);
    int  (*panic_write)(const uint8_t *buf, uint16_t len, void *ctx);

    void (*on_drop)(uint32_t lost, void *ctx);
    void *ctx;
} axiom_backend_t;

/* Designated-initializer wrapper: fills size automatically.
 * Example: static const axiom_backend_t my_be = AXIOM_BACKEND_INIT(
 *     .name = "uart", .write = uart_write, .ctx = &uart_ctx); */
#define AXIOM_BACKEND_INIT(...)  { .size = sizeof(axiom_backend_t), __VA_ARGS__ }

typedef struct {
    uint8_t *buf;
    uint32_t size;
    uint32_t head;
} axiom_memory_backend_ctx_t;

axiom_backend_t axiom_backend_memory(const char *name, uint8_t *buf, uint32_t size,
                                     axiom_memory_backend_ctx_t *ctx);

/* ---------------------------------------------------------------------------
 * Backend Registry — Error Codes
 * --------------------------------------------------------------------------- */
typedef enum {
    AXIOM_BACKEND_OK         =  0,
    AXIOM_BACKEND_ERR_NULL   = -1,  /* backend or write callback is NULL */
    AXIOM_BACKEND_ERR_FULL   = -2,  /* registry full (>= AXIOM_BACKEND_MAX) */
    AXIOM_BACKEND_ERR_STRUCT = -3,  /* struct size mismatch — recompile backend */
} axiom_backend_err_t;

#ifndef AXIOM_BACKEND_MAX
#define AXIOM_BACKEND_MAX 4
#endif

/* ---------------------------------------------------------------------------
 * Backend Degradation & Recovery
 *
 * After N consecutive write failures, a backend is temporarily disabled
 * (soft-disabled) for AXIOM_BACKEND_RECOVERY_US microseconds. This
 * prevents a broken UART/USB from blocking other backends indefinitely.
 * Auto-recovery is attempted during axiom_backend_dispatch().
 *
 * Enable via AXIOM_BACKEND_DEGRADATION=1 (default).
 * --------------------------------------------------------------------------- */
#ifndef AXIOM_BACKEND_DEGRADATION
#define AXIOM_BACKEND_DEGRADATION 1
#endif

#if AXIOM_BACKEND_DEGRADATION
    #ifndef AXIOM_BACKEND_FAIL_THRESHOLD
    #define AXIOM_BACKEND_FAIL_THRESHOLD 5
    #endif
    #ifndef AXIOM_BACKEND_RECOVERY_US
    #define AXIOM_BACKEND_RECOVERY_US 1000000u /* 1 second */
    #endif
#endif

/* Register a backend. Returns AXIOM_BACKEND_OK on success, negative on error.
 * MUST be called before any axiom_write() — not thread-safe. */
int axiom_backend_register(const axiom_backend_t *backend);

/* Dispatch a frame to all registered backends.
 * Called internally by axiom_flush() after ring buffer peek+consume.
 * Includes auto-recovery of soft-disabled backends. */
void axiom_backend_dispatch(const uint8_t *frame, uint16_t len);

/* Flush all backends that provide a flush callback. */
void axiom_backend_flush_all(void);

/* Panic write to all backends that provide panic_write.
 * Bypasses degradation — panic always attempts all backends. */
void axiom_backend_panic_write(const uint8_t *frame, uint16_t len);

/* Query per-backend health status (for diagnostics / self-report).
 * Returns number of currently active (non-disabled) backends. */
uint8_t axiom_backend_active_count(void);

/* Query total number of registered backends.
 * Returns count regardless of degradation state. */
uint8_t axiom_backend_count(void);

#ifdef __cplusplus
}
#endif

#endif /* AXIOM_BACKEND_H */
