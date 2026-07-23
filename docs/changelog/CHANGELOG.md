> [English](CHANGELOG.md) | [简体中文](CHANGELOG_zh.md)

# Changelog

All notable changes to AxiomTrace will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Changed
- Top-level Port selection now has one explicit architecture path (`host`, `cortex-m`, or `riscv`); SDK-dependent vendor integrations remain self-contained.
- Documentation now matches the actual source tree and distinguishes core builds from vendor packages.

### Removed
- Unwired root examples, placeholder `ports/soc` selectors, and unused per-architecture CMake wrappers.

## [1.0.0] - 2026-07-15

### Added
- Public diagnostics counters for filtering, Core ring pressure, frontend overflow, invalid input, and backend loss.
- Release single header with one implementation TU mode, optional default Port, Memory/Deferred backends, and multi-TU tests.
- Installable `AxiomTrace::axiomtrace` CMake package and add-subdirectory/install consumer fixtures.

### Changed
- Core ingress now validates all public inputs, defaults to dropping new frames, and only overwrites complete old frames when explicitly configured.
- `axiom_flush()` now drains and validates the Core ring before cascading backend flush callbacks.
- Deferred buffering is independent of downstream readiness and retains failed frames for retry.
- Fault Capsule uses one record-aware frame ring and streams v1 images without a second full RAM image.
- README and toolchain contracts now distinguish tested Host behavior, compile-only reference ports, and experimental platforms.

## [0.7.0] - 2026-05-30

### Fixed
- **Concurrency**: Fixed race condition in `axiom_filter_drop()` — moved call inside critical section in both `axiom_write()` code paths (`AXIOM_SHORT_CS=0` and `AXIOM_SHORT_CS=1`) to protect `drop_count` read-modify-write from concurrent ISR pre-emption.
- **Correctness**: Fixed `axiom_selftest.c` encoder test index offset — `test_encoder_roundtrip()` verification indices didn't match actual encoding layout.
- **Correctness**: Fixed `axiom_selftest.c` implicit declaration of `axiom_port_log()` — switched to existing `axiom_port_string_out()` and added `#include "axiom_port.h"`.
- **Correctness**: Fixed `axiom_selftest.c` overflow test logic — old logic filled `AXIOM_MAX_PAYLOAD_LEN` then checked inequality in wrong direction.
- **Concurrency**: Fixed race condition in `axiom_timestamp_encode()` — moved call inside critical section in `axiom_write()` to prevent `s_ts_ctx.last_raw` corruption when pre-empted by a higher-priority ISR.
- **Concurrency**: Fixed race condition in drop summary reporting — snapshot `drop_count`/`drop_module`/`drop_event` inside critical section before clearing, preventing data corruption from pre-empting ISR.
- **Correctness**: Added `volatile` qualifier to `level_mask` and `module_mask` in `axiom_filter_t` to ensure correct ISR-context reads.
- **Correctness**: Fixed `axiom_timestamp_decode_len()` returning 3 for `0xFE` instead of 5 — caused frame parse misalignment and CRC failures when delta exceeded 21 bits.
- **Correctness**: Fixed `axiom_backend_deferred_init()` initializing the wrong ring instance, leaving `ring.mask = 0` which caused all deferred writes to overwrite `buf[0]`.
- **Correctness**: Fixed duplicate `AXIOM_MAX_PAYLOAD_LEN` definition — `axiom_event.h` (64) conflicted with `axiom_config.h` (128). Now `axiom_event.h` includes `axiom_config.h` as the single source of truth.
- **Correctness**: Fixed `axiom_enc_timestamp()` writing duplicate type tag (`[0x08][0x05][val...]` instead of `[0x08][val...]`).
- **Documentation**: Fixed stale `0xFF` references in timestamp encoding comments — encoder/decoder use `0xFE` as the 5-byte large-delta marker. Corrected in `axiom_event.h`, `wire_format.md`, and `wire_format_zh.md`.
- **Linkage**: Added `static` qualifier to `s_filter` global in `axiom_event.c` to prevent unintended symbol export.

### Changed
- **BREAKING / Wire v2.0**: Normal event arguments are now dictionary-defined packed values; tagged source-location and metadata-identity suffixes remain explicit. The host decoder keeps compatibility parsing for historical typed-payload v1.0/v1.1 frames.
- **Performance**: Optimized `axiom_flush()` and `deferred_flush()` to use new `axiom_ring_consume()` instead of redundant `axiom_ring_read()`, eliminating one `memcpy` per frame.
- **API**: Added `axiom_ring_consume()` to ring buffer API — advance tail without data copy.
- **Documentation**: Corrected ring buffer description from "Lock-free" to "IRQ-safe SPSC with critical-section protection".
- **Documentation**: Added production warning comments to default no-op port implementations in `axiom_port_generic.c`.
- **Documentation**: Added doc comments for `module_id < 32` filter limit, unused `baseline` field, and external `drop_summary_ready` / `drop_count_get_and_clear` API purpose.
- **Documentation**: English-ified all Chinese comments in `axiom_backend_deferred.h` and added inline descriptions to `AXIOM_BACKEND_CAP_*` flags.
- **Documentation**: Added thread-safety note to `axiom_backend_register()` — must be called before any `axiom_write()`.
- **BREAKING**: Complete v1.0 architecture refactor. The project pivots from v2.0 "text-first, Flash-default" design to v1.0 "Event Record as the single source of truth" micro-kernel model.
- Replaced all v2.0 placeholder implementations with v1.0 five-plane architecture (Semantic / Frontend / Core / Backend / Tool).
- Renamed and restructured `baremetal/` directory to align with v1.0 planes.

### Added
- **API**: Added `axiom_type_t` enum — named type tags (`AXIOM_TYPE_BOOL`, `AXIOM_TYPE_U8`, etc.) for payload encoding, replacing raw integer `#define` constants with type-safe enum while maintaining source-level backward compatibility.
- **API**: Added `axiom_backend_err_t` enum — named error codes (`AXIOM_BACKEND_OK`, `AXIOM_BACKEND_ERR_NULL`, `AXIOM_BACKEND_ERR_FULL`, `AXIOM_BACKEND_ERR_STRUCT`) replacing integer return values.
- **API**: Added `AXIOM_SYNC_BYTE`, `AXIOM_HEADER_LEN`, `AXIOM_CRC_LEN`, `AXIOM_MAX_TIMESTAMP_LEN`, `AXIOM_TAG_SIZE` named constants to replace hard-coded magic numbers in encoder and spec.
- **API**: Added `axiom_backend_count()` to query registered backend count, pairing with existing `axiom_backend_active_count()`.
- **Build**: Added CMake option `AXIOM_DEBUG` (default OFF) — global toggle for all module debug logging.
- **Tests**: Added 4 new extended tests: `test_encoder` (14 groups), `test_ring_ext` (9 groups), `test_event_ext` (7 groups), `test_backend_ext` (8 groups) covering encoder edge cases, ring buffer full/append/consume, event seq/header/filter mask, and backend register/multi-dispatch/panic.
- **Tests**: Extended `test_filter.c` to 9 groups covering all log levels, module mask, drop recording, and runtime mask set&get APIs.
- **Tests**: Extended `test_crc.c` with CRC-16 incremental update and whole-frame CRC consistency verification.
- **API**: Added library version macros (`AXIOMTRACE_VERSION_MAJOR/MINOR/PATCH`) and compile-time `AXIOMTRACE_VERSION_CHECK()` macro for downstream version gating.
- **API**: Added `AXIOM_MODULE_MAX` configurable constant (default 32) — replaces hardcoded `32` in filter logic.
- **API**: Added `AXIOM_DEPRECATED(msg)` cross-compiler macro (GCC/Clang/MSVC/IAR) for future deprecation annotations.
- **API**: Added `size` field to `axiom_backend_t` and `AXIOM_BACKEND_INIT(...)` designated-initializer wrapper — enables forward-compatible struct evolution. Old zero-initialized backends remain ABI-compatible (size==0 treated as legacy layout). `axiom_backend_register()` rejects undersized structs with `-3`.
- `../project/RULES.md`: Enforced development rules including hot-path prohibitions (no malloc, no printf, no Flash erase), drop-summary requirements, and mandatory issue→design→spec→golden→impl→decoder→tests→benchmark→docs→changelog workflow.
- `../project/PLAN.md` v1.0: Frozen target architecture, release gates, and design philosophy.
- `../project/ROUTE.md` v1.0: Development stages from v0.1-core through v0.9-rc to v1.0-stable.
- `../../spec/api_reference.md`: Frontend API reference for `AX_LOG`, `AX_EVT`, `AX_PROBE`, `AX_FAULT`, `AX_KV`.
- `../../spec/decoder_protocol.md`: Decoder input/output protocol and dictionary format specification.

### Removed
- v2.0 `axiom_storage_t` unified storage abstraction (replaced by Backend Contract).
- v2.0 `AXIOM_LOG("fmt", ...)` printf-like runtime string hashing (replaced by structured Event Record macros).
- v2.0 linker-section auto-registration backend system (replaced by explicit `axiom_backend_register()`).

## [0.6.0] - 2026-05-27

### Added
- **API**: Added `axiom_type_t` enum — named type tags (`AXIOM_TYPE_BOOL`, `AXIOM_TYPE_U8`, etc.) for payload encoding, replacing raw integer `#define` constants.
- **API**: Added `AXIOM_SYNC_BYTE`, `AXIOM_HEADER_LEN`, `AXIOM_CRC_LEN`, `AXIOM_MAX_TIMESTAMP_LEN`, `AXIOM_TAG_SIZE` named constants replacing hard-coded magic numbers in encoder and spec.
- **Tests**: Added 4 new extended test suites — `test_encoder` (14 groups), `test_ring_ext` (9 groups), `test_event_ext` (7 groups), `test_backend_ext` (8 groups).
- **Tests**: Extended `test_filter.c` to 9 groups covering all log levels, module mask, drop recording, and runtime mask set&get APIs.
- **Tests**: Extended `test_crc.c` with CRC-16 incremental update and whole-frame CRC consistency verification.
- **Benchmark**: Added MCU realistic benchmark suite with P99.9/P99.99 percentiles and ARM instruction-count estimation.
- **Tool**: Added metadata bundle system — `axiom-bundle generate`, `axiom-codegen`, dictionary validator, source map, capsule report.
- **Tool**: Added text render (`render.py`) for dictionary template filling.
- **CMake**: Added `AxiomTraceTools.cmake` for CMake bundle integration.
- **Tests**: Added source location test (`test_location.c`) and golden frame generator (`generate_golden.c`).

### Changed
- **BREAKING / Wire v2.0**: Event payloads migrated to dictionary-defined packed values; metadata identity and source-location suffixes remain tagged.
- **Tool**: Python CLI restructured with `axiom-codegen`, `axiom-bundle`, `axiom-decoder` subcommands.
- **Tool**: Decoder refactored with new payload parser supporting packed v2 and legacy typed frames.
- **Docs**: All specification documents synced with latest API changes (EN + ZH).

### Fixed
- **Concurrency**: Fixed race condition in `axiom_filter_drop()` — moved call inside critical section in `axiom_write()`.
- **Concurrency**: Fixed drop summary report race condition — snapshot inside critical section before clearing.

## [0.4.0] - 2026-05-07

### Added
- **API**: Added `axiom_backend_err_t` enum — named error codes replacing integer return values.
- **API**: Added `axiom_ring_consume()` to ring buffer API — advance tail without data copy.
- **API**: Added library version macros (`AXIOMTRACE_VERSION_MAJOR/MINOR/PATCH`) and compile-time `AXIOMTRACE_VERSION_CHECK()` macro.
- **API**: Added `AXIOM_MODULE_MAX` configurable constant (default 32).
- **API**: Added `AXIOM_DEPRECATED(msg)` cross-compiler macro.
- **API**: Added `size` field to `axiom_backend_t` and `AXIOM_BACKEND_INIT(...)` for forward-compatible struct evolution.

### Changed
- **Performance**: Optimized `axiom_flush()` to use `axiom_ring_consume()` instead of redundant `axiom_ring_read()`.
- **Documentation**: Corrected ring buffer description from "Lock-free" to "IRQ-safe SPSC with critical-section protection".
- **Documentation**: Synced all spec documents with latest API.

### Fixed
- **Correctness**: Fixed `axiom_timestamp_decode_len()` returning 3 for `0xFE` instead of 5.
- **Correctness**: Fixed race condition in `axiom_timestamp_encode()`.
- **Correctness**: Fixed `axiom_enc_timestamp()` writing duplicate type tag.
- **Correctness**: Fixed `axiom_backend_deferred_init()` initializing the wrong ring instance.
- **Correctness**: Fixed duplicate `AXIOM_MAX_PAYLOAD_LEN` definition.
- **Correctness**: Added `volatile` qualifier to `level_mask` and `module_mask` in `axiom_filter_t`.
- **Linkage**: Added `static` qualifier to `s_filter` global in `axiom_event.c`.

## [0.3.0] - 2026-05-02

### Fixed
- **Correctness**: Fixed timestamp encoding using `0xFF` as large delta marker — changed to `0xFE`.
- **Correctness**: Added power-of-2 assertion for ring buffer size.

## [0.2.0] - 2026-05-01

### Added
- **CI**: Added GitHub Actions pipeline with cppcheck and scan-build static analysis.
- **CI**: Added Cortex-M QEMU cross-compilation CI target.
- **Docs**: Added `DIR_STRUCTURE.md` directory overview document.
- **Docs**: Restructured project documents into `docs/` subdirectories.

### Changed
- **License**: Switched from MIT to GPL-3.0.
- **CI**: Stripped CI to minimal build-and-test pipeline for reliability.

### Fixed
- **CI**: Fixed cppcheck include paths for three-layer port architecture.
- **CI**: Fixed Cortex-M QEMU test — added ARM cross-compiler flags.
- **Docs**: Fixed bilingual links across all markdown files.
- **Docs**: Fixed README license misstatement.

## [0.1.0] - 2026-05-01

### Added
- **Core**: Lock-free ISR-safe RAM ring buffer (`axiom_ring.c`) with blind-overwrite policy.
- **Core**: Event Record assembly (`axiom_event.c`) with header, payload_len, payload, and CRC-16.
- **Core**: Binary encoder (`axiom_encode.h`) with `_Generic` type dispatch.
- **Core**: CRC-16/CCITT-FALSE with lookup table (`axiom_crc.c`).
- **Core**: Compressed relative timestamp with delta encoding (`axiom_timestamp.c`).
- **Core**: Level filtering and drop statistics (`axiom_filter.c`).
- **Backend**: Backend registry and dispatcher (`axiom_backend.c`) with `axiom_backend_register()` API.
- **Backend**: Memory Backend for host testing.
- **Backend**: Deferred Backend with ring-based dispatch.
- **Frontend**: `AX_LOG`, `AX_EVT`, `AX_PROBE`, `AX_FAULT`, `AX_KV` macro APIs with `DEV/FIELD/PROD` profile pruning.
- **Port**: Three-layer port architecture — generic host, Cortex-M, RISC-V, ESP32, nRF52, STM32.
- **Tool**: Python decoder, golden frame generator, and amalgamation script.
- **Tests**: Host unit tests for ring, encoder, CRC, event, filter, backend, location, profile, and benchmark.
- **Build**: CMake build system with `AXIOM_PRESET` resource presets (tiny/prod/field/dev).
- **Spec**: Event model, wire format, backend contract, fault capsule, API reference, decoder protocol, event dictionary, versioning, and toolchain ecosystem design specifications (all bilingual).
- **Docs**: `PLAN.md`, `ROUTE.md`, `RULES.md` project governance documents (all bilingual).
- **Examples**: `example_minimal.c` (3-line quick start) and `example_full.c` (multi-API combo).

### Changed
- **Performance**: D2R (Direct-to-Ring) O(1) write path with incremental CRC — single critical section for complete frame write.
