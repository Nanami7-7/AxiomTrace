> [English](ROUTE.md) | [简体中文](ROUTE_zh.md)

# AxiomTrace Development Route

> Version: v1.0 | Status: **In Execution** | Update Date: 2026-04-29

---

## v0.1-core — Core Provable

**Goal**: Prove the minimal kernel is correct. No malloc, no printf, no blocking, O(1), ISR-writable.

- [x] Repository directory structure reorganized into five planes.
- [x] `PLAN.md` / `ROUTE.md` / `RULES.md` finalized.
- [ ] `../../spec/event_model.md` finalized (Event Record semantics and versioned payload interpretation).
- [ ] `../../spec/wire_format.md` finalized (current wire frame structure).
- [ ] `../../spec/backend_contract.md` finalized (`axiom_backend_t` interface).
- [ ] `../../spec/fault_capsule.md` finalized (capsule format and lifecycle).
- [ ] `../../spec/api_reference.md` initial draft (Frontend macro APIs).
- [ ] Lock-free ISR-safe RAM Ring `axiom_ring.c`.
- [ ] Event Record assembly `axiom_event.c` (header + payload_len + payload + crc).
- [ ] Binary encoder `axiom_encode.c` (`_Generic` type dispatch + v2 packed values).
- [ ] CRC-16/CCITT-FALSE lookup table `axiom_crc.c`.
- [ ] Compressed relative timestamp `axiom_timestamp.c`.
- [ ] Level filtering and drop statistics `axiom_filter.c`.
- [ ] Memory Backend `axiom_backend_memory.c`.
- [ ] Host Unit Tests: `test_ring.c`, `test_encode.c`, `test_crc.c`, `test_event.c`, `test_filter.c`.
- [ ] CMake build system adapted to new directories.

**Acceptance Criteria**:
- All `ctest` passing.
- Hot path `axiom_write()` cycle counts measurable and stable.
- No placeholder code.

---

## v0.2-wire — Wire Format Freezable

**Goal**: Protocol can be maintained long-term. encoder → frame → decoder → expected.json match exactly.

- [ ] Frozen binary frame field order and size.
- [ ] Generate first batch of golden frames (covering packed schemas, metadata suffixes, and boundary payloads).
- [ ] Write `expected.json` (expected output for decoder).
- [ ] Python decoder `../../tool/decoder/axiom_decoder.py` (parsing header/payload/crc).
- [ ] Decoder explicitly rejects invalid frames (returns `FRAME_INVALID`, no crash).
- [ ] `../../tool/golden/update_golden.py` (encoder generates bin + expected.json).
- [ ] Host regression test: `test_decoder.py` full comparison against golden frames.
- [ ] Documentation update: `../../spec/wire_format.md` marked as **FROZEN**.

**Acceptance Criteria**:
- `encoder -> frame -> decoder -> expected.json` 100% match.
- Illegal frames (tampered CRC, payload_len, version) are explicitly rejected.
- `update_golden.py` auto-generates golden frames when packed schemas or metadata suffixes change.

---

## v0.3-frontend — Frontend Usable

**Goal**: Truly easy for users to start. Log the first event in 5 minutes.

- [ ] `AX_LOG(msg)` macro implementation (developer-readable text; output via port `byte_out` at runtime; expanded to empty in `PROD` profile).
- [ ] `AX_EVT(level, mod, evt, args...)` macro implementation (structured events, `_Generic` encoded).
- [ ] `AX_PROBE(tag, value)` macro implementation (high frequency, low disturbance, prunable).
- [ ] `AX_FAULT(mod, evt, args...)` macro implementation (fault tracing, triggers capsule hook).
- [ ] `AX_KV(level, mod, evt, "k", v, ...)` macro implementation (lightweight KV pairs).
- [ ] `DEV / FIELD / PROD` Profile compile-time pruning macros.
- [ ] `axiom_frontend.h` unified entry (auto-includes all Frontend macros).
- [ ] Example: `example_minimal.c` (start with 3 lines of code).
- [ ] Example: `example_full.c` (multi-API combo + filter + backend).
- [ ] Host tests: Macro expansion behavior verification under each Profile.

**Acceptance Criteria**:
- `example_minimal.c` compiles and runs on host `gcc` with zero config, outputting expected results.
- `AX_LOG` / `AX_PROBE` compile to nothing in `PROD` profile, no code/data generated.
- All Frontend APIs eventually call the same `axiom_write()`.

---

## v0.4-backend — Backend Pluggable

**Goal**: Full peripheral compatibility without being chip-bound. Adding a backend is zero-intrusive to the core.

- [ ] `axiom_backend.c` registry and dispatcher implementation.
- [ ] `axiom_backend_t` struct and `axiom_backend_register()` API.
- [ ] Memory Backend refinement (direct export of ring buffer area).
- [ ] UART Backend Template (COBS encoding + 0x00 delimiter, user fills UART transmit function).
- [ ] USB CDC Backend Template (bulk IN endpoint, user fills USB transmit function).
- [ ] RTT Backend Template (SEGGER RTT up-channel, user fills SEGGER RTT function).
- [ ] SWO/ITM Backend Template (32-bit stimulus word stream, user fills ITM function).
- [ ] CAN-FD Backend Template (frame splitting and ID mapping, user fills CAN transmit function).
- [ ] Backend drop callback and rate-limiting mechanism.
- [ ] Host tests: `test_backend.c` (registration, dispatch, drop, panic_write).

**Acceptance Criteria**:
- Adding a new backend only requires: implementing 3 functions + calling `axiom_backend_register()`, no changes to `core/`.
- Backend returns `-EAGAIN` when busy; Core correctly increments drop counter.
- All templates compile on host with mock functions.

---

## v0.5-probe — Probe Timing Investigation Mature

**Goal**: High-frequency signal tracking without disturbing business logic.

- [ ] `AX_PROBE(tag, value)` high-frequency optimization (bypass filter, minimal header, optional no-crc).
- [ ] Independent Probe ring buffer (isolated from main ring to avoid overwriting critical logs).
- [ ] Probe backend: SWO/ITM (recommended, lossless output).
- [ ] Probe decoder support (restores probe stream into timing waveforms).
- [ ] Benchmark: Probe write cycles < 100 cycles (Cortex-M4 @ 80MHz).
- [ ] Host tests: Probe ring independence and overwrite policy testing.

**Acceptance Criteria**:
- 1000Hz probe does not drop main ring events.
- Probe data exportable to CSV/JSON timing via decoder.

---

## v0.6-capsule — Fault Capsule Mature

**Goal**: Fault traceability, forming core differentiation.

- [ ] Dual-zone ring design (Normal Ring + Capsule Ring).
- [ ] `AX_FAULT()` triggers freeze: copies pre-window events to Capsule Ring.
- [ ] Post-window events continue into Capsule Ring until full or window ends.
- [ ] Register snapshot: `pc`, `lr`, `sp`, `xpsr` (via port layer).
- [ ] Reset reason logging (via port layer).
- [ ] Firmware hash (compile-time injected `__attribute__((section))` string, CRC verified).
- [ ] Capsule CRC (covering all capsule contents).
- [ ] `axiom_capsule_commit()`: write Capsule Ring contents to Flash (non-ISR).
- [ ] `axiom_capsule_present()` / `axiom_capsule_read()` / `axiom_capsule_clear()`.
- [ ] Flash power-loss recovery tests (integrity after erase/write interruption).
- [ ] Host tests: `test_capsule.c` (freeze, pre/post windows, crc, read/clear).

**Acceptance Criteria**:
- Zero Flash writes during normal operation.
- Capsule is fully readable after Fault trigger, no pre/post event loss.
- Capsule data passes CRC check after power-loss simulation.

---

## v0.7-toolchain — Toolchain Complete

**Goal**: Binary logs are readable, testable, and exportable.

- [ ] Decoder refinement: supports current packed frames, legacy typed frames, capsules, and `DROP_SUMMARY`.
- [ ] Text render: dictionary template filling (`"motor current over limit: phase={u8}"`).
- [ ] JSON export: full event stream exported as JSON array.
- [x] Standard metadata bundle: `manifest.json`, `dictionary.json`, `source_map.json`, `build_info.json`, optional `firmware.elf`/`.map`.
- [x] `axiom-bundle generate`: creates the standard bundle from event definitions, ELF, and compile database.
- [x] CMake integration: `axiomtrace_add_bundle(TARGET ... EVENTS ... OUTPUT_DIR ...)`.
- [x] Decoder bundle mode: `--bundle`, `--bundle-store`, metadata-id matching, and raw fallback.
- [ ] Capsule report: HTML/Markdown fault analysis report.
- [x] Dictionary validator: verify payload types match dictionary templates.
- [ ] `../../tool/scripts/amalgamate.py`: merges core+frontend+port into single-file `axiomtrace.h`.
- [ ] `../../tool/scripts/extract_dict.py`: extracts `dictionary.json` from C source/X-Macros.
- [ ] Benchmark tool: `../../tool/benchmark/host_benchmark.c` measures encode/CRC/ring write cycles.
- [ ] Golden regression: CI auto-runs `update_golden.py` + `test_decoder.py`.
- [ ] Documentation governance: README indexes only; detailed contracts live in canonical specs; no unlinked ad-hoc Markdown.

**Acceptance Criteria**:
- `binary -> text` passes all golden frames.
- `binary -> json` has correct structure and complete fields.
- `trace -> bundle -> text/jsonl` restores dictionary semantics and source location when enabled.
- `trace -> bundle-store` refuses mismatched firmware identities unless raw mode is requested.
- `capsule -> report` contains registers, event sequences, and firmware hash.
- Amalgamated output passes all host tests.

---

## v0.8-portability — Cross-platform Validation

**Goal**: Prove it runs on more than one type of MCU.

- [ ] Cortex-M port refinement (DWT_CYCCNT / SysTick / DisableIRQ / Flash HAL hook).
- [ ] RISC-V port refinement (mtime / CLINT / Interrupt control / Flash hook).
- [ ] Generic port (host simulation) refinement as the default CI environment.
- [ ] Integration validation in at least 2 real MCU projects (different vendors).
- [ ] Integration docs: `../reference/porting_guide.md`.
- [ ] Cross-compiler validation: GCC, Clang, IAR (optional).

**Acceptance Criteria**:
- Same `core/` + `frontend/` code compiles across platforms just by swapping `port/`.
- Port layer provides weak symbol defaults for missing functions; compilation does not fail.

---

## v0.9-rc — Release Candidate

**Goal**: No new features, only bug fixing.

- [ ] API review (naming consistency, macro behavior consistency, doc/code alignment).
- [ ] Spec review (all `../../spec/*.md` checked against implementation).
- [ ] Backend review (all templates compile with mock drivers).
- [ ] Capsule review (freeze / commit / dump full path test passed).
- [ ] Benchmark report finalized (cycles locked, recorded in `../reference/benchmark.md`).
- [ ] Fuzz malformed frame (custom fuzz target: random tampering, decoder no-crash).
- [ ] Fault injection (simulate HardFault, verify capsule capture integrity).
- [ ] Power-loss simulation (Flash erase/write power-loss, capsule recovery).
- [ ] Docs complete (README, api_reference, porting_guide, example comments).
- [ ] Examples complete (all independent, compilable, runnable, outputs correct).
- [ ] All P0/P1 issues cleared.

---

## v1.0-stable — Stable Release

**Goal**: Official v1.0.

- [ ] Meet all release thresholds in `PLAN.md` §4.
- [ ] Meet all release thresholds in `RULES.md` §10.
- [ ] Git tag: `git tag v1.0-stable`.
- [ ] Release Note (binaries, single-file library `axiomtrace.h`, decoder, docs).
- [ ] Announcement: Developer-readable, Production-low-disturbance, Fault-traceable, Protocol-freezable, Toolchain-trusted.
