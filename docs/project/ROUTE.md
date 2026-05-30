> [English](ROUTE.md) | [简体中文](ROUTE_zh.md)

# AxiomTrace Development Route

> Version: v1.0 | Status: **In Execution** | Update Date: 2026-05-28

---

## v0.1-core — Core Provable

**Goal**: Prove the minimal kernel is correct. No malloc, no printf, no blocking, O(1), ISR-writable.

- [x] Repository directory structure reorganized into five planes.
- [x] `PLAN.md` / `ROUTE.md` / `RULES.md` finalized.
- [x] `../../spec/event_model.md` finalized (Event Record semantics and versioned payload interpretation).
- [x] `../../spec/wire_format.md` finalized (current wire frame structure).
- [x] `../../spec/backend_contract.md` finalized (`axiom_backend_t` interface).
- [x] `../../spec/fault_capsule.md` finalized (capsule format and lifecycle).
- [x] `../../spec/api_reference.md` initial draft (Frontend macro APIs).
- [x] Lock-free ISR-safe RAM Ring `axiom_ring.c`.
- [x] Event Record assembly `axiom_event.c` (header + payload_len + payload + crc).
- [x] Binary encoder `axiom_encode.h` (`_Generic` type dispatch + v2 packed values).
- [x] CRC-16/CCITT-FALSE lookup table `axiom_crc.c`.
- [x] Compressed relative timestamp `axiom_timestamp.c`.
- [x] Level filtering and drop statistics `axiom_filter.c`.
- [x] Memory Backend `axiom_backend_memory.c`.
- [x] Host Unit Tests: ring, encoder, CRC, event, filter, backend, location, profile, dynamic call-chain, and benchmark coverage.
- [x] CMake build system adapted to new directories.

**Acceptance Criteria**:
- All `ctest` passing.
- Hot path `axiom_write()` cycle counts measurable and stable.
- No placeholder code.

---

## v0.2-wire — Wire Format Freezable

**Goal**: Protocol can be maintained long-term. encoder → frame → decoder → expected.json match exactly.

- [x] Frozen binary frame field order and size.
- [x] Generate first batch of golden frames (covering packed schemas, metadata suffixes, and boundary payloads).
- [x] Write `expected.json` (expected output for decoder).
- [x] Python decoder `../../tool/decoder/axiom_decoder.py` (parsing header/payload/crc).
- [x] Decoder explicitly rejects invalid frames with structured errors and no crash.
- [x] `../../tool/golden/update_golden.py` (encoder generates bin + expected.json).
- [x] Host regression test: `tests/test_python_tools.py` full comparison against golden frames.
- [x] Documentation update: `../../spec/wire_format.md` marked as **FROZEN**.

**Acceptance Criteria**:
- `encoder -> frame -> decoder -> expected.json` 100% match.
- Illegal frames (tampered CRC, payload_len, version) are explicitly rejected.
- `update_golden.py` auto-generates golden frames when packed schemas or metadata suffixes change.

---

## v0.3-frontend — Frontend Usable

**Goal**: Truly easy for users to start. Log the first event in 5 minutes.

- [x] `AX_LOG(msg)` macro implementation (developer-readable text; output via `axiom_port_string_out()` at runtime; expanded to empty in `PROD` profile).
- [x] `AX_EVT(level, mod, evt, args...)` macro implementation (structured events, `_Generic` encoded).
- [x] `AX_PROBE(tag, value)` macro implementation (high frequency, low disturbance, prunable).
- [x] `AX_FAULT(mod, evt, args...)` macro implementation (fault tracing, triggers fault hook).
- [x] `AX_KV(level, mod, evt, "k", v, ...)` macro implementation (lightweight KV pairs).
- [x] `DEV / FIELD / PROD` Profile compile-time pruning macros.
- [x] `axiom_frontend.h` unified entry (auto-includes all Frontend macros).
- [x] Example: `example_minimal.c` (start with 3 lines of code).
- [x] Example: `example_full.c` (multi-API combo + filter + backend).
- [x] Host tests: Macro expansion behavior verification under each Profile.

**Acceptance Criteria**:
- `example_minimal.c` compiles and runs on host `gcc` with zero config, outputting expected results.
- `AX_LOG` / `AX_PROBE` compile to nothing in `PROD` profile, no code/data generated.
- All Frontend APIs eventually call the same `axiom_write()`.

---

## v0.4-backend — Backend Pluggable

**Goal**: Full peripheral compatibility without being chip-bound. Adding a backend is zero-intrusive to the core.

- [x] `axiom_backend.c` registry and dispatcher implementation.
- [x] `axiom_backend_t` struct and `axiom_backend_register()` API.
- [x] Memory Backend refinement (direct export of ring buffer area).
- [x] UART Backend Template (COBS encoding + 0x00 delimiter, user fills UART transmit function).
- [ ] USB CDC Backend Template (bulk IN endpoint, user fills USB transmit function).
- [x] RTT Backend Template (SEGGER RTT up-channel, user fills SEGGER RTT function).
- [ ] SWO/ITM Backend Template (32-bit stimulus word stream, user fills ITM function).
- [ ] CAN-FD Backend Template (frame splitting and ID mapping, user fills CAN transmit function).
- [x] Backend drop callback and rate-limiting/degradation mechanism.
- [x] Host tests: `test_backend_ext.c` and `test_dynamic_call_chain.c` (registration, dispatch, drop, busy backend, degradation, recovery, panic_write).

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

- [x] Dual-zone ring design (Normal Ring + Capsule Ring).
- [x] `AX_FAULT()` triggers freeze: copies pre-window events to Capsule Ring.
- [x] Post-window events continue into Capsule Ring until full or window ends.
- [x] Register snapshot: `pc`, `lr`, `sp`, `xpsr` (via port layer).
- [x] Reset reason logging (via port layer).
- [x] Firmware hash (compile-time injected `__attribute__((section))` string, CRC verified).
- [x] Capsule CRC (covering all capsule contents).
- [x] `axiom_capsule_commit()`: write Capsule Ring contents to Flash (non-ISR).
- [x] `axiom_capsule_present()` / `axiom_capsule_read()` / `axiom_capsule_clear()`.
- [x] Flash power-loss recovery tests (integrity after erase/write interruption).
- [x] Host tests: `test_capsule.c` (freeze, pre/post windows, crc, read/clear).

**Acceptance Criteria**:
- Zero Flash writes during normal operation.
- Capsule is fully readable after Fault trigger, no pre/post event loss.
- Capsule data passes CRC check after power-loss simulation.

---

## v0.7-toolchain — Toolchain Complete

**Goal**: Binary logs are readable, testable, and exportable.

- [x] Decoder refinement: supports current packed frames, legacy typed frames, capsules, and `DROP_SUMMARY`.
- [x] Text render: dictionary template filling (`"motor current over limit: phase={u8}"`).
- [x] JSON export: full event stream exported as JSON array.
- [x] Standard metadata bundle: `manifest.json`, `dictionary.json`, `source_map.json`, `build_info.json`, optional `firmware.elf`/`.map`.
- [x] `axiom-bundle generate`: creates the standard bundle from event definitions, ELF, and compile database.
- [x] CMake integration: `axiomtrace_add_bundle(TARGET ... EVENTS ... OUTPUT_DIR ...)`.
- [x] Decoder bundle mode: `--bundle`, `--bundle-store`, metadata-id matching, and raw fallback.
- [x] Capsule report: host-side JSON/Markdown/HTML fault analysis report for the draft capsule format.
- [x] Dictionary validator: verify payload types match dictionary templates.
- [x] `../../tool/scripts/amalgamate.py`: merges core+frontend+port into a single-file header; GNU11 include smoke test passes.
- [ ] Amalgamated output passes the complete host test matrix as a release artifact.
- [x] `../../tool/scripts/extract_dict.py`: extracts `dictionary.json` from C source/X-Macros.
- [x] Benchmark tool: `tests/host/test_benchmark.c` and `../../tool/benchmark/host_benchmark.c` measure encode/CRC/ring write timing.
- [x] Golden regression: local `update_golden.py --check` and Python golden tests.
- [x] CI gate: auto-run `update_golden.py --check` + Python decoder regression tests on every relevant change.
- [x] Documentation governance: README indexes only; detailed contracts live in canonical specs; no unlinked ad-hoc Markdown.

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
- [x] Host cross-compiler validation: GCC + Clang. IAR remains optional and target-specific.

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
