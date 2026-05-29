> [English](event_model.md) | [简体中文](event_model_zh.md)

# AxiomTrace Event Model Specification

> Version: v2.0 | Status: **FROZEN** | Code: `baremetal/core/axiom_event.h`

---

## 1. Overview

AxiomTrace events are **structured binary records**. Each event consists of a fixed-size header followed by a compact payload. Wire `v2.0` encodes normal arguments as dictionary-defined packed values; no format strings or human-readable text are stored in the firmware hot path. Wire `v1.x` typed payloads remain readable by the host decoder for compatibility.

**Core principle**: The Event Record is the only log entity. Text / JSON / Binary are merely render or transport forms.

---

## 2. Event Header

The header is strictly **8 bytes** (packed, little-endian on wire):

| Offset | Field       | Type     | Bits | Description                              |
|--------|-------------|----------|------|------------------------------------------|
| 0      | `sync`      | uint8_t  | 8    | Sync byte `0xA5` (framing hint)          |
| 1      | `version`   | uint8_t  | 8    | Wire format version (`major << 4 | minor`) |
| 2      | `level`     | uint8_t  | 4    | Log level (see §3), lower nibble         |
| 2      | `reserved`  | uint8_t  | 4    | Reserved, must be zero                   |
| 3      | `module_id` | uint8_t  | 8    | Module identifier (compile-time assigned) |
| 4      | `event_id`  | uint16_t | 16   | Per-module event identifier (LE)         |
| 6      | `seq`       | uint16_t | 16   | Global sequence number (LE, wraps naturally) |

**Compile-time size assertions** (enforced in `axiom_event.h`):
```c
_Static_assert(sizeof(axiom_event_header_t) == 8, "header size");
_Static_assert(_Alignof(axiom_event_header_t) == 1, "header align");
```

---

## 3. Log Levels

| Value | Name      | Semantic                                | Profile Behavior          |
|-------|-----------|-----------------------------------------|---------------------------|
| 0     | `DEBUG`   | Verbose diagnostic information          | Droppable, rate-limited   |
| 1     | `INFO`    | Normal operational events               | Default output level      |
| 2     | `WARN`    | Unexpected but recoverable condition    | Always retained           |
| 3     | `ERROR`   | Failure that may affect operation       | Always retained           |
| 4     | `FAULT`   | Critical fault; triggers capsule freeze | Always retained; invokes `axiom_port_fault_hook()` |
| 5-15  | reserved  | Reserved for future use                 | —                         |

**Level filtering**: A 32-bit mask controls which levels are emitted. `DEBUG` may be dropped under memory pressure or rate limiting.

---

## 4. Payload Model

After the header comes the **payload length** (`1 byte`, max 255) followed by the **payload data**.

In wire `v2.0`, normal arguments are packed in dictionary order:

| Dictionary Type | Value Encoding |
|-----------------|----------------|
| `bool`, `u8`, `i8` | 1 byte |
| `u16`, `i16` | 2 bytes, little-endian |
| `u32`, `i32`, `f32`, `timestamp` | 4 bytes, little-endian |
| `bytes` | 1-byte length followed by raw bytes |

The bundle dictionary is required to locate and type these normal argument values. Two metadata suffixes remain explicitly tagged:

| Tag | Encoding After Tag | Purpose |
|-----|--------------------|---------|
| `0x0A` | `mode` plus fixed-width location fields | Optional call-site location |
| `0x0B` | `metadata_id:8` | Select the matching metadata bundle |

`location metadata` does not change the fixed header. It is appended to the packed payload only when enabled:

- `FILE_ID` (`mode = 2`): `[0x0A][mode:u8][file_id:u16][line:u16]`, 6 bytes total.
- `HASH` (`mode = 1`): `[0x0A][mode:u8][file_hash:u16][line:u16][func_hash:u16]`, 8 bytes total.
- `NONE`: no location field is emitted.

`metadata identity` is emitted as `[0x0B][metadata_id:8]` in the reserved system event `(module_id = 0, event_id = 2)`. The host uses it to select and validate the matching metadata bundle before semantic decoding.

Reserved system events have fixed protocol schemas and do not depend on an application dictionary:

| System Event | Payload Rule |
|--------------|--------------|
| `(0, 0)` `AX_PROBE` | Typed fields `[u16 tag][tag_hash:u16][value tag][value]`, because the probe value type varies at each call site |
| `(0, 1)` `DROP_SUMMARY` | Packed fixed fields `[lost_count:u32][last_module_id:u8][last_event_id:u16]` |
| `(0, 2)` metadata identity | Exactly `[0x0B][metadata_id:8]`; trailing bytes are malformed |

**Compatibility principles**:
1. Wire `v2.0` reduces per-argument overhead by making the matched dictionary/bundle part of semantic decoding.
2. Without a dictionary, the v2 decoder validates framing/CRC, exposes ordinary payload bytes as raw data, and still decodes the fixed system events above.
3. Wire `v1.x` remains structurally decodable through its historical per-field type tags.

---

## 5. Complete Frame Layout & Encoding

```
┌────────┬───────────┬─────────┬────────┬──────────┬──────┬────────┬─────────┬────────────┐
│ Header │ Timestamp │ Payload │ Payload│ CRC-16 │
│ 8B     │ 1..5B     │ Len 1B  │ N B    │ 2B LE  │
└────────┴───────────┴─────────┴────────┘──────────┘
```

- **Header**: see §2
- **Timestamp**: Variable length delta-compressed timestamp.
- **Payload Length**: 1 byte, value `0..255`
- **Payload**: `payload_len` bytes, v2 packed values with optional tagged metadata suffixes
- **CRC-16**: CCITT-FALSE (`poly = 0x1021`, `init = 0xFFFF`, no reflection, final XOR `0x0000`)

### 5.1 Short Critical Section Frame Assembly

AxiomTrace uses bounded binary frame assembly to minimize hot-path latency:

1. **Packed Payload Encoding**: Frontend macros encode only dictionary-defined binary values; no format strings are written to the firmware event stream.
2. **Short Critical Section**: With `AXIOM_SHORT_CS=1` (default), `axiom_write()` pre-encodes the complete frame outside the critical section, then commits it to the ring in one write while shared state is protected.
3. **CRC-Protected Frame**: CRC-16 covers the header, timestamp, payload length, and payload. The fallback `AXIOM_SHORT_CS=0` path keeps incremental CRC while reducing stack usage.

For byte-stream transports (UART, USB CDC), the entire frame may be COBS-encoded with a `0x00` delimiter. See `spec/wire_format.md` for transport variations.

---

## 6. Event Dictionary Mapping

Each `(module_id, event_id)` pair maps to a dictionary entry:

```json
{
  "0x0305": {
    "module": "MOTOR",
    "level": "WARN",
    "name": "MOTOR_CURRENT_OVER_LIMIT",
    "text": "motor current over limit: phase={u8}, current={i16}, limit={i16}"
  }
}
```

The `text` field uses named placeholders with types that define the v2 packed argument layout. The validator rejects malformed payloads and unmatched bundle identities.

**Dictionary sources** (choose one per project):
1. **X-Macro** (`axiom_events.h`): C macros expanded at compile time; `extract_dict.py` extracts JSON.
2. **YAML**: Layer 3 advanced workflow; `codegen.py` generates C macros and dictionary JSON.
3. **Manual JSON**: Directly edit `dictionary.json` for small projects.

---

## 7. Constraints

- Protocol payload length field capacity: **255 bytes**; the current firmware configuration defaults `AXIOM_MAX_PAYLOAD_LEN` to **128 bytes**.
- Maximum number of modules: **256** (`module_id` is `uint8_t`).
- Maximum events per module: **65536** (`event_id` is `uint16_t`, practically limited by ROM).
- Header, Timestamp and payload are always CRC-protected.
- **Stack Bound**: Default short-critical-section mode uses a bounded frame buffer (`AXIOM_MAX_FRAME_LEN`) to reduce interrupt latency. Set `AXIOM_SHORT_CS=0` on extremely stack-constrained targets.

---

## 8. Versioning

- `version` byte = `major << 4 | minor`
- **Major** change = incompatible header or payload interpretation change.
- **Minor** change = backward-compatible addition within an existing major version.

Current emitted wire format: **v2.0** (`version = 0x20`). The host decoder retains typed-payload structural decoding for historical **v1.0/v1.1** frames.
