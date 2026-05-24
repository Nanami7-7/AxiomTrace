> [English](api_reference.md) | [简体中文](api_reference_zh.md)

# AxiomTrace API Reference

> Version: v0.1 | Status: **Frozen (v0.1-frontend finalized)**

---

## 1. Overview

AxiomTrace provides a tiered API surface:

- **Layer 0 — Quick Start**: Hard-coded IDs, zero config, single-file library.
- **Layer 1 — Structured**: X-Macro event definitions, compile-time ID management.
- **Layer 2 — Team Scale**: YAML dictionary + Python codegen, full semantic control.

All layers compile to the same binary frame; the difference is only in developer experience and type safety.

---

## 2. Frontend Macros

### 2.1 AX_LOG — Development Logging

```c
AX_LOG("Boot complete, version=%u", 1u);
AX_LOG_DEBUG("Sensor temp=%d", (int16_t)25);
AX_LOG_INFO("System ready");
AX_LOG_WARN("Battery low, level=%u", 15u);
AX_LOG_ERROR("Comm timeout, retries=%u", 3u);
```

**Semantics**:

- Human-readable text for development and field debugging.
- In `PROD` profile, all `AX_LOG*` macros expand to nothing (zero code/data cost).
- Format strings are **not** transmitted in the binary frame; they are resolved by the host decoder using the dictionary or ELF section extraction.
- Arguments are encoded with `_Generic` type dispatch into the binary payload.

**Profile behavior**:

| Profile | `AX_LOG` | `AX_LOG_*` |
|---------|----------|------------|
| `DEV`   | Active   | Active     |
| `FIELD` | Active   | Active     |
| `PROD`  | **No-op**| **No-op**  |

---

## 3. AX_EVT — Structured Event

```c
AX_EVT(INFO, MOTOR, START, (uint16_t)3200);
AX_EVT(WARN, SENSOR, TEMP_HIGH, (uint16_t)adc_val, (int16_t)limit);
AX_EVT(ERROR, COM, TIMEOUT, (uint8_t)bus_id, (uint32_t)ms);
```

**Semantics**:

- The primary production API. Always compiled, regardless of profile.
- `MOTOR`, `START` are module/event names resolved to `module_id` / `event_id` by X-Macro or codegen.
- Arguments are type-safe via `_Generic`; mismatches cause compile errors.
- **Direct-to-Ring (D2R)**: Encoding happens directly in the ring buffer, ensuring zero-copy and minimal stack overhead.
- Binary frame contains only IDs and typed payload — no strings.

**Requirements**:

- Layer 1/2: Must define `AXIOM_MODULE_LIST` and `AXIOM_EVENT_LIST` before including `axiomtrace.h`.
- Layer 0: Use raw IDs directly: `AX_EVT_RAW(INFO, 0x03, 0x01, (uint16_t)3200)`.

---

## 4. AX_PROBE — Timing Probe

```c
AX_PROBE("pwm_duty", (uint16_t)duty);
AX_PROBE("adc_sample", (uint16_t)adc);
```

**Semantics**:

- High-frequency, low-overhead signal tracing.
- Uses a separate Probe Ring to avoid flooding the main event log.
- Minimal header (optional: no CRC, no module_id) for maximum throughput.
- Recommended backend: SWO/ITM.

**Profile behavior**:

| Profile | `AX_PROBE` |
|---------|------------|
| `DEV`   | Active     |
| `FIELD` | Active     |
| `PROD`  | **No-op**  |

---

## 5. AX_FAULT — Fault Event

```c
AX_FAULT(MOTOR, OVERCURRENT, (uint8_t)phase, (int16_t)current);
```

**Semantics**:

- Critical fault logging. Always compiled, regardless of profile.
- Triggers `axiom_port_fault_hook()`.
- Initiates Fault Capsule freeze (pre-window + post-window capture).
- Level is implicitly `AXIOM_LEVEL_FAULT`.

---

## 6. AX_KV — Key-Value Event

```c
AX_KV(INFO, MOTOR, START, "rpm", (uint16_t)3200);
AX_KV(ERROR, COM, TIMEOUT, "bus", (uint8_t)1, "ms", (uint32_t)50);
```

**Semantics**:

- Lightweight structured logging with named parameters.
- Key names are hashed at compile time to 16-bit IDs; keys themselves are not transmitted.
- Payload format: `[key_hash_1 (2B)] [value_1 (typed)] [key_hash_2 (2B)] [value_2 (typed)] ...`
- Useful when event semantics are stable but parameter names aid decoder readability.

---

## 7. Profile Control

```c
#define AXIOM_PROFILE_DEV    0
#define AXIOM_PROFILE_FIELD  1
#define AXIOM_PROFILE_PROD   2

#ifndef AXIOM_PROFILE
#define AXIOM_PROFILE AXIOM_PROFILE_DEV
#endif
```

Set `AXIOM_PROFILE` before including `axiomtrace.h`:

```c
#define AXIOM_PROFILE AXIOM_PROFILE_PROD
#include "axiomtrace.h"
```

**Effect per profile**:

| Feature         | DEV | FIELD | PROD |
|-----------------|-----|-------|------|
| `AX_LOG`        | Yes | Yes   | No   |
| `AX_EVT`        | Yes | Yes   | Yes  |
| `AX_PROBE`      | Yes | Yes   | No   |
| `AX_FAULT`      | Yes | Yes   | Yes  |
| `AX_KV`         | Yes | Yes   | Yes  |
| `DEBUG` level   | Yes | No    | No   |
| Asserts         | Yes | Yes   | No   |

---

## 8. Core API

### 8.1 Initialization

```c
void axiom_init(void);
```

Initializes the Core ring buffer, sequence counter, level mask, and drop counters. Must be called before any log macro.

### 8.2 Backend Registration

```c
int axiom_backend_register(const axiom_backend_t *backend);
```

Registers a backend for event dispatch. Returns 0 on success, negative on error (e.g., max backends reached).

### 8.3 Filter Control

```c
void axiom_level_mask_set(uint32_t mask);
uint32_t axiom_level_mask_get(void);

void axiom_module_mask_set(uint32_t mask);
uint32_t axiom_module_mask_get(void);
```

Runtime filter control. `mask` is a bitfield where bit `N` corresponds to level/module `N`.

### 8.4 Flush

```c
void axiom_flush(void);
```

Explicitly drain the ring buffer to all registered backends. Optional; backends may also drain asynchronously.

---

## 9. Port Layer API

All port functions provide `__attribute__((weak))` default implementations. Override by providing strong symbols in your project.

```c
uint32_t axiom_port_timestamp(void);
void     axiom_port_critical_enter(void);
void     axiom_port_critical_exit(void);
void     axiom_port_fault_hook(uint8_t module_id, uint16_t event_id,
                                const uint8_t *payload, uint8_t payload_len);

/* Flash operations (for capsule backend) */
int  axiom_port_flash_erase(uint32_t addr, uint32_t len);
int  axiom_port_flash_write(uint32_t addr, const uint8_t *data, uint32_t len);
```

---

## 10. Configuration Macros

| Macro | Default | Description |
|-------|---------|-------------|
| `AXIOM_RING_BUFFER_SIZE` | 4096 | Main ring buffer size in bytes |
| `AXIOM_RING_BUFFER_POLICY` | `DROP` | `DROP` or `OVERWRITE` |
| `AXIOM_MAX_PAYLOAD_LEN` | 128 | Maximum payload bytes per event |
| `AXIOM_MODULE_MAX` | 32 | Maximum module IDs for filter bitmask (0 .. MAX-1) |
| `AXIOM_BACKEND_MAX` | 4 | Maximum registered backends |
| `AXIOMTRACE_VERSION_MAJOR` | 0 | Library major version |
| `AXIOMTRACE_VERSION_MINOR` | 7 | Library minor version |
| `AXIOMTRACE_VERSION_PATCH` | 0 | Library patch version |
| `AXIOM_PROFILE` | `DEV` | `DEV`, `FIELD`, or `PROD` |
| `AXIOM_CFG_LOCATION_MODE` | `NONE` | `NONE`, `HASH`, or `FILE_ID` payload location metadata |
| `AXIOM_CFG_LOCATION_FUNCTION` | 0 | Include `__func__` hash in `HASH` mode |
| `AXIOM_SOURCE_FILE_ID` | 0 | Per-translation-unit ID injected for `FILE_ID` mode |
| `AXIOM_CFG_USE_LOCATION` | 0 | Compatibility switch mapping enabled legacy builds to `HASH` mode |
| `AXIOM_CAPSULE_ENABLED` | 1 | Enable fault capsule |
| `AXIOM_CAPSULE_PRE_EVENTS` | 32 | Pre-fault window size |
| `AXIOM_CAPSULE_POST_EVENTS` | 16 | Post-fault window size |

Override before including `axiomtrace.h`:

```c
#define AXIOM_RING_BUFFER_SIZE 1024
#define AXIOM_PROFILE AXIOM_PROFILE_PROD
#include "axiomtrace.h"
```

---

## 11. Type System Constants

### 11.1 Sync Byte

```c
#define AXIOM_SYNC_BYTE 0xA5u
```

Fixed synchronization byte at the start of every binary frame. Used by decoders to locate frame boundaries in a byte stream.

### 11.2 Header Length

```c
#define AXIOM_HEADER_LEN  8u  /* sync + version + level + module_id + event_id(2) + seq(2) */
```

### 11.3 CRC Length

```c
#define AXIOM_CRC_LEN 2u  /* CRC-16/CCITT-FALSE trailing bytes */
```

### 11.4 Maximum Timestamp Length

```c
#define AXIOM_MAX_TIMESTAMP_LEN 5u  /* Variable-length: 1–5 bytes */
```

### 11.5 Payload Type Tags (`axiom_type_t`)

```c
typedef enum {
    AXIOM_TYPE_BOOL      = 0x00,
    AXIOM_TYPE_U8        = 0x01,
    AXIOM_TYPE_I8        = 0x02,
    AXIOM_TYPE_U16       = 0x03,
    AXIOM_TYPE_I16       = 0x04,
    AXIOM_TYPE_U32       = 0x05,
    AXIOM_TYPE_I32       = 0x06,
    AXIOM_TYPE_F32       = 0x07,
    AXIOM_TYPE_TIMESTAMP = 0x08,
    AXIOM_TYPE_BYTES     = 0x09,
    AXIOM_TYPE_META_LOCATION = 0x0A,
    AXIOM_TYPE_META_IDENTITY = 0x0B
    /* 0x0C–0x7F: Reserved  |  0x80–0xFF: User-defined */
} axiom_type_t;
```

Each tag is a 1-byte type marker prefixed before the value in the binary payload. The original `#define` constants (e.g., `AXIOM_TYPE_U16`) remain available for backward compatibility.

### 11.6 Encoder Tag Size

```c
#define AXIOM_TAG_SIZE 1u  /* All type tags are exactly 1 byte */
```

Used by the encoder to account for the fixed size of type tag bytes in overflow checks.

### 11.7 Metadata Identity Event

```c
#define AXIOM_METADATA_ID_LEN 8u
void axiom_emit_metadata_id(const uint8_t metadata_id[AXIOM_METADATA_ID_LEN]);
```

The generated `axiom_metadata_id_generated.h` header exposes `AXIOM_EMIT_METADATA_ID()`. Emit it once in a captured trace before semantic events when decoding with `--bundle` or `--bundle-store`.

---

## 12. Backend Error Codes (`axiom_backend_err_t`)

```c
typedef enum {
    AXIOM_BACKEND_OK       =  0,
    AXIOM_BACKEND_ERR_NULL = -1,  /* NULL pointer parameter */
    AXIOM_BACKEND_ERR_FULL = -2,  /* Registration table full */
    AXIOM_BACKEND_ERR_STRUCT = -3  /* Corrupt struct (invalid .version) */
} axiom_backend_err_t;
```

The original integer return values (`0`, `-1`, `-2`, `-3`) are preserved for source-level backward compatibility.

