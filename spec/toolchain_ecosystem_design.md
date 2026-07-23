> [English](toolchain_ecosystem_design.md) | [简体中文](toolchain_ecosystem_design_zh.md)

# Toolchain contract — AxiomTrace 1.0

This document describes implemented host tooling only. Live serial capture, a standalone benchmark CLI, RTOS integration packages, GUI, OTLP export, and transport provisioning are outside the 1.0 contract.

## Firmware-only path

A normal firmware build needs a GNU11 GCC/Clang compiler and CMake only when consuming the modular target. Python, YAML, code generation, and bundles are optional. The generated single-header follows:

```c
#define AXIOMTRACE_IMPLEMENTATION
#include "axiomtrace.h"
```

Define `AXIOMTRACE_NO_DEFAULT_PORT` in that implementation TU to omit generic weak defaults and provide all `axiom_port_*` functions in the hardware project.

## Installed commands

The `axiomtrace-tools` Python 3.12 package installs four commands:

| Command | Implemented responsibility |
| --- | --- |
| `axiom-decoder` | Read a binary file, resynchronize Wire frames, validate framing/CRC, and render `raw`, `text`, `json`, or `jsonl` |
| `axiom-codegen` | Convert JSON or optional YAML event definitions into C headers, dictionary JSON, source map, and metadata identity |
| `axiom-bundle` | Generate and inspect a versioned directory containing dictionary, source map, build information, and identity |
| `axiom-validate` | Validate definitions, dictionaries, bundles, golden vectors, and traces |

YAML support is optional; JSON requires no YAML dependency.

## Recommended learning path

1. Decode a captured binary file structurally: `axiom-decoder trace.bin --format raw`.
2. Add a handwritten JSON dictionary with `--dictionary`, or extract one from a documented X-Macro list.
3. Use `axiom-codegen` when generated IDs and headers become valuable.
4. Use metadata identity and `axiom-bundle` when traces must be matched deterministically to build metadata and source locations.

Wire v2 ordinary payloads are schema-driven packed values. Raw mode reports frame structure and payload bytes without claiming field semantics. Semantic modes require a matching dictionary or bundle. Reserved system events retain their fixed schemas from the event-model specification.

## Validation authority

- `tests/golden` is the byte-compatibility authority for normal Wire v2 events.
- `axiom-validate` is the authority for dictionary/bundle/trace consistency.
- CTest covers the C library, examples, single-header multi-TU linkage, and CMake consumers.
- Pytest covers the Python decoder and metadata tooling.

Host timing is a regression signal only. Target Flash, BSS, stack usage, and timing must come from the selected cross compiler and hardware integration; CI compile-only jobs do not constitute hardware certification.
