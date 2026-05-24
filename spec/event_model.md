> [English](event_model.md) | [简体中文](event_model_zh.md)

# AxiomTrace Event Model Specification

> 版本：v1.1  |  状态：**FROZEN**  |  对应代码：`baremetal/core/axiom_event.h`

---

## 1. Overview

AxiomTrace events are **structured binary records**. Each event consists of a fixed-size header followed by a self-describing typed payload. No format strings or human-readable text are stored in the firmware hot path; readability is restored on the host via the event dictionary.

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

Payload is a concatenation of typed fields. Each field is **self-describing** via its type tag:

| Type Tag | C Type      | Wire Size | Encoding                          |
|----------|-------------|-----------|-----------------------------------|
| 0x00     | `bool`      | 1         | 0 or 1                            |
| 0x01     | `uint8_t`   | 1         | Raw little-endian                 |
| 0x02     | `int8_t`    | 1         | Raw two's complement              |
| 0x03     | `uint16_t`  | 2         | Little-endian                     |
| 0x04     | `int16_t`   | 2         | Little-endian two's complement    |
| 0x05     | `uint32_t`  | 4         | Little-endian                     |
| 0x06     | `int32_t`   | 4         | Little-endian two's complement    |
| 0x07     | `float`     | 4         | IEEE-754 single little-endian     |
| 0x08     | `timestamp` | 4         | Compressed relative timestamp (see `axiom_timestamp.c`) |
| 0x09     | `bytes`     | N         | 1-byte length prefix + raw bytes  |
| 0x0A     | location metadata | 5 or 7 | Mode plus fixed-width source location fields |
| 0x0B     | metadata identity | 8       | Bundle metadata ID bytes          |
| 0x0C-0x7F| reserved    | —         | Reserved for future standard types |
| 0x80-0xFF| user-defined| —         | Available for project-specific types |

`location metadata` does not change the fixed header. It is appended to the typed payload only when enabled:

- `FILE_ID` (`mode = 2`): `[0x0A][mode:u8][file_id:u16][line:u16]`, 6 bytes total.
- `HASH` (`mode = 1`): `[0x0A][mode:u8][file_hash:u16][line:u16][func_hash:u16]`, 8 bytes total.
- `NONE`: no location field is emitted.

`metadata identity` is emitted as `[0x0B][metadata_id:8]` in the reserved system event `(module_id = 0, event_id = 2)`. The host uses it to select and validate the matching metadata bundle before semantic decoding.

**Design principles**:
1. The encoder writes type tags so the decoder can reconstruct arguments **without external schema knowledge**.
2. The dictionary is the authoritative source of human-readable names and templates, but is **not required** for structural decoding.
3. New type tags in the remaining `0x0C-0x7F` standard range require spec update, decoder update, golden update, and docs update.

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
- **Payload**: `payload_len` bytes, self-describing type tags
- **CRC-16**: CCITT-FALSE (`poly = 0x1021`, `init = 0xFFFF`, no reflection, final XOR `0x0000`)

### 5.1 Direct-to-Ring (D2R) Mechanism

AxiomTrace uses the **Direct-to-Ring (D2R)** mechanism to minimize RAM overhead and latency:

1. **Zero Stack Buffering**: Instead of assembling the entire frame in a temporary stack buffer, the encoder acquires space in the ring buffer and writes fields directly to it.
2. **Incremental CRC**: The CRC-16 is computed incrementally as each byte is written. This eliminates the need for a second pass over the frame and avoids storing the full frame on the stack.
3. **Atomic Commit**: The ring buffer "head" is only updated after the full frame (including CRC) is successfully written, ensuring that partial or interrupted writes do not corrupt the stream.

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

The `text` field uses named placeholders with type hints that must match the encoded payload types. Mismatches are detected by the dictionary validator tool.

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
- **Stack Efficiency**: By using D2R, the stack usage is reduced to a few dozen bytes, regardless of the `AXIOM_MAX_PAYLOAD_LEN` setting. No large local arrays are required.

---

## 8. Versioning

- `version` byte = `major << 4 | minor`
- **Major** change = incompatible header or payload structure change; decoder must reject.
- **Minor** change = backward-compatible addition (new type tag, new flag); decoder may ignore unknown fields.

Current wire format: **v1.1** (`version = 0x11`). Decoders retain structural support for v1.0 frames without location metadata.
