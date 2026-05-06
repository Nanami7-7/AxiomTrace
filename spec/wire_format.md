> [English](wire_format.md) | [简体中文](wire_format_zh.md)

# AxiomTrace Wire Format Specification

> 版本：v1.0  |  状态：**FROZEN**  |  对应代码：`baremetal/core/axiom_event.c`, `baremetal/core/axiom_crc.c`

---

## 1. Scope

This document defines how AxiomTrace Event Records are serialized to byte streams for transport over UART, USB, CAN-FD, or memory buffers.

**Frozen as of v0.2-wire**. Any modification requires spec update, golden frame update, decoder update, and full regression test pass.

---

## 2. Frame Structure

A wire frame consists of:

```c
[ Frame Body ]
  ├── Header (8 bytes)
  ├── Timestamp (1..5 bytes, delta-compressed)
  ├── Payload Length (1 byte)
  ├── Payload (0..255 bytes)
  └── CRC-16 (2 bytes)
[ Optional Transport Wrapper ]
```

### 2.1 Timestamp

The timestamp field is auto-inserted by `axiom_write()` for every event. It uses delta compression relative to the previous event's raw timestamp (from `axiom_port_timestamp()`):

| Range of delta | Bytes | Encoding |
|---------------|-------|----------|
| 0 – 127 | 1 | `0b0xxxxxxx` (7-bit value) |
| 128 – 16,383 | 2 | `0b10xxxxxx` + 1 byte (13-bit value) |
| 16,384 – 2,097,151 | 3 | `0b110xxxxx` + 2 bytes (19-bit value) |
| 2,097,152 – 4,294,967,295 | 5 | `0xFE` + 4 bytes (full 32-bit) |

The timestamp is included in the CRC-16 calculation (Header + Timestamp + Payload Length + Payload). Decoders must decode the variable-length timestamp field before locating `payload_len` at `frame[8 + ts_len]`.

### 2.2 Byte Order

All multi-byte fields are **little-endian** unless the transport mandates otherwise (e.g., CAN-FD uses big-endian by convention; the CAN-FD backend performs byte swap).

### 2.3 Alignment

Wire format is unaligned (packed). The encoder uses `memcpy` or byte-wise writes to avoid undefined behavior on unaligned access. Decoders must do the same.

---

## 3. COBS Encoding (Byte-Stream Transports)

For UART and USB CDC, the entire Frame Body is **COBS-encoded** to eliminate `0x00` bytes inside the frame:

```c
[ COBS Encoded Frame Body ]
[ 0x00 Delimiter ]
```

**Properties**:

- Worst-case overhead: `ceil(n/254)` bytes.
- Guaranteed no `0x00` bytes inside the encoded block.
- Single-pass encode/decode, no lookup tables required.
- The final `0x00` delimiter guarantees resynchronization after frame loss.
- **Blind Overwrite Compatibility**: Even if a frame is partially overwritten in a ring buffer, the next frame starting after a `0x00` delimiter can be reliably located.

**Note**: COBS is applied to the **entire Frame Body** (Header + Timestamp + Payload Length + Payload + CRC). The delimiter is **not** part of the COBS block.

---

## 4. CRC-16

- **Algorithm**: CRC-16/CCITT-FALSE (`poly = 0x1021`, `init = 0xFFFF`, no reflection, final XOR `0x0000`).
- **Coverage**: Header (8B) + Timestamp (1..5B) + Payload Length (1B) + Payload (N bytes).
- **Computation**: Precomputed 256-byte ROM lookup table for O(n) speed.
- **Verification**: Decoder recomputes CRC over the same range and compares with the trailing 2 bytes. Mismatch = `FRAME_INVALID`.
- **Error Recovery**: In "Blind Overwrite" scenarios, CRC failure allows the host to detect partially overwritten frames and safely discard them without losing synchronization for subsequent frames.

---

## 5. Transport Variations

| Transport   | COBS | CRC  | Delimiter | Notes                                   |
|-------------|------|------|-----------|-----------------------------------------|
| UART        | Yes  | Yes  | `0x00`    | Full frame per DMA descriptor or IRQ    |
| USB CDC     | Yes  | Yes  | `0x00`    | Bulk IN endpoint, 64-byte chunks        |
| Memory Dump | No   | Yes  | N/A       | Sequential writes to RAM/Flash region   |
| SWO/ITM     | No   | No   | N/A       | Stream of 32-bit stimulus words         |
| SEGGER RTT  | No   | Optional | N/A   | Up-channel binary blob                  |
| CAN-FD      | No   | Yes  | N/A       | Frame split across multiple IDs; BE swap|

**Rules**:

- Backends must not define private protocols. They only apply transport-specific wrappers.
- The Frame Body (Header + Timestamp + Payload Length + Payload + CRC) is identical across all transports.
- SWO/ITM omits CRC because the transport is assumed lossless within the debug probe channel.

---

## 6. Decoder Behavior

### 6.1 Valid Frame

A frame is valid if and only if:

1. `sync == 0xA5`
2. `version` major nibble is supported (currently `0x1`)
3. `level` upper nibble is `0`
4. Timestamp is decodable (first byte after header identifies length 1/2/3/5)
5. `payload_len` matches actual payload bytes read before CRC
6. CRC-16 recomputed over Header + Timestamp + Payload Length + Payload matches the trailing 2 bytes

### 6.2 Invalid Frame Handling

On any validation failure, the decoder must:

1. Reject the frame explicitly (return `FRAME_INVALID` or equivalent)
2. Advance to the next `0x00` delimiter (COBS transports) or next `0xA5` sync byte
3. Resume decoding from the next candidate frame
4. **Never crash or enter undefined state**

### 6.3 Unknown Type Tag

If the decoder encounters a type tag in `0x0A-0x7F` that it does not recognize:

1. Skip the field based on the type tag's known size (if known)
2. If size is unknown, skip to the next field boundary heuristically or mark the payload as `PARTIAL_DECODE`
3. Do not crash

---

## 7. Versioning

The `version` byte in the header encodes `major << 4 | minor`.

- Decoders **must reject** unsupported **major** versions.
- **Minor** version additions are safe to ignore (new type tags, new reserved fields).

Current version: **0x10** (v1.0).
