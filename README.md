> [English](README.md) | [简体中文](README_zh.md)

# AxiomTrace v0.1 — Industrial Observability Microkernel

AxiomTrace is a high-performance, deterministic observability core designed for bare-metal MCU environments. It transforms the way embedded logs are handled by pushing complexity to the host and keeping the firmware hot path pure, fast, and O(1).

---

## 🚀 Quick Start

```c
#include "axiomtrace.h"

int main(void) {
    axiom_init();
    /* Structured event: O(1), ISR-safe, 0-malloc, 0-printf */
    AX_EVT(INFO, 0x03u, 0x0001u, (uint16_t)3200);
    return 0;
}
```

## 💎 The Industrial Pillars

### ⚡ Deterministic O(1) Performance
AxiomTrace guarantees that every log call executes in constant time. By using a **Blind Overwrite** policy and a bitwise-mask ring buffer, it avoids expensive frame boundary searches during interrupts.

### 🧬 Short Critical Section Encoding
Unlike traditional loggers that format text in the hot path, AxiomTrace packs bounded binary values and writes a complete frame to the ring in one critical section. The default `AXIOM_SHORT_CS=1` path trades a bounded stack buffer for lower interrupt latency.

### 🌐 Dual-Track Time Synchronization
Supports high-resolution relative counters for precise timing analysis, while allowing periodic Unix timestamp injection for real-world wall-clock alignment on the host side.

### 🎨 Rich Host-side Semantics
Keep your firmware binary lean by storing only raw IDs and integers. Use the **Host Dictionary** and standard **Metadata Bundle** to map IDs back to human-readable text, source locations, enums, physical units, and rich metadata.

---

## 🛠️ Key Features

- **Protocol-Entity Architecture**: Text/JSON/Binary are just views; the Event Record is the only truth.
- **Pluggable Backends**: UART, USB, RTT, SWO, or Flash Capsule — add new ones without touching the core. Use `AXIOM_BACKEND_INIT(...)` for forward-compatible struct initialization.
- **Fault Capsule**: `AX_FAULT` emits a fault-level Event Record, calls the platform fault hook, freezes the RAM pre/post window, and writes Flash only when user code explicitly calls `axiom_capsule_commit()`.
- **Profile-based Pruning**: `PROD` profile automatically removes debug probes and logs at compile-time.
- **Library Versioning**: Compile-time version check via `AXIOMTRACE_VERSION_CHECK(major, minor, patch)`.
- **Configurable Module Limits**: `AXIOM_MODULE_MAX` (default 32) controls the module filter bitmask width.
- **Bilingual Documentation**: Seamless transition between English and 简体中文 for global collaboration.

---

## 🏗️ Architecture

```text
AxiomTrace/
  Frontend Plane   AX_LOG / AX_EVT / AX_PROBE / AX_FAULT / AX_KV
  Core Plane       Packed Encode → CRC → Filter → Short Critical Ring Write
  Backend Plane    UART / RTT / USB / SWO / Flash Capsule / CAN-FD
  Tool Plane       Metadata Bundle / Python Decoder / Text Render / JSON Export / Golden Test
```

---

## 📦 Getting Started

### 1. Build & Test

```bash
cmake -B build -S . -DAXIOM_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

### 2. Python Toolchain

```bash
# Install the decoder
python -m pip install -e ./tool

# Build and test with the local CMake/Ninja toolchain
cmake -B build -S . -G Ninja -DAXIOM_BUILD_TESTS=ON -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build
ctest --test-dir build --output-on-failure

# Decode with the standard metadata bundle
axiom-codegen --events events.yaml --out build/generated
axiom-bundle generate --events events.yaml --compile-db build/compile_commands.json --out build/axiomtrace-bundle
axiom-decoder trace.bin --bundle build/axiomtrace-bundle --format text
```

Bundle-backed semantic decode requires the trace to emit the generated metadata identity once, using `AXIOM_EMIT_METADATA_ID()` from `axiom_metadata_id_generated.h`. The CMake helper described in the toolchain specification generates and wires this header.

Wire `v2.0` encodes ordinary event arguments as dictionary-defined packed values; metadata identity and optional source-location suffixes remain tagged. The reserved `AX_PROBE` system event retains typed fields because its value type varies without an application event schema. `AX_KV` event dictionaries must declare each packed key-hash/value pair in emission order. Use a metadata bundle for semantic `v2` decoding. The host decoder also retains structural support for historical typed-payload `v1.x` frames. JSON event definitions require no optional parser; YAML input requires `python -m pip install -e "./tool[yaml]"`.

### MCU resource presets

Use `AXIOM_PRESET` to move between resource classes without hand-tuning every macro:

```bash
cmake -B build-tiny -S . -G Ninja -DAXIOM_PRESET=tiny -DAXIOM_BUILD_TESTS=OFF
```

| Preset | Intended target | Key behavior |
| :--- | :--- | :--- |
| `custom` | project-owned tuning | no preset overrides; use individual `AXIOM_*` macros |
| `tiny` | very small MCU / strict ISR stack | PROD profile, 256B ring, 32B payload, no capsule, no time sync, per-phase write path |
| `prod` | production firmware | PROD profile, 1KB ring, 64B payload, no debug logs/probes, capsule off by default |
| `field` | service builds | FIELD profile, 2KB ring, capsule window enabled with smaller buffers |
| `dev` | default host/dev build | full diagnostics, 4KB ring, capsule enabled |

---

## 📚 Documentation Index

| Document | Description |
| :--- | :--- |
| [Directory Structure](docs/reference/DIR_STRUCTURE.md) | Complete file tree with plane annotations |
| [API Reference](spec/api_reference.md) | Frontend macros and Core control APIs |
| [Wire Format](spec/wire_format.md) | Binary serialization and framing (COBS) |
| [Event Model](spec/event_model.md) | Header layout, timestamp, and event semantics |
| [Dictionary Spec](spec/event_dictionary.md) | YAML schema and Enum mapping |
| [Toolchain Ecosystem](spec/toolchain_ecosystem_design.md) | Decoder, bundle, codegen, validation, and host-side workflow standards |
| [Rules & Policy](docs/project/RULES.md) | Engineering standards and hot-path mandates |
| [Fault Capsule](spec/fault_capsule.md) | Fault freeze, commit, and non-volatile storage |
| [Porting Guide](docs/reference/porting_guide.md) | How to port AxiomTrace to new MCU platforms |

---

## License

[GNU General Public License v3.0](LICENSE)
