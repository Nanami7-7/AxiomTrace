> [English](event_dictionary.md) | [简体中文](event_dictionary_zh.md)

# AxiomTrace Event Dictionary Specification

## 1. Purpose

The event dictionary is the authoritative mapping from `(module_id, event_id)` to human-readable metadata. It lives on the host (build machine / PC) and is never flashed to the target as text.

## 2. Generated JSON Schema

`dictionary.json` is the decoder collateral. `axiom-codegen` accepts this JSON-shaped event source directly and also accepts YAML source files when the optional `PyYAML` dependency is installed.

```json
{
  "version": "1.0",
  "enums": {
    "ENUM_NAME": {
      "0": "LABEL_OK",
      "1": "LABEL_ERR"
    }
  },
  "dictionary": {
    "0x01": {
      "name": "MODULE_NAME",
      "description": "Human readable module description",
      "events": {
        "0x01": {
          "level": "INFO",
          "name": "EVENT_NAME",
          "text": "Human readable template with {type} placeholders",
          "args": [
            {
              "name": "arg0_name",
              "type": "bool|u8|i8|u16|i16|u32|i32|f32|timestamp|bytes",
              "enum": "ENUM_NAME"
            }
          ]
        }
      }
    }
  }
}
```

## 3. Placeholder Syntax

The `text` field uses `{name:type}` placeholders. Supported types:

| Placeholder | Frontend C Type | Description            |
|-------------|------------------|------------------------|
| `{name:u8}`  | `uint8_t`        | Unsigned 8-bit         |
| `{name:i8}`  | `int8_t`         | Signed 8-bit           |
| `{name:u16}` | `uint16_t`       | Unsigned 16-bit        |
| `{name:i16}` | `int16_t`        | Signed 16-bit          |
| `{name:u32}` | `uint32_t`       | Unsigned 32-bit        |
| `{name:i32}` | `int32_t`        | Signed 32-bit          |
| `{name:f32}` | `float`          | IEEE-754 float         |
| `{name:timestamp}`| `uint32_t`    | Fixed-width timestamp argument |
| `{name:bytes}`| byte sequence     | Length-prefixed bytes |

For an `AX_KV` event in wire `v2.0`, the dictionary `args` list must alternate a `u16` key-hash field and the corresponding packed value field in the exact emitted order.

## 4. Enum Mapping

Enums provide a way to translate raw integer values into descriptive strings during decoding.

- **Definition**: Defined globally under the `enums` key. Key is the enum name, values are mappings of integers (as keys) to strings.
- **Mapping**: In the firmware, enums are passed as standard integer types (`u8`, `u32`, etc.).
- **Resolution**: The decoder uses the `enum` field in the argument definition to find the appropriate mapping. If a value is received that is not defined in the enum, the decoder should fall back to displaying the raw integer.

## 5. Code Generation

The `codegen` tool reads an events JSON file, or a YAML file when `PyYAML` is installed, and emits:

1. `axiom_events_generated.h` — C macros mapping `MODULE_EVENT_NAME` to `(module_id, event_id)`.
2. `axiom_modules_generated.h` — Module ID enum.
3. `dictionary.json` — Compact JSON for decoder consumption.

## 6. Validation Rules

- Every `(module_id, event_id)` must be unique.
- `level` must match the dictionary entry; the firmware encoder does not enforce this, but the validator warns on mismatch.
- `args` type sequence must match the order of encoded payload fields.
- `text` placeholders must reference exactly the declared `args`.
- Referenced `enum` names must exist in the `enums` section.

