> [English](decoder_protocol.md) | [简体中文](decoder_protocol_zh.md)

# AxiomTrace Decoder Protocol Specification

> Wire Version: v2.0 | Status: **Building**

---

## 1. Overview

The AxiomTrace decoder toolchain runs on the host (Linux/macOS/Windows) and converts binary Event Records / Fault Capsules into human-readable forms.

**Components**:

- `axiom-decoder` — Parses binary frames and renders text/json/jsonl/raw output
- `axiom-codegen` — Generates C headers and `dictionary.json` from event definitions
- `axiom-bundle` — Creates the standard metadata bundle
- `axiom-validate` — Validates dictionaries, event sources, bundles, and golden inputs

---

## 2. Input Formats

### 2.1 Raw Binary Stream

A concatenation of complete raw v2.0 Frame Bodies. Historical v1.0/v1.1 typed-payload frames remain structurally decodable. Transport wrappers such as COBS are outside the decoder input contract and must be removed first.

```bash
axiom-decoder trace.bin --bundle build/axiomtrace-bundle --format text
```

### 2.2 Memory Dump

A raw memory dump containing the ring buffer metadata + raw ring data:

```
┌─────────────┬─────────────┬─────────────┬─────────────────────────────┐
│ head (4B LE)│ tail (4B LE)│ cap (4B LE) │ raw ring data (cap bytes)   │
└─────────────┴─────────────┴─────────────┴─────────────────────────────┘
```

```bash
axiom-decoder trace_ram.bin --bundle build/axiomtrace-bundle --format raw
```

### 2.3 Capsule Binary

A fault capsule committed to Flash. See `spec/fault_capsule.md` for layout.

```bash
axiom-decoder capsule.bin --bundle build/axiomtrace-bundle --format raw
```

---

## 3. Output Formats

### 3.1 Text Render

Default output. Uses dictionary templates to fill placeholders:

```
[00:00:01.234] [INFO] [MOTOR] START: rpm=3200
[00:00:05.678] [WARN] [MOTOR] CURRENT_OVER_LIMIT: phase=2, current=1520, limit=1200
```

Template syntax in dictionary:

```json
{
  "text": "motor current over limit: phase={u8}, current={i16}, limit={i16}"
}
```

Type hints (`{u8}`, `{i16}`, `{f32}`) define the packed v2 payload layout used during bundle-backed decoding.

### 3.2 JSON Export

```json
[
  {
    "timestamp": 1.234,
    "level": "INFO",
    "module": "MOTOR",
    "event": "START",
    "seq": 42,
    "payload": {
      "rpm": 3200
    }
  }
]
```

### 3.3 Capsule Report

Markdown or HTML report containing:

- Fault summary (time, reset reason, fault type)
- Register snapshot (pc, lr, sp, xpsr, etc.)
- Pre-fault event sequence
- Post-fault event sequence
- Firmware hash
- Drop statistics

---

## 4. Dictionary Format

### 4.1 JSON Dictionary

```json
{
  "version": "1.0",
  "modules": {
    "0x03": {
      "name": "MOTOR",
      "events": {
        "0x01": {
          "name": "START",
          "level": "INFO",
          "text": "motor started: rpm={u16}",
          "args": [
            { "name": "rpm", "type": "u16" }
          ]
        }
      }
    }
  }
}
```

### 4.2 Validation Rules

- Every `(module_id, event_id)` referenced in the binary must exist in the dictionary, or the decoder marks it as `UNKNOWN_EVENT`.
- For v2, `text`/`args` types define the packed argument boundaries; truncated values or unknown metadata suffixes are validation failures.
- Reserved v2 system events are decoded by fixed schemas: typed `AX_PROBE` `(0,0)`, packed `DROP_SUMMARY` `(0,1)`, and exact-length metadata identity `(0,2)`.
- For historical v1, typed fields continue to be parsed structurally without requiring a dictionary.
- `level` in dictionary should match the frame's `level` field. Mismatch = `LEVEL_MISMATCH` warning.

---

## 5. CLI Interface

```bash
# Decode binary to text
axiom-decoder trace.bin --bundle build/axiomtrace-bundle --format text

# Decode binary to JSON
axiom-decoder trace.bin --bundle build/axiomtrace-bundle --format json > trace.json

# Compatibility path using only dictionary.json
axiom-decoder trace.bin --dictionary dictionary.json --format jsonl

# Validate dictionary against golden frames
axiom-validate --bundle build/axiomtrace-bundle --golden golden/

# Generate code and bundle
axiom-codegen --events events.yaml --out build/generated
axiom-bundle generate --events events.yaml --compile-db build/compile_commands.json --out build/axiomtrace-bundle
```

---

## 6. Error Handling

| Error Code | Description | Decoder Action |
|------------|-------------|----------------|
| `FRAME_INVALID` | CRC mismatch, bad sync, or bad version | Skip to next delimiter/sync, log warning |
| `UNKNOWN_EVENT` | (module_id, event_id) not in dictionary | Output raw IDs and hex payload, continue |
| `UNKNOWN_EVENT_SCHEMA` | v2 event lacks dictionary metadata | Output raw payload; semantic validation fails |
| `TRUNCATED_PAYLOAD` | payload_len > actual bytes available | Mark as truncated, skip frame |
| `UNKNOWN_METADATA_SUFFIX` | Bytes remain after v2 packed args but do not form metadata | Mark malformed; semantic validation fails |
| `INVALID_METADATA_IDENTITY` | System metadata identity payload is not exactly tag plus 8-byte ID | Mark malformed; semantic validation fails |
| `UNKNOWN_TYPE_TAG` | Historical v1 type tag is not recognized | Mark unknown, continue |

**Critical rule**: The decoder must never crash or enter undefined state, regardless of input corruption.
