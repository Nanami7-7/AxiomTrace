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
    AX_EVT(INFO, MOTOR, START, (uint16_t)3200); 
    return 0;
}
```

## 💎 The Industrial Pillars

### ⚡ Deterministic O(1) Performance
AxiomTrace guarantees that every log call executes in constant time. By using a **Blind Overwrite** policy and a bitwise-mask ring buffer, it avoids expensive frame boundary searches during interrupts.

### 🧬 Direct-to-Ring (D2R) Technology
Unlike traditional loggers that assemble frames on the stack, AxiomTrace writes data **directly into the ring buffer segments**. Coupled with **Incremental CRC**, it eliminates redundant memory copies and massive stack frame allocations.

### 🌐 Dual-Track Time Synchronization
Supports high-resolution relative counters for precise timing analysis, while allowing periodic Unix timestamp injection for real-world wall-clock alignment on the host side.

### 🎨 Rich Host-side Semantics
Keep your firmware binary lean by storing only raw IDs and integers. Use the **Host Dictionary** to map IDs back to human-readable text, enums, physical units, and rich metadata.

---

## 🛠️ Key Features

- **Protocol-Entity Architecture**: Text/JSON/Binary are just views; the Event Record is the only truth.
- **Pluggable Backends**: UART, USB, RTT, SWO, or Flash Capsule — add new ones without touching the core. Use `AXIOM_BACKEND_INIT(...)` for forward-compatible struct initialization.
- **Fault Capsule**: Automatically freezes pre/post windows during critical faults and commits to non-volatile storage.
- **Profile-based Pruning**: `PROD` profile automatically removes debug probes and logs at compile-time.
- **Library Versioning**: Compile-time version check via `AXIOMTRACE_VERSION_CHECK(major, minor, patch)`.
- **Configurable Module Limits**: `AXIOM_MODULE_MAX` (default 32) controls the module filter bitmask width.
- **Bilingual Documentation**: Seamless transition between English and 简体中文 for global collaboration.

---

## 🏗️ Architecture

```text
AxiomTrace/
  Frontend Plane   AX_LOG / AX_EVT / AX_PROBE / AX_FAULT / AX_KV
  Core Plane       Direct-to-Ring (D2R) → Incremental CRC → Filter → Bit-Mask Ring
  Backend Plane    UART / RTT / USB / SWO / Flash Capsule / CAN-FD
  Tool Plane       Python Decoder / Text Render / JSON Export / Golden Test
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
pip install ./tool

# Decode a binary stream
axiom-decoder trace.bin -d events.yaml -o text
```

---

## 📚 Documentation Index

| Document | Description |
| :--- | :--- |
| [Directory Structure](docs/reference/DIR_STRUCTURE.md) | Complete file tree with plane annotations |
| [API Reference](spec/api_reference.md) | Frontend macros and Core control APIs |
| [Wire Format](spec/wire_format.md) | Binary serialization and framing (COBS) |
| [Event Model](spec/event_model.md) | Header layout, timestamp, and D2R mechanics |
| [Dictionary Spec](spec/event_dictionary.md) | YAML schema and Enum mapping |
| [Rules & Policy](docs/project/RULES.md) | Engineering standards and hot-path mandates |
| [Fault Capsule](spec/fault_capsule.md) | Fault freeze, commit, and non-volatile storage |
| [Porting Guide](docs/reference/porting_guide.md) | How to port AxiomTrace to new MCU platforms |

---

## License

[GNU General Public License v3.0](LICENSE)
