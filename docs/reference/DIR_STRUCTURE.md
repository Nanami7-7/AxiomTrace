> [English](DIR_STRUCTURE.md)

# AxiomTrace Directory Structure

```
AxiomTrace/
│
├── README.md                          # Project overview and quick start
├── LICENSE                            # GPL-3.0 license
├── CMakeLists.txt                     # Top-level build configuration
├── cmake/                             # CMake helper modules
│   └── AxiomTraceTools.cmake          # axiomtrace_add_bundle() integration
│
├── docs/                              # All documentation
│   ├── project/                       # Project management docs
│   │   ├── PLAN.md / PLAN_zh.md       # Roadmap and milestone definitions
│   │   ├── ROUTE.md / ROUTE_zh.md     # Version route (v0.1-core ~ v0.9-rc)
│   │   └── RULES.md / RULES_zh.md     # Hot-path rules, dev workflow, release gates
│   ├── changelog/                     # Release history
│   │   └── CHANGELOG.md / CHANGELOG_zh.md
│   ├── reference/                     # Technical references
│   │   ├── DIR_STRUCTURE.md           # This file — directory index
│   │   ├── platform_reference.md      # Platform-specific reference
│   │   └── porting_guide.md           # Porting guide for new platforms
│
├── spec/                              # Formal specifications (bilingual where public)
│   ├── event_model.md                 # Event record semantics, header layout, D2R
│   ├── wire_format.md                 # Binary serialization: framing, COBS, CRC
│   ├── backend_contract.md            # Backend interface contract
│   ├── api_reference.md               # Frontend macros, core API, config macros
│   ├── fault_capsule.md               # Fault capsule freeze and commit semantics
│   ├── event_dictionary.md            # YAML schema, enum mapping, dictionary validation
│   ├── decoder_protocol.md            # Decoder I/O protocol, dictionary format, CLI
│   ├── toolchain_ecosystem_design.md  # Toolchain, metadata bundle, codegen, validation
│   ├── versioning.md                  # Wire format versioning rules
│   ├── static_analysis.md             # Code quality and static analysis policy
│   └── minimalist_architecture_analysis.md  # Architecture evolution analysis
│
├── baremetal/                         # Firmware source (Five-Plane Architecture)
│   ├── axiom_config.h                 # Build-time configuration defaults
│   ├── axiomtrace.h                   # Shortcut: single-include header for quick start
│   ├── core/                          # [Core Plane] Ring buffer, encoding, CRC, timestamp, filter
│   │   ├── axiom_event.h/.c           # Event record assembly, write/flush/init API
│   │   ├── axiom_ring.h/.c            # Lock-free IRQ-safe RAM ring buffer
│   │   ├── axiom_encode.h             # Binary encoder with _Generic type dispatch
│   │   ├── axiom_crc.h/.c             # CRC-16/CCITT-FALSE lookup table
│   │   ├── axiom_timestamp.h/.c       # Delta-compressed timestamp encoding
│   │   └── axiom_filter.h/.c          # Level mask filter, drop statistics, DROP_SUMMARY
│   ├── frontend/                      # [Frontend Plane] User-facing macros
│   │   └── axiom_frontend.h           # AX_LOG/EVT/PROBE/FAULT/KV + profile control
│   ├── backend/                       # [Backend Plane] Backend contract and implementations
│   │   ├── axiom_backend.h/.c         # Backend registry and dispatcher
│   │   ├── axiom_backend_deferred.h/.c# Deferred ring → downstream chain
│   │   └── axiom_backend_memory.c     # Memory dump backend
│   ├── port/                          # [Port Layer — flat] Generic weak-symbol defaults
│   │   ├── axiom_port.h               # Port interface: timestamp, critical, fault_hook, flash
│   │   └── axiom_port_generic.c       # Host (desktop) weak default implementations
│   ├── ports/                         # [Port Layer — structured] Multi-platform ports
│   │   ├── generic/                   # Shared generic implementation
│   │   ├── arch/                      # CPU architecture ports
│   │   │   ├── cortex-m/              # Cortex-M: DWT_CYCCNT, SysTick, IRQ control
│   │   │   └── riscv/                 # RISC-V: mtime, CLINT
│   │   ├── soc/                       # SoC-specific ports
│   │   │   ├── esp32/
│   │   │   ├── nrf52/
│   │   │   └── stm32f4/
│   │   ├── esp32/                     # ESP32 port (UART backend + port implementation)
│   │   ├── nrf52/                     # nRF52 port (RTT backend + port implementation)
│   │   └── stm32/                     # STM32 port (UART backend + port implementation)
│   ├── include/                       # Supplemental headers
│   │   └── axiom_static_assert.h      # Static assert portability macro
│   └── examples/                      # Usage examples
│       ├── example_minimal.c          # Minimal: init + AX_EVT + flush
│       └── example_full.c             # Full: multi-backend, filter, capsule
│
├── tool/                              # Host toolchain (Python)
│   ├── pyproject.toml                 # Python project config
│   ├── scripts/                       # Utility scripts
│   │   ├── amalgamate.py              # Single-file library generator
│   │   └── (extract_dict.py)          # Dictionary extraction from X-Macro
│   ├── decoder/                       # Deprecated compatibility entrypoint
│   │   └── axiom_decoder.py           # Structural CLI wrapper around package decoder
│   ├── golden/                        # Golden frame regression tests
│   │   ├── frames/                    # Binary golden frame files (*.bin)
│   │   ├── expected/                  # Expected decoder output (JSON)
│   │   └── update_golden.py           # Golden frame generator
│   ├── benchmark/                     # Host benchmark
│   │   └── host_benchmark.c           # Encoding, CRC, ring write cycle counts
│   └── src/                           # pip-installable Python package
│       └── axiomtrace_tools/          # CLI tools: decode, codegen, bundle, validate
│           ├── decoder.py             # Raw binary frame parser
│           ├── dictionary.py          # dictionary.json loading and lookup
│           ├── render.py              # text/json/jsonl/raw renderers
│           ├── codegen.py             # events.yaml/json → C headers + dictionary
│           ├── bundle.py              # axiomtrace-bundle generation/loading
│           ├── source_map.py          # compile_commands.json → source_map.json
│           ├── metadata_id.py         # Decode-metadata identity hashing helpers
│           ├── validator.py           # Bundle/trace contract validation
│           └── capsule.py             # Fault-capsule decode/report support
│
├── tests/                             # Firmware test suite
│   ├── host/                          # Host-side C tests (CTest)
│   │   ├── test_ring.c                # Ring buffer overwrite/coverage/ISR safety
│   │   ├── test_crc.c                 # CRC-16 correctness and error detection
│   │   ├── test_event.c               # Frame assembly and parse consistency
│   │   └── test_filter.c              # Level filtering, drop stats, DROP_SUMMARY
│   ├── test_python_tools.py           # Python decoder golden regression test
│   └── test_integration.sh            # Shell integration test (build, run, verify)
│
├── examples/                          # External toolchain examples
│   ├── CMakeLists.txt                 # External project CMake config
│   ├── ports/                         # Example port configurations
│   └── example_precompiled/           # Pre-compiled single-file example
│
├── MSPM0G3507_Project/                # TI MSPM0G3507 reference project
│   ├── app/                           # Application layer (tasks, config)
│   ├── bsp/                           # Board Support Package
│   ├── hal/                           # Hardware Abstraction Layer
│   ├── osal/                          # OS Abstraction Layer
│   ├── freertos/                      # FreeRTOS config
│   ├── cmsis/                         # CMSIS system startup
│   ├── startup/                       # Startup code
│   └── linker/                        # Linker script
│
├── .github/                           # CI/CD
│   └── workflows/ci.yml               # GitHub Actions: build, test, lint
│
└── .gitignore                         # Git ignore rules
```
