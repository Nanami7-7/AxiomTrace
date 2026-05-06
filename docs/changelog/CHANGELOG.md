> [English](CHANGELOG.md) | [简体中文](CHANGELOG_zh.md)

# Changelog

All notable changes to AxiomTrace will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Fixed
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
- `../project/RULES.md`: Enforced development rules including hot-path prohibitions (no malloc, no printf, no Flash erase), drop-summary requirements, and mandatory issue→design→spec→golden→impl→decoder→tests→benchmark→docs→changelog workflow.
- `../project/PLAN.md` v1.0: Frozen target architecture, release gates, and design philosophy.
- `../project/ROUTE.md` v1.0: Development stages from v0.1-core through v0.9-rc to v1.0-stable.
- `../../spec/api_reference.md`: Frontend API reference for `AX_LOG`, `AX_EVT`, `AX_PROBE`, `AX_FAULT`, `AX_KV`.
- `../../spec/decoder_protocol.md`: Decoder input/output protocol and dictionary format specification.

### Removed
- v2.0 `axiom_storage_t` unified storage abstraction (replaced by Backend Contract).
- v2.0 `AXIOM_LOG("fmt", ...)` printf-like runtime string hashing (replaced by structured Event Record macros).
- v2.0 linker-section auto-registration backend system (replaced by explicit `axiom_backend_register()`).

## [0.1.0] - TBD

### Added
- Core Plane: IRQ-safe SPSC RAM Ring buffer with critical-section protection, compile-time configurable size and policy (DROP / OVERWRITE).
- Core Plane: Binary Event Record encoder with C11 `_Generic` type-safe payload encoding and self-describing type tags.
- Core Plane: CRC-16/CCITT-FALSE with 256-byte ROM lookup table.
- Core Plane: Compressed relative timestamp (delta encoding).
- Core Plane: Runtime level/module filter and drop statistics with DROP_SUMMARY generation.
- Frontend Plane: `AX_LOG`, `AX_EVT`, `AX_PROBE`, `AX_FAULT`, `AX_KV` macros with DEV/FIELD/PROD Profile compile-time trimming.
- Backend Plane: Unified `axiom_backend_t` contract with `write/ready/flush/panic_write/on_drop`.
- Backend Plane: Memory, UART, USB CDC, RTT, SWO/ITM, CAN-FD backend templates.
- Fault Capsule: Pre-window freeze, post-window capture, register snapshot, firmware hash, capsule CRC, flash commit.
- Tool Plane: Python decoder, text renderer, JSON exporter, capsule reporter, golden frame manager, benchmark tool.
- Host unit tests and Python regression tests.
- Single-file library generator (`../../tool/scripts/amalgamate.py`).