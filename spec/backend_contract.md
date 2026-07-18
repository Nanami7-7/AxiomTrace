> [English](backend_contract.md) | [简体中文](backend_contract_zh.md)

# AxiomTrace Backend Contract Specification

> Version: v1.0.0 | Status: **FROZEN** | Ref: `baremetal/backend/axiom_backend.h`

---

## 1. Overview

Backends are the transport layer that moves encoded Event Records from the Core Ring Buffer to the outside world. All backends conform to a single contract; no backend may define a private protocol.

**Design goals**:

- **Zero intrusion**: Adding a new backend does not require modifying `core/` or `frontend/`.
- **User owns the driver**: AxiomTrace provides the contract and dispatcher; the user provides the actual peripheral driver functions.
- **Non-blocking**: Backends must not block indefinitely. If the transport is busy, return `-EAGAIN`.

---

## 2. Backend Descriptor

```c
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

/* Recommended: use AXIOM_BACKEND_INIT() for forward-compatible initialization */
#define AXIOM_BACKEND_INIT(...)  { .size = sizeof(axiom_backend_t), __VA_ARGS__ }
```

### 2.1 Field Description

| Field        | Type     | Required | Description |
|--------------|----------|----------|-------------|
| `size`       | `uint16_t` | Auto | Struct size at compile time. Filled automatically by `AXIOM_BACKEND_INIT()`. A descriptor zero-initialized with the current header may use `size==0`. |
| `name`       | `const char *` | Yes | Human-readable name for diagnostics |
| `caps`       | `uint32_t` | No | Capability flags (see §4) |
| `write`      | `int (*)(...)` | Yes | Transmit a complete frame. Must be IRQ-safe or called from critical section. Return 0 on success, `-EAGAIN` if busy, negative on error. |
| `ready`      | `int (*)(void*)` | No | Return non-zero if backend can accept data. Called before `write` to avoid unnecessary drop. |
| `flush`      | `int (*)(void*)` | No | Drain any internal buffers. Called explicitly by user or during shutdown. |
| `panic_write`| `int (*)(...)` | No | Fallback write used in fault/crash contexts. Must be as simple as possible (e.g., polled UART byte out). If NULL, `write` is used instead. |
| `on_drop`    | `void (*)(...)` | No | Callback invoked when Core drops a frame intended for this backend. `lost` = number of consecutively dropped frames. |
| `ctx`        | `void *` | No | User context pointer passed to all callbacks. |

### 2.2 Registration

```c
/* User code: register a backend at boot time */
static const axiom_backend_t my_uart = AXIOM_BACKEND_INIT(
    .name = "uart0",
    .write = my_uart_dma_send,
    .ready = my_uart_tx_ready,
    .ctx = &uart0_handle
);
axiom_backend_register(&my_uart);
```

**Rules**:

- Registration must occur before the first log call (typically in `main()` after peripheral init).
- Registration is **not** auto-discovered via linker sections (deliberately removed for portability).
- A maximum of `AXIOM_BACKEND_MAX` backends can be registered (default 4, compile-time configurable).
- `size==0` is source compatibility for code recompiled with the current header; it is not compatibility with an older binary struct layout.
- `axiom_backend_register()` returns `AXIOM_BACKEND_ERR_STRUCT` (`-3`) if the struct's `size` field is non-zero but smaller than `sizeof(axiom_backend_t)` — indicating the backend was compiled against an older header. Recompile the backend.

### 2.3 Backend Error Codes

```c
typedef enum {
    AXIOM_BACKEND_OK        =  0,   /* Success */
    AXIOM_BACKEND_ERR_NULL  = -1,   /* NULL pointer parameter */
    AXIOM_BACKEND_ERR_FULL  = -2,   /* Registration table full */
    AXIOM_BACKEND_ERR_STRUCT = -3   /* Struct size is too small for this library */
} axiom_backend_err_t;
```

The original integer return values (`0`, `-1`, `-2`, `-3`) are preserved for source-level backward compatibility.

---

## 3. Lifecycle

1. **Boot-time**: User creates `axiom_backend_t` instances and calls `axiom_backend_register()`.
2. **Runtime**: Core calls `axiom_backend_dispatch()` after writing a frame to the Ring.
   - For each registered backend, Core calls `ready()` (if non-NULL).
   - If ready, Core calls `write()` with the complete frame buffer.
   - If `write()` fails or `ready()` returns false, Core increments the public backend diagnostic and calls `on_drop()` (if non-NULL). This is distinct from Core ingress `DROP_SUMMARY` accounting.
3. **Fault-time**: If a fault occurs, Core attempts `panic_write()` on all backends that provide it.
4. **Flush**: Normal code calls `axiom_flush()` to drain the Core ring and then backend buffers. `axiom_backend_flush_all()` only invokes backend flush callbacks.

---

## 4. Capability Flags

```c
#define AXIOM_BACKEND_CAP_MEMORY  (1u << 0)  /* RAM/Flash direct dump */
#define AXIOM_BACKEND_CAP_UART    (1u << 1)  /* UART byte stream */
#define AXIOM_BACKEND_CAP_USB     (1u << 2)  /* USB CDC */
#define AXIOM_BACKEND_CAP_RTT     (1u << 3)  /* SEGGER RTT */
#define AXIOM_BACKEND_CAP_SWO     (1u << 4)  /* SWO/ITM */
#define AXIOM_BACKEND_CAP_CANFD   (1u << 5)  /* CAN-FD */
#define AXIOM_BACKEND_CAP_CAPSULE (1u << 6)  /* Flash fault capsule */
```

Caps flags are **informational** for tooling and filtering. Core does not enforce behavior based on caps.

---

## 5. Backend Templates

AxiomTrace provides **templates** for common backends. A template is a `.c` file that implements the `axiom_backend_t` interface except for the actual peripheral send function, which the user provides.

| Template File                | Transport | User Must Provide |
|------------------------------|-----------|-------------------|
| `axiom_backend_memory.c`     | Linear RAM capture | Buffer and context passed to `axiom_backend_memory()` |
| `axiom_backend_deferred.c`   | Local deferred ring | Downstream `axiom_backend_t` |
| `axiom_capsule.c`            | Flash capsule | `axiom_port_flash_erase()` / `write()` / `read()` |

Memory and Deferred are linked into the main library. No hardware transport backend is part of the v1.0 library; UART, RTT, USB CDC, SWO/ITM, and CAN-FD are integration work outside this contract.

Deferred `ready()` describes capacity in its local queue, not downstream availability. It accepts complete frames while the downstream is busy, uses DROP for its own ring regardless of the Core ring policy, and retains the oldest pending frame when downstream `ready()` is false or `write()` fails.

**Usage**: Add the template `.c` file to your build system. Implement the required user functions. Register the backend.

---

## 6. Constraints

- `write()` runs from the caller of `axiom_flush()` and is not called with the Core critical section held. ISR use is an integration choice, not a dispatcher guarantee.
- **`write()` must not block indefinitely**. If the transport is busy, return `-EAGAIN`. Blocking writes (e.g., waiting for DMA completion or UART TX FIFO drain) will stall all ISR-driven logging and may cause **priority inversion** or **watchdog resets** on safety-critical MCUs.
- Backends must not allocate memory dynamically.
- Backends must not modify the frame buffer passed to `write()`.
- `panic_write()` should avoid DMA, IRQ, or complex state machines. Polled byte output is preferred.
