> [English](DIR_STRUCTURE.md)

# AxiomTrace Directory Structure

```text
AxiomTrace/
├── CMakeLists.txt                 # Core library, options, install package
├── cmake/                         # Reusable CMake helpers
│   ├── AxiomTracePresets.cmake
│   ├── AxiomTraceTools.cmake
│   └── AxiomTraceConfig.cmake.in
├── scripts/                       # Local CI, cleanup, release and budget checks
│   ├── ci.sh
│   ├── clean.sh
│   ├── benchmark_gate.py
│   ├── release_checks.py
│   └── resource_report.py
├── baremetal/                     # Firmware library
│   ├── axiom_config.h             # Configuration and version macros
│   ├── axiomtrace.h               # Public umbrella header
│   ├── core/                      # Frame, encoding, ring, filter, capsule, diagnostics
│   ├── frontend/                  # User-facing AX_* macros
│   ├── backend/                   # Backend contract, Memory and Deferred backends
│   ├── port/                      # Stable axiom_port_* declarations
│   ├── ports/                     # Host/architecture/vendor port implementations
│   │   ├── generic/
│   │   ├── arch/cortex-m/
│   │   ├── arch/riscv/
│   │   ├── stm32/                 # External STM32 + UART package
│   │   ├── nrf52/                 # External nRF52 + RTT package
│   │   └── esp32/                 # ESP-IDF Component package
│   └── examples/                  # Minimal and full C examples
├── tool/                          # Python host toolchain
│   ├── pyproject.toml
│   ├── uv.lock
│   ├── src/axiomtrace_tools/      # Decoder, bundle, codegen, validator, renderer
│   ├── decoder/                   # Standalone decoder compatibility entry point
│   ├── golden/                    # Golden frames, dictionary and updater
│   ├── benchmark/                 # Host benchmark source
│   └── scripts/                   # Amalgamation, dictionary and link utilities
├── tests/
│   ├── host/                      # C unit, integration and generated-header tests
│   ├── consumer/                  # add_subdirectory/find_package fixtures
│   ├── cmake/bundle_fixture/      # Metadata bundle CMake fixture
│   ├── test_python_tools.py
│   └── test_release_scripts.py
├── docs/                          # Project, reference and changelog documents
├── spec/                          # Versioned wire/API/toolchain specifications
└── .github/workflows/ci.yml       # Hosted CI entry point
```

Architecture-level selection is intentionally limited to `host`, `cortex-m`, and `riscv`. SoC/board-specific SDK integrations remain self-contained under their vendor directory instead of being represented by empty selector directories.
