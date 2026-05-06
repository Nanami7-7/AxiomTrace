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
 * - Old backends (zero-initialized) have size==0, treated as legacy v1 layout.
 * - New backends set size via AXIOM_BACKEND_INIT() so the registry can
 *   detect the layout version and safely access fields added in future minor
 *   releases without breaking ABI.
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

/* ---------------------------------------------------------------------------
 * Backend Registry
 * --------------------------------------------------------------------------- */

#ifndef AXIOM_BACKEND_MAX
#define AXIOM_BACKEND_MAX 4
#endif

/* Register a backend. Returns 0 on success, negative on error.
 * MUST be called before any axiom_write() — not thread-safe. */
int axiom_backend_register(const axiom_backend_t *backend);

/* Dispatch a frame to all registered backends.
 * Called internally by axiom_write() after ring buffer insertion. */
void axiom_backend_dispatch(const uint8_t *frame, uint16_t len);

/* Flush all backends that provide a flush callback. */
void axiom_backend_flush_all(void);

/* Panic write to all backends that provide panic_write. */
void axiom_backend_panic_write(const uint8_t *frame, uint16_t len);

#ifdef __cplusplus
}
#endif

#endif /* AXIOM_BACKEND_H */
