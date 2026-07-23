> [English](wire_format.md) | [简体中文](wire_format_zh.md)

# AxiomTrace Wire Format Specification

> Version: v2.0 | Status: **FROZEN** | Code: `baremetal/core/axiom_event.c`, `baremetal/core/axiom_crc.c`

---

## 1. Scope

This document defines how AxiomTrace Event Records are serialized to byte streams for transport over UART, USB, CAN-FD, or memory buffers.

Wire `v2.0` is the current emitted format. Any modification requires a spec update, golden frame update, decoder update, and full regression test pass.

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
| 16,384 – 2,097,151 | 3 | `0b110xxxxx` + 2 bytes (21-bit value) |
| 2,097,152 – 4,294,967,295 | 5 | `0xFE` + 4 bytes (full 32-bit) |

The timestamp is included in the CRC-16 calculation (Header + Timestamp + Payload Length + Payload). Decoders must decode the variable-length timestamp field before locating `payload_len` at `frame[8 + ts_len]`.

### 2.2 Optional Location and Metadata Identity Fields

Wire `v2.0` retains the 8-byte header and timestamp layout. Ordinary event arguments are packed values whose lengths and types are supplied by the matching dictionary. Host-decoding metadata remains tagged at the end of that packed payload:

| Tag | Encoding after tag | Purpose |
|-----|--------------------|---------|
| `0x0A` | `mode=1, file_hash:u16, line:u16, func_hash:u16` or `mode=2, file_id:u16, line:u16` | Optional call-site location |
| `0x0B` | `metadata_id:8` | Selects the exact host metadata bundle |

`0x0A` adds 8 bytes total in `HASH` mode or 6 bytes total in `FILE_ID` mode, including its tag. `0x0B` adds 9 bytes total and is normally emitted in the system metadata identity event `(module_id = 0, event_id = 2)`.

Reserved system payloads are exceptions to application dictionary decoding: `AX_PROBE` `(0,0)` retains typed `tag_hash`/value fields, `DROP_SUMMARY` `(0,1)` has fixed packed `u32/u8/u16` fields, and metadata identity `(0,2)` must contain exactly its nine-byte tagged identifier.

### 2.3 Byte Order

All multi-byte fields inside the Frame Body are **little-endian**. An external transport wrapper must not alter the CRC-covered Frame Body.

### 2.4 Alignment

Wire format is unaligned (packed). The encoder uses `memcpy` or byte-wise writes to avoid undefined behavior on unaligned access. Decoders must do the same.

---

## 3. Stream Framing and Resynchronization

Wire v2 emitted by Core is the raw Frame Body above; Core does not apply COBS or append a delimiter. A stream decoder searches for sync byte `0xA5`, validates version, level, timestamp length, payload length, maximum frame length, and CRC, and advances one byte after an invalid candidate. Core and Deferred flush use the same boundary checks.

Transport-specific framing may wrap the complete Frame Body outside AxiomTrace, but that wrapper is not part of Wire v2 and must be removed before using the standard decoder.

---

## 4. CRC-16

- **Algorithm**: CRC-16/CCITT-FALSE (`poly = 0x1021`, `init = 0xFFFF`, no reflection, final XOR `0x0000`).
- **Coverage**: Header (8B) + Timestamp (1..5B) + Payload Length (1B) + Payload (N bytes).
- **Computation**: Precomputed 256-byte ROM lookup table for O(n) speed.
- **Verification**: Decoder recomputes CRC over the same range and compares with the trailing 2 bytes. Mismatch = `FRAME_INVALID`.
- **Error Recovery**: CRC failure rejects the candidate frame; byte-wise sync search then finds the next valid raw frame.

---

## 5. Transport Variations

Memory and Deferred backends preserve the complete Frame Body byte-for-byte. Hardware transport sources in the repository are reference integrations; USB CDC, SWO/ITM, and CAN-FD contracts are outside v1.0. A custom backend must pass complete frames and must not alter the CRC-covered body.

---

## 6. Decoder Behavior

### 6.1 Valid Frame

A frame is valid if and only if:

1. `sync == 0xA5`
2. `version` major nibble is supported (current emitter uses `0x2`; historical `0x1` is supported)
3. `level` upper nibble is `0`
4. Timestamp is decodable (first byte after header identifies length 1/2/3/5)
5. `payload_len` matches actual payload bytes read before CRC
6. CRC-16 recomputed over Header + Timestamp + Payload Length + Payload matches the trailing 2 bytes

### 6.2 Invalid Frame Handling

On any validation failure, the decoder must:

1. Reject the frame explicitly (return `FRAME_INVALID` or equivalent)
2. Advance one byte and search for the next `0xA5` sync candidate
3. Resume decoding from the next candidate frame
4. **Never crash or enter undefined state**

### 6.3 Unknown Type Tag

For wire `v2`, a decoder without a matching dictionary reports ordinary payload values as raw bytes because field boundaries cannot be inferred. With a dictionary, bytes after the expected packed arguments must be recognized metadata suffixes (`0x0A` or `0x0B`); an unknown suffix is a malformed payload warning. For historical wire `v1.x`, the decoder continues to recognize per-field type tags and reports unknown tags without crashing.

---

## 7. Versioning

The `version` byte in the header encodes `major << 4 | minor`.

- Decoders **must reject** unsupported **major** versions.
- **Minor** version additions must remain compatible within a major-version payload interpretation.

Current emitted version: **0x20** (v2.0). The host decoder remains able to decode historical typed-payload `v1.0` and `v1.1` frame bodies.

---

## 8. Frame Constants (Named Definitions)

The following named constants are defined in `baremetal/core/axiom_event.h` to replace hard-coded magic numbers:

| Constant | Value | Description |
|----------|-------|-------------|
| `AXIOM_SYNC_BYTE` | `0xA5u` | Sync byte at frame start |
| `AXIOM_HEADER_LEN` | `8u` | Header: sync(1) + version(1) + level(1) + module_id(1) + event_id(2) + seq(2) |
| `AXIOM_CRC_LEN` | `2u` | Trailing CRC-16/CCITT-FALSE bytes |
| `AXIOM_MAX_TIMESTAMP_LEN` | `5u` | Maximum variable-length timestamp encoding |
| `AXIOM_MAX_PAYLOAD_LEN` | `128u` | Maximum payload bytes per event |
| `AXIOM_TAG_SIZE` | `0u` | Per-argument type-tag overhead in wire v2 packed payloads |
