> [English](CHANGELOG.md) | [简体中文](CHANGELOG_zh.md)

# Changelog

All notable changes to AxiomTrace will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Changed
- (none yet)

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
