> [English](RULES.md) | [简体中文](RULES_zh.md)

# AxiomTrace RULES.md

> Version: v1.0 | Status: **Enforced** | Update Date: 2026-05-28

---

## 1. Project Purpose (Unshakeable)

AxiomTrace is an **MCU Bare-metal Observability Microkernel**. Our firm goals:

- **Easy to Use**: Developers can log their first structured event within 5 minutes. APIs have clear semantics, intuitive naming, and complete documentation.
- **Lightweight & Efficient**: Hot path O(1), no malloc, no printf, no blocking, ISR-writable. Minimum configuration: Flash < 4KB, RAM < 2KB.
- **Extensibility**: Pluggable Backends, extensible Payload types, prunable Profiles, forward-compatible protocol. Adding a backend does not modify the Core.
- **User Friendly**: Zero-config start, single-file library for quick entry, progressive APIs (from hardcoded IDs to X-Macros to YAML). Clear error messages and examples covering all scenarios.
- **Trusted & Releasable**: Frozen protocol, verifiable toolchain, regression-ready golden tests, traceable faults.

**What it is NOT**:
- Not a printf replacement.
- Not an RTOS/Linux logging library.
- Not a full filesystem.
- Not a chip HAL encapsulation library.

---

## 2. Hot Path Iron Rules (Prohibitions)

The following behaviors are **strictly prohibited** in `axiom_write()` and all ISR call paths:

| Prohibition | Reason | Consequence of Violation |
| :--- | :--- | :--- |
| `malloc` / `free` | Non-deterministic heap allocation, potential blocking, fragmentation. | Rejection of PR. |
| `printf` / `sprintf` / `vsnprintf` | Large code size, high stack consumption, non-deterministic. | Rejection of PR. |
| `snprintf` formatting | Same as above. | Rejection of PR. |
| Flash `erase` | Takes hundreds of milliseconds, blocks the system. | Rejection of PR; only allowed in non-hot paths for Capsule commit. |
| Flash `write` | Same as above; only allowed for Capsule commit. | Rejection of PR. |
| Blocking for backend ready | Backend should return `-EAGAIN` when busy; Core updates drop counter. | Rejection of PR. |
| Copying large data in loops | Must use fixed frame lengths; complete in a single critical section. | Rejection of PR. |
| Floating point (Hot path) | Some MCUs lack FPU; software FP is extremely slow. | `_Generic` allowed for `float`, but use `memcpy` for bitwise copy during encoding. |
| String compare/search | Runtime string parsing is prohibited. | Rejection of PR. |

**Allowed Items**:
- Packed frame encoding with a short critical section.
- Blind Overwrite strategy for O(1) deterministic latency when the ring is full.
- Incremental CRC (streaming calculation without re-reading memory).
- Writing to RAM Ring (single critical section).
- Updating global drop counter (`volatile uint32_t` increment).
- Lightweight timestamp (reading 32-bit counter or delta encoding).
- CRC-16 lookup table (256B ROM table, single loop).
- `_Generic` type dispatch (compile-time resolution, zero runtime overhead).

---

## 3. Storage Rules

| Rule | Description |
| :--- | :--- |
| Default RAM Ring | All events enter RAM Ring first; this is the only hot path storage. |
| Flash Disabled by Default | Flash writing must not be enabled by default. |
| Flash for Fault Capsule Only | **Never** write to Flash during normal operation. Commit to Flash only after a fault, called from user code in a non-ISR context via `axiom_capsule_commit()`. |
| Explicit Persistence | If general events need Flash persistence, implement via a separate backend template with rate-limiting and wear-leveling strategies, outside the hot path. |
| Power-loss Recovery Test | Any backend involving Flash/FRAM/NVRAM must provide power-loss recovery test cases. |
| Wear Strategy Documentation | Flash backends must document erase/write estimates, wear-leveling strategies, and life expectancy. |

---

## 4. Protocol Rules

| Rule | Description |
| :--- | :--- |
| Event Record is the Only Log Entity | No distinction between "text logs" and "binary logs" in firmware; only Event Records exist. |
| No Private Backend Protocols | All buffers received by backends must be unified Event Record frames. Backends handle transport adaptation (e.g., COBS, CAN-FD framing) but cannot change internal frame structure. |
| Text is Just Rendering | Text output is rendered by the host decoder using dictionary templates. |
| JSON is Just Export | JSON is converted from binary by host tools. |
| Binary is Storage/Transport Form | Binary frame is the only data form crossing the firmware-host boundary. |
| Versioned Payload Interpretation | Wire v2 normal arguments are packed according to the identity-matched dictionary; metadata suffixes remain tagged. The decoder retains structural decoding for historical wire v1 typed payloads. |
| Forward Compatibility | Minor version increments preserve the payload interpretation within the major version. Major version changes must synchronize decoder, golden frames, specs, and docs. |

---

## 5. Dropping Rules

| Rule | Description |
| :--- | :--- |
| DEBUG Dropping Allowed | DEBUG level events can be discarded when resources are tight. |
| Rate Limiting Allowed | Discard events when global or per-backend rate limits are exceeded. |
| Backend Busy Dropping | Discard when backend `ready()` is false or `write()` returns `-EAGAIN`. |
| **No Silent Dropping** | All drops must notify the backend via `on_drop` callback, and Core must increment the drop counter. |
| **Must Generate DROP_SUMMARY** | When recovering from a dropping state, Core must auto-generate an `AXIOM_LEVEL_WARN` `DROP_SUMMARY` event containing `lost_count`, `last_module_id`, and `last_event_id`. |
| Persistent Drop Stats | Drop counters must be saved with the capsule during Capsule commit for post-mortem analysis. |

---

## 6. Extensibility Rules

| Rule | Description |
| :--- | :--- |
| Zero-Intrusive Backend Extension | New backends only need to implement `axiom_backend_t` and call `axiom_backend_register()`. No modification to `core/` or `frontend/` files. |
| Extensible Payload Metadata | In wire v2, `0x0A` and `0x0B` identify location and metadata identity suffixes after dictionary-defined packed arguments. Any new suffix requires synchronized encoder, decoder, spec, golden, and documentation updates; unknown suffixes must be rejected safely. |
| Prunable Profiles | `DEV` / `FIELD` / `PROD` profiles controlled by macros. New profiles added in `frontend/` must not alter existing profile semantics. |
| Weak Port Symbols | All port functions provide `__attribute__((weak))` defaults. New ports (e.g., new MCU series) only override necessary functions without modifying library code. |
| Extensible Toolchain | Decoder uses pluggable dictionary loading (JSON/YAML/X-Macro). New export formats (e.g., CSV, PCAP) implemented via new render modules. |
| API Stability | `AX_*` macros and `axiom_backend_t` struct ABI are locked from v1.0. Internal `_axiom_*` functions may change in minor versions. |

---

## 7. User Friendly Rules

| Rule | Description |
| :--- | :--- |
| 5-Minute Start | New users can copy `axiomtrace.h` to their project, write 3 lines of code, and compile/log immediately. |
| Intuitive API Naming | Macro naming follows `AX_<LEVEL>(MOD, EVT, ...)` or `AX_LOG(msg)`. No obscure abbreviations. |
| Progressive Complexity | Layer 0 (Hardcoded ID) → Layer 1 (X-Macro) → Layer 2 (YAML + codegen). Users upgrade as needed without forced complexity. |
| Clear Error Messages | Compile-time errors must use `#error "AxiomTrace: xxx"` with clear English prompts. Runtime errors (e.g., ring full) exposed via `on_drop` callback; no silent failures. |
| Full Example Coverage | Every Frontend API, Backend Template, and Profile must have an independent, compilable example. |
| Synchronized Documentation | Any API change must update `../../spec/api_reference.md`. Any protocol change must update `../../spec/wire_format.md` and the decoder. |
| Automated Single-file Library | Provide `../../tool/scripts/amalgamate.py` to merge the library into `axiomtrace.h`. Amalgamated output must pass all host tests. |

### Documentation Governance

| Rule | Description |
| :--- | :--- |
| Single Owner per Topic | Each concept has one canonical document: API in `spec/api_reference.md`, wire data in `spec/wire_format.md`, event semantics in `spec/event_model.md`, toolchain/bundle behavior in `spec/toolchain_ecosystem_design.md`, and project process in `docs/project/*.md`. |
| No Ad-hoc Markdown | Do not add new `.md` files unless the topic cannot fit an existing canonical document and the new file is linked from README plus `docs/reference/DIR_STRUCTURE.md`. |
| README is an Index | README files explain positioning and link to canonical docs; they must not duplicate schemas, long CLI contracts, or implementation plans. |
| Bilingual Parity | Public docs that already have English and Chinese versions must be updated in both languages in the same change. |
| Tool Help for Details | CLI option details belong in tool help/docstrings first; Markdown should describe stable contracts and workflows, not copy every flag repeatedly. |
| Route Before Expansion | Large new toolchain or documentation work must first update `PLAN.md` / `ROUTE.md` so the scope is visible before implementation. |

---

## 8. Development Workflow (Mandatory)

Any new feature, bug fix, or protocol change must strictly follow this workflow:

```text
Issue Opened
    ↓
Design Note (State design decisions, impact, alternatives in the issue)
    ↓
Spec Update (Modify spec/*.md to explain new semantics)
    ↓
Golden Frame Update (Update golden/*.bin and expected.json if wire format changes)
    ↓
Implementation (Follow all RULES.md)
    ↓
Decoder Update (Update Python decoder if protocol, packed schema, or metadata suffixes change)
    ↓
Tests (C host unit tests + Python decoder regression tests)
    ↓
Benchmark (Confirm no hot path regression, record in benchmark report)
    ↓
Docs (Update README, api_reference.md, example comments)
    ↓
Changelog (Record in "Keep a Changelog" format)
    ↓
PR Review (At least 1 reviewer, check against RULES.md)
    ↓
Merge
```

**Skipping any step = Rejection of PR.**

---

## 9. Maintenance Workflow

### 9.1 Daily Maintenance

- **Iteration Start**: Check `ROUTE.md` goals, ensure no scope creep.
- **Each Commit**: Run host `ctest` plus `python -m pytest -q` locally before pushing.
- **Protocol Tweaks**: Run `python tool/golden/update_golden.py --check` first; commit regenerated golden frames only when the wire contract intentionally changes.
- **Weekly**: Check `tests/` coverage; add tests for any uncovered new code.

### 9.2 Release Workflow

1. Confirm all `ROUTE.md` tasks for the stage are complete.
2. Run host C matrix: Clang and GCC builds, then `ctest --test-dir <build-dir> --output-on-failure` for each.
3. Run Python/tooling matrix: `python -m pytest -q`, `python -m compileall -q tool/src tests`, and `PYTHONPATH=tool/src python -m axiomtrace_tools.cli validate --golden tool/golden`.
4. Run golden verification: `python tool/golden/update_golden.py --check`.
5. Run preset smoke builds for `custom`, `tiny`, `prod`, `field`, and `dev` with `AXIOM_BUILD_TESTS=OFF`.
6. Run integration: on Windows use Git Bash for `./tests/test_integration.sh`.
7. Run benchmark: `tests/host/test_benchmark` from a verified build directory and update the report if the baseline changes.
8. Check for no P0/P1 issues (see §10).
9. Update `../changelog/CHANGELOG.md`.
10. Update `PLAN.md` status.
11. Tag git: `git tag v0.x-stage`.
12. Run amalgamation script and verify the single-file library passes tests.
13. Publish Release Note (including binaries, single-file library, decoder, and docs).

### 9.3 Issue Grading and Response

| Grade | Definition | Response Time | Handling |
| :--- | :--- | :--- | :--- |
| **P0** | Data loss, silent drop, protocol incompatibility, build failure. | 24h | Fix immediately, block release. |
| **P1** | Performance regression > 20%, API/doc mismatch, backend leak. | 72h | Must fix, delay release. |
| **P2** | Doc error, example build failure, non-core defect. | 1 week | Schedule fix. |
| **P3** | Code style, optimization suggestion, enhancement request. | Variable | Decide after discussion. |

---

## 10. Release Threshold (v1.0-stable)

Official `v1.0` tag allowed **ONLY** when all are met:

- [ ] Stable API (`AX_*` macros locked).
- [ ] Stable wire format (header, packed-argument, and metadata-suffix contracts frozen).
- [ ] Stable event model (Event Record semantics unchanged).
- [ ] Stable backend contract (`axiom_backend_t` struct frozen).
- [ ] Stable capsule format (capsule layout frozen).
- [ ] Stable decoder (Python decoder can parse current packed frames, legacy typed frames, and capsules).
- [ ] Stable golden tests (all passing, no flakiness).
- [ ] Stable examples (all compile and run as expected).
- [ ] Stable benchmark report (hot path cycle count baseline locked).
- [ ] Stable docs (README, spec, api_reference complete, no TODOs).
- [ ] No P0/P1 issues.
- [ ] CI passing (host tests + decoder tests + format check).

---

## 11. Version Naming Rules

Version naming during development (avoid generic "v3"):

| Version | Meaning |
| :--- | :--- |
| v0.1-core | Core Plane provable. |
| v0.2-wire | Wire Format freezable. |
| v0.3-frontend | Frontend usable. |
| v0.4-backend | Backend pluggable. |
| v0.5-probe | Probe timing investigation mature. |
| v0.6-capsule | Fault Capsule mature. |
| v0.7-toolchain | Toolchain complete. |
| v0.8-portability | Cross-platform validation complete. |
| v0.9-rc | Release Candidate. |
| v1.0-stable | Stable Release. |

---

## 12. Architectural Compliance Checklist

Authors must self-assess this checklist before submitting new code:

- [ ] All features fit into one of: `LOG / EVT / PROBE / FAULT / BACKEND / TOOL`.
- [ ] No stray modules (new .c/.h must belong to one of the five planes).
- [ ] No dual protocols (no private backend protocols).
- [ ] No RTOS/Linux implementation branches (port layer isolation, core has no OS dependency).
- [ ] Hot path has no malloc, no printf, no Flash erase, no blocking.
- [ ] New backend did not modify core code.
- [ ] New packed layouts or metadata suffixes synchronized with decoder and spec.
- [ ] New APIs synchronized with docs, examples, and tests.
- [ ] No silent dropping (drop counter + `DROP_SUMMARY` present).
- [ ] Examples compile independently (no reliance on undocumented internal symbols).

**Incomplete Checklist = PR Rejection.**
