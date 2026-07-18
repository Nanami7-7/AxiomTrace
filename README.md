> **English** | [简体中文](README_zh.md)

# AxiomTrace 1.0

AxiomTrace is a bounded, allocation-free Wire v2 event recorder for bare-metal C firmware. Firmware records IDs and packed values; host tools validate CRC and turn frames into raw JSON or dictionary-backed text/JSON. The normal ingress policy drops a new frame when the Core ring is full and reports recovery through `DROP_SUMMARY`.

## Supported capabilities

- C11 `AX_EVT`, `AX_LOG`, `AX_PROBE`, `AX_FAULT`, and `AX_KV` frontends.
- CRC-protected Wire v2 frames, filtering, drop diagnostics, Memory and Deferred backends.
- Capsule v1 pre/fault/post capture with explicit, segmented Flash commit.
- Modular CMake library and generated `axiomtrace.h` single-header distribution.
- Raw decoding without metadata; optional JSON/X-Macro dictionary, codegen, metadata identity, and bundle workflows.

The official compiler contract is GCC or Clang in GNU11 mode. Host GCC/Clang are test platforms; ARM GNU and RISC-V GNU jobs are compile-only reference checks. IAR, MSVC, Arm Compiler, vendor ports, and real MCU timing remain experimental unless separately measured by an integrating project.

## Quick start

Generate the release header:

```sh
python tool/scripts/amalgamate.py
```

Use it from exactly one implementation translation unit:

```c
/* axiomtrace_impl.c */
#define AXIOMTRACE_IMPLEMENTATION
#include "axiomtrace.h"
```

Application code registers a backend, emits, and uses `axiom_flush()` as the normal flush entry point:

```c
#include "axiomtrace.h"

static uint8_t trace[128];
static axiom_memory_backend_ctx_t memory_context;

int main(void) {
    axiom_backend_t memory =
        axiom_backend_memory("trace", trace, sizeof(trace), &memory_context);

    axiom_init();
    if (axiom_backend_register(&memory) != AXIOM_BACKEND_OK) return 1;
    AX_EVT(INFO, 0x03u, 0x0001u, (uint16_t)3200u);
    axiom_flush();

    return memory_context.head >= 12u ? 0 : 2;
}
```

Define `AXIOMTRACE_NO_DEFAULT_PORT` in the implementation TU when supplying your own `axiom_port_*` functions. Otherwise the single header includes generic weak defaults.

The repository example prints one complete frame as hexadecimal:

```sh
cmake -S . -B build -DAXIOM_BUILD_TESTS=ON -DAXIOM_BUILD_EXAMPLES=ON
cmake --build build
./build/baremetal/examples/example_minimal
```

After installing the Python tool, decode a binary trace structurally with no dictionary:

```sh
axiom-decoder trace.bin --format raw
```

Then add semantics progressively:

1. Keep raw IDs and raw decode for the smallest firmware integration.
2. Supply a handwritten JSON dictionary or extract one from X-Macros.
3. Enable codegen, metadata identity, bundle validation, and source locations only when needed.

## Build and consume with CMake

```sh
cmake -S . -B build -DAXIOM_BUILD_TESTS=ON -DAXIOM_BUILD_EXAMPLES=ON
cmake --build build
ctest --test-dir build --output-on-failure
cmake --install build --prefix install
```

Consumers may use either `add_subdirectory()` or an installed package:

```cmake
find_package(AxiomTrace 1.0 CONFIG REQUIRED)
target_link_libraries(firmware PRIVATE AxiomTrace::axiomtrace)
```

When AxiomTrace is a subproject, tests and examples default to off. Python, YAML parsing, codegen, and bundle generation are not required for a normal firmware build.

### Port selection

The top-level build selects only an architecture port:

```sh
cmake -S . -B build-cortex-m -DAXIOM_PLATFORM=cortex-m \
  -DAXIOM_BUILD_TESTS=OFF -DAXIOM_BUILD_EXAMPLES=OFF
```

`AXIOM_SOC` and `AXIOM_BOARD` are intentionally not top-level options. SDK-dependent integrations are separate packages under `baremetal/ports/stm32`, `baremetal/ports/nrf52`, and `baremetal/ports/esp32`; see the [porting guide](docs/reference/porting_guide.md) for their dependency boundaries.

## Resource presets

Preset RAM/Flash/stack reports are release artifacts; configured sizes are not claims about total linked firmware cost.

| Preset | Core ring | Payload | Capsule | Intended use |
| --- | ---: | ---: | --- | --- |
| `tiny` | 256 B | 32 B | off | smallest production integration |
| `prod` | 1 KiB | 64 B | off | production firmware |
| `field` | 2 KiB | 96 B | 2 KiB frame history | service builds |
| `dev` | 4 KiB | 128 B | 4 KiB frame history | host/development |
| `custom` | project-defined | project-defined | project-defined | explicit tuning |

The release budgets are Tiny RAM ≤512 B, Prod ≤1.5 KiB, Field ≤4.5 KiB, and Dev ≤9 KiB under ARM GNU MinSizeRel. Do not substitute Host measurements for target measurements.

## Documentation

- [API reference](spec/api_reference.md)
- [Wire format](spec/wire_format.md)
- [Backend contract](spec/backend_contract.md)
- [Fault capsule](spec/fault_capsule.md)
- [Toolchain design](spec/toolchain_ecosystem_design.md)
- [Porting guide](docs/reference/porting_guide.md)
- [Changelog](docs/changelog/CHANGELOG.md)

## License

[GNU General Public License v3.0](LICENSE)
