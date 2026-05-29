> [English](static_analysis.md) | [简体中文](static_analysis_zh.md)

# AxiomTrace Quality Assurance and Static Analysis Design Document

> **Version**: v1.0-draft  
> **Role**: quality-assurance-designer  
> **Goal**: Establish a "Static Analysis Excellence" system for the AxiomTrace MCU bare-metal structured logging core, covering compile-time assertions, static analysis, CI/CD, the test pyramid, code standards, and documentation standards.

---

## Table of Contents

1. [Three-Layer Static Analysis System](#1-three-layer-static-analysis-system)
2. [CI/CD Pipeline](#2-ci-cd-pipeline)
3. [Test Pyramid](#3-test-pyramid)
4. [Code Quality Standards](#4-code-quality-standards)
5. [Documentation Standards](#5-documentation-standards)
6. [Summary: Implementation Path for Static Analysis Excellence](#6-summary-implementation-path-for-static-analysis-excellence)

---

## 1. Three-Layer Static Analysis System

### 1.1 L1 — Compile-Time Assertions (`_Static_assert` + Custom Macros)

AxiomTrace requires **zero-runtime-overhead** constraint validation. All constraints related to ABI, memory layout, and configuration values must be enforced at compile time.

#### 1.1.1 Existing Foundation

The project already provides `baremetal/include/axiom_static_assert.h`:

```c
#define AXIOM_STATIC_ASSERT(expr, msg) _Static_assert((expr), msg)
#define AXIOM_CHECK_SIZE(type, expected)      ...
#define AXIOM_CHECK_ALIGN(type, align)        ...
#define AXIOM_CHECK_FIELD_OFFSET(type, field, expected) ...
#define AXIOM_CHECK_RANGE(val, min, max)      ...
```

#### 1.1.2 Complete L1 Macro List and Use Cases

| Macro Name | Purpose | Use Case |
|------------|---------|----------|
| `AXIOM_STATIC_ASSERT(expr, msg)` | General C11 compile-time assertion | Base macro for all compile-time checks |
| `AXIOM_CHECK_SIZE(type, expected)` | Freezing struct/union size | `axiom_event_header_t` must be exactly 8 bytes; `axiom_backend_desc_t` size frozen per major version |
| `AXIOM_CHECK_ALIGN(type, align)` | Validation of alignment requirements | Ensure ring buffer elements meet 4-byte alignment (DMA friendly) |
| `AXIOM_CHECK_FIELD_OFFSET(type, field, expected)` | Freezing field offsets | `axiom_event_header_t` field offsets must match wire format (spec/event_model.md) |
| `AXIOM_CHECK_RANGE(val, min, max)` | Range checking for config values | `AXIOM_RING_BUFFER_SIZE` must be a power of two; `AXIOM_MAX_PAYLOAD_LEN <= 255` |
| `AXIOM_CHECK_POWER_OF_TWO(val)` | Power of two check | Ring buffer size, capsule buffer size |
| `AXIOM_CHECK_SAME_TYPE(a, b)` | Type consistency | Ensure `_Generic` branches correspond to dictionary-defined wire v2 value encodings |
| `AXIOM_CHECK_ARRAY_SIZE(arr, expected)` | Freezing array length | CRC lookup table must be 256 items; backend registry capacity |
| `AXIOM_CHECK_POINTER_SIZE(expected)` | Pointer size validation | Ensure 32-bit MCU assumptions hold (excludes 8/16-bit) |
| `AXIOM_CHECK_BIT_WIDTH(type, bits)` | Integer bit-width validation | `uint32_t` must be exactly 32 bits (`sizeof(type) * CHAR_BIT == bits`) |
| `AXIOM_CHECK_NO_MALLOC_FN(fn)` | Forbid malloc check | Static assertion at critical function pointer types (via macro wrapper) |

#### 1.1.3 L1 Usage Examples (Suggested for inclusion in source files)

```c
/* axiom_event.h — Already exists, suggest expansion */
AXIOM_CHECK_SIZE(axiom_event_header_t, 8);
AXIOM_CHECK_ALIGN(axiom_event_header_t, 1);
AXIOM_CHECK_FIELD_OFFSET(axiom_event_header_t, sync,       0);
AXIOM_CHECK_FIELD_OFFSET(axiom_event_header_t, version,    1);
AXIOM_CHECK_FIELD_OFFSET(axiom_event_header_t, level,      2);
AXIOM_CHECK_FIELD_OFFSET(axiom_event_header_t, module_id,  3);
AXIOM_CHECK_FIELD_OFFSET(axiom_event_header_t, event_id,   4);
AXIOM_CHECK_FIELD_OFFSET(axiom_event_header_t, seq,        6);

/* axiom_config.h — Config value range and power-of-two checks */
AXIOM_CHECK_RANGE(AXIOM_MAX_PAYLOAD_LEN, 1, 255);
AXIOM_CHECK_POWER_OF_TWO(AXIOM_RING_BUFFER_SIZE);
AXIOM_CHECK_RANGE(AXIOM_CAPSULE_PRE_EVENTS,  0, 256);
AXIOM_CHECK_RANGE(AXIOM_CAPSULE_POST_EVENTS, 0, 256);

/* axiom_crc.c — Lookup table size freezing */
AXIOM_CHECK_ARRAY_SIZE(crc16_table, 256);

/* axiom_port.h / Port implementation — 32-bit assumptions */
AXIOM_CHECK_POINTER_SIZE(4);
AXIOM_CHECK_BIT_WIDTH(uint32_t, 32);
AXIOM_CHECK_BIT_WIDTH(uint16_t, 16);
AXIOM_CHECK_BIT_WIDTH(uint8_t,   8);
```

#### 1.1.4 Supplementary L1 Macro Implementations

Suggested additions to `axiom_static_assert.h`:

```c
#define AXIOM_CHECK_POWER_OF_TWO(val) \
    AXIOM_STATIC_ASSERT(((val) != 0) && (((val) & ((val) - 1)) == 0), \
                        #val " must be a power of two")

#define AXIOM_CHECK_SAME_TYPE(a, b) \
    AXIOM_STATIC_ASSERT(__builtin_types_compatible_p(typeof(a), typeof(b)), \
                        #a " and " #b " must be the same type")

#define AXIOM_CHECK_ARRAY_SIZE(arr, expected) \
    AXIOM_STATIC_ASSERT((sizeof(arr) / sizeof((arr)[0])) == (expected), \
                        "Array " #arr " size mismatch")

#define AXIOM_CHECK_POINTER_SIZE(expected) \
    AXIOM_STATIC_ASSERT(sizeof(void*) == (expected), \
                        "Pointer size mismatch: expected " #expected)

#define AXIOM_CHECK_BIT_WIDTH(type, bits) \
    AXIOM_STATIC_ASSERT(sizeof(type) * CHAR_BIT == (bits), \
                        #type " bit width mismatch")
```

> **Note**: `__builtin_types_compatible_p` is a GCC/Clang extension; compatibility layers for IAR/Keil can fallback to runtime assertions or skip via `#ifdef`.

---

## 2. CI/CD Pipeline

### 2.1 GitHub Actions Workflow Overview

Design a primary workflow `.github/workflows/ci.yml` containing the following parallel/serial tasks:

```
┌─────────────────────────────────────────────────────────────┐
│                        CI Pipeline                           │
├─────────────────────────────────────────────────────────────┤
│  Stage 1: Fast Feedback                                      │
│    ├── lint-commit-msg    (Commit message format)            │
│    ├── lint-clang-format  (Code formatting)                  │
│    └── lint-clang-tidy    (Fast static analysis scan)        │
├─────────────────────────────────────────────────────────────┤
│  Stage 2: Build Matrix                                       │
│    ├── build-host-gcc     (x86_64-linux-gnu)                │
│    ├── build-host-clang   (x86_64-linux-gnu)                │
│    ├── build-arm-none-eabi-gcc (Cortex-M4, thumb)           │
│    └── build-arm-none-eabi-gcc (Cortex-M7, thumb/fpu)       │
├─────────────────────────────────────────────────────────────┤
│  Stage 3: Deep Analysis & Test                               │
│    ├── static-scan-build    (Path-sensitive analysis)        │
│    ├── static-cppcheck      (Cross-function analysis)        │
│    ├── test-host-unit       (Unity unit tests)               │
│    ├── test-golden-frame    (Golden Frame comparison)        │
│    ├── test-conformance     (Python conformance tests)       │
│    └── test-fuzz-smoke      (libFuzzer smoke test, 60s)      │
├─────────────────────────────────────────────────────────────┤
│  Stage 4: Quality Gate                                       │
│    ├── coverage-gcov        (Code coverage, threshold 85%)   │
│    ├── benchmark-smoke      (Benchmark smoke test)           │
│    └── docs-build           (Doxygen + MkDocs)               │
└─────────────────────────────────────────────────────────────┘
```

---

## 3. Test Pyramid

### 3.1 Pyramid Structure

```
         /\
        /  \   Fuzz Testing (libFuzzer)      — Exploratory, boundary cases
       /____\
      /      \  Conformance Testing (Python) — strict wire format compliance
     /________\
    /          \ Golden Frame Testing        — Byte-level binary comparison
   /____________\
  /              \ Benchmarking (Cycles/RAM/ROM) — Regession detection
 /________________\
/                  \
Host Unit Tests (Unity) — Encoder, Decoder, CRC, Ring buffer logic
```

### 3.2 Host Unit Tests (Unity Framework)

#### 3.2.1 Directory Structure

```
tests/
├── host/
│   ├── CMakeLists.txt
│   ├── test_ring_buffer.c      # Lock-free ring buffer tests
│   ├── test_encode_decode.c    # Encoder/Decoder symmetry tests
│   ├── test_crc.c              # CRC-16/CCITT lookup validation
│   ├── test_capsule.c          # Fault capsule state machine tests
│   ├── test_filter.c           # Level/Module mask filtering tests
│   ├── test_backend_dispatch.c # Backend dispatch and priority tests
│   └── unity/                  # Unity framework source
├── conformance/
│   ├── requirements.txt
│   ├── test_wire_format.py     # Frame format consistency
│   └── test_golden_frames.py   # Golden frame comparison
├── fuzz/
│   ├── CMakeLists.txt
│   └── fuzz_encode.c           # libFuzzer target
├── golden/
│   ├── frame_01_minimal.bin    # Minimal frame (0 payload)
│   ├── frame_02_u8_u16.bin     # Mixed type payload
│   ├── frame_03_full_payload.bin # 255 bytes payload
│   └── expected/
│       ├── frame_01.json
│       ├── frame_02.json
│       └── frame_03.json
└── benchmark/
    ├── CMakeLists.txt
    ├── bench_encode.c          # Encoding throughput
    ├── bench_ring.c            # Ring buffer throughput
    └── bench_crc.c             # CRC calculation speed
```

---

## 4. Code Quality Standards

### 4.1 Code Review Checklist

Every Reviewer must confirm before approving a PR:

#### Functional Correctness
- [ ] New features have corresponding unit test coverage.
- [ ] Golden Frame tests pass (if encoder is modified).
- [ ] Conformance tests pass (if wire format is modified).

#### Static Analysis
- [ ] Zero warnings from `clang-tidy` (at `WarningsAsErrors` level).
- [ ] No diff from `clang-format`.
- [ ] No errors from `cppcheck`.
- [ ] No high/medium bugs from `scan-build`.

#### Constraint Compliance
- [ ] Hot path is free of `malloc` / `calloc` / `realloc` / `free`.
- [ ] Hot path is free of `printf` / `sprintf` / `fprintf`.
- [ ] No blocking calls (e.g., `sleep`, `sem_wait`) in the `axiom_log_write` chain.
- [ ] New `_Static_assert` covers ABI / Layout / Configuration constraints.

#### Performance & Resources
- [ ] Cognitive complexity ≤ 15 (`readability-function-cognitive-complexity`).
- [ ] Function lines ≤ 80, statements ≤ 100.
- [ ] New global variables are justified and documented.
- [ ] Pointer arithmetic is limited to within the same array.

#### Compatibility & Documentation
- [ ] IAR/Keil compatibility layer considered (or marked as GCC/Clang only).
- [ ] Public APIs have Doxygen comments.
- [ ] Version number updated per SemVer (if ABI affected).
- [ ] `CHANGELOG.md` updated.

---

## 5. Documentation Standards

### 5.1 API Documentation (Doxygen)

**Comment Template**:

```c
/**
 * @brief Write a structured log event to the ring buffer.
 *
 * This is the primary hot-path entry point. It performs no dynamic
 * allocation, no formatting, and no blocking I/O.
 *
 * @param level      Log severity (DEBUG..FAULT).
 * @param module_id  Module identifier (0..255).
 * @param event_id   Per-module event identifier.
 * @param payload    Raw payload bytes (may be NULL if @p payload_len is 0).
 * @param payload_len Payload length in bytes (0..255).
 *
 * @pre payload_len <= AXIOM_MAX_PAYLOAD_LEN
 * @pre axiom_backend_init_all() has been called.
 *
 * @note IRQ-safe. May be called from interrupt context.
 */
void axiom_log_write(axiom_level_t level,
                     uint8_t module_id,
                     uint16_t event_id,
                     const uint8_t *payload,
                     uint8_t payload_len);
```

---

## 6. Summary: Implementation Path for Static Analysis Excellence

AxiomTrace's "Static Analysis Excellence" is achieved through a **three-layer defense + automated pipeline + test pyramid**:

### Layer 1: Compile-Time Assertions (L1) — Absolute Constraints with Zero Runtime Overhead

- Utilize C11 `_Static_assert` and GCC extensions to freeze all ABI contracts at the **compilation stage**: struct sizes, field offsets, alignment, config value ranges, and pointer widths.
- Any code violating ABI or config constraints **will fail to compile**, fundamentally preventing wire format incompatibilities caused by layout changes.
- Specifically for MCU scenarios, added power-of-two checks, array length freezing, and type consistency checks ensure critical data structures like ring buffers, CRC tables, and backend registries are validated at compile time.

### Layer 2: Compile-Time Analysis (L2) — Deep Rule Coverage with `clang-tidy`

- Enable `bugprone-*`, `clang-analyzer-*`, `cppcoreguidelines-*`, `performance-*`, `portability-*`, and `readability-*` categories to bring **path-sensitive analysis, performance patterns, and portability issues** into the compile-time feedback loop.
- Enforce **Cognitive Complexity ≤ 15** and **prefixed naming conventions** (`axiom_` / `AXIOM_`), ensuring code reaches industrial standards for readability and maintainability.

### Layer 3: Cross-Function Analysis (L3) — Global Perspective with `cppcheck` + `scan-build`

- `cppcheck` supplements cross-translation-unit analysis, detecting **uninitialized variable propagation, resource leaks, and redundant conditions** that single-file `clang-tidy` might miss.
- `scan-build` generates path-sensitive HTML reports, performing deep analysis on cross-unit call chains like `axiom_log_write` → `axiom_backend_dispatch`.

### Final Goal

> **Any change that violates "hot-path zero malloc/printf/blocking" constraints, breaks ABI compatibility, introduces undefined behavior, or reduces code readability shall fail to compile, fail static analysis, fail CI gates, and shall not enter the main branch.**

This is the implementation path for AxiomTrace's "Static Analysis Excellence."
