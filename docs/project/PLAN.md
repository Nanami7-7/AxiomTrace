> [English](PLAN.md) | [简体中文](PLAN_zh.md)

# AxiomTrace v1.0 Target Plan

> Version: v1.0 | Status: **Building** | Update Date: 2026-04-29

---

## 1. v1.0 Positioning

**AxiomTrace v1.0** = MCU Bare-metal Observability Microkernel

Components: Event Core + Log Facade + Realtime Probe + Fault Capsule + Backend Contract + Toolchain

**What it is NOT**:
- Not a printf replacement.
- Not an RTOS logging library.
- Not a Linux logging library.
- Not a full filesystem.
- Not a chip HAL encapsulation library.

**Final Definition**: AxiomTrace v1.0 is not about being feature-rich, but about proving that this bare-metal observability microkernel is: Developer-readable, Production-low-disturbance, Fault-traceable, Protocol-freezable, and Toolchain-trusted.

---

## 2. v1.0 Final Architecture

```text
AxiomTrace/
  Semantic Plane   // Event Semantics: level / module / event / payload / schema
  Frontend Plane   // User Entry: AX_LOG / AX_EVT / AX_PROBE / AX_FAULT / AX_KV
  Core Plane       // Core: ring / encode / filter / drop / timestamp / crc
  Backend Plane    // Backend: memory / RTT / USB / UART / SWO / CAN / flash capsule
  Tool Plane       // Tools: decoder / viewer / codegen / golden / benchmark
```

### Core Principle

> **Event Record is the entity of the log. Text / JSON / Binary are just rendering or transmission forms.**

---

## 3. Road to v1.0

### Phase 0: Freeze Target Specifications

**Goal**: Define exactly what v1.0 looks like.

**Deliverables**:
- `PLAN.md`
- `ROUTE.md`
- `RULES.md`
- `../../spec/event_model.md`
- `../../spec/wire_format.md`
- `../../spec/backend_contract.md`
- `../../spec/fault_capsule.md`
- `../../spec/api_reference.md`

**Acceptance**:
- All features fit into `LOG / EVT / PROBE / FAULT / BACKEND / TOOL`.
- No stray modules.
- No dual protocols.
- No RTOS/Linux implementation branches.

---

### Phase 1: Core Provable

**Goal**: Prove the minimal kernel is correct.

**Deliverables**:
- RAM Ring (Lock-free, IRQ-safe, Single-producer Single-consumer).
- Event Record (Fixed 8B header + 1B payload_len + payload + 2B crc16).
- Encoder (`_Generic` type-safe dispatch + wire v2 packed value writing).
- CRC-16/CCITT-FALSE (256B ROM lookup table).
- Compressed Relative Timestamp (delta encoding).
- Memory Backend (Directly writing to RAM Ring region).
- Host Unit Test (ring, encode, crc, event assembly).

**Rules**:
- No malloc.
- No printf.
- No blocking.
- O(1) write.
- ISR-writable.

---

### Phase 2: Wire Format Freezable

**Goal**: Ensure the protocol can be maintained long-term.

**Deliverables**:
- Binary frame definition frozen.
- payload_len (1B).
- event_id (2B LE).
- module_id (1B).
- seq (2B LE).
- timestamp (4B LE, compressed delta).
- crc16 (2B LE).
- Golden frames (*.bin).
- expected.json (Decoder expected output).
- Initial Python decoder version.

**Acceptance**:
- `encoder -> frame -> decoder -> expected.json` match exactly.
- Invalid frames must be explicitly rejected (return `FRAME_INVALID`).
- Every protocol change must update golden frames and run regression tests.

---

### Phase 3: Frontend Usable

**Goal**: Make it truly easy for users to start.

**Deliverables**:
- `AX_LOG(msg)` — Developer-readable, pruned to empty by Profile in production.
- `AX_EVT(level, mod, evt, args...)` — Production structured events.
- `AX_PROBE(tag, value)` — Timing probe, high frequency, low disturbance.
- `AX_FAULT(mod, evt, args...)` — Fault tracing, triggers Capsule.
- `AX_KV(level, mod, evt, "k", v, ...)` — Lightweight Key-Value.
- `DEV / FIELD / PROD` Profile compile-time pruning.

**Boundaries**:
- `AX_LOG`: Developer-readable, expands to empty under `PROD` profile.
- `AX_EVT`: Production structured events, retained in all profiles.
- `AX_PROBE`: Timing probe, prunable in `FIELD/PROD`.
- `AX_FAULT`: Fault tracing, retained in all profiles.

**Prohibitions**:
- `AX_LOG / AX_EVT / AX_PROBE / AX_FAULT` defining different protocols.
- All Frontend APIs eventually call the same `axiom_write()`.

---

### Phase 4: Backend Pluggable

**Goal**: Compatible with all peripherals without being tied to a specific chip.

**Deliverables**:
- Memory Backend.
- UART Backend Template (COBS + 0x00 delimiter).
- USB CDC Backend Template (bulk IN endpoint).
- RTT Backend Template (SEGGER RTT up-channel).
- SWO/ITM Backend Template (32-bit stimulus word stream).
- CAN-FD Backend Template (frame splitting and ID mapping).
- Flash Capsule Backend (commits on fault, no writes during normal operation).

**Backend Contract**:

```c
typedef struct {
    const char *name;
    uint32_t caps;

    int  (*write)(const uint8_t *buf, uint16_t len, void *ctx);
    int  (*ready)(void *ctx);
    int  (*flush)(void *ctx);
    int  (*panic_write)(const uint8_t *buf, uint16_t len, void *ctx);

    void (*on_drop)(uint32_t lost, void *ctx);
    void *ctx;
} axiom_backend_t;
```

**Rules**:
- User is responsible for peripheral drivers (filling `write/ready` function pointers).
- AxiomTrace is responsible for buffering, rate limiting, degradation, drop statistics, and protocol consistency.
- Adding a new backend is zero-intrusive to the core (no modification to files in `core/` or `frontend/`).

---

### Phase 5: Fault Capsule Mature

**Goal**: Form the core differentiation.

**Deliverables**:
- Pre-window freeze (retain last N events before fault).
- Post-window capture (capture M events after fault).
- Reset reason logging.
- Fault type classification.
- PC / LR / SP / xPSR register snapshot.
- Firmware hash (injected at compile-time).
- Capsule CRC (integrity check).
- Flash commit (non-ISR call).
- Reboot dump support.

**Rules**:
- No Flash writes by default during normal operation.
- Commit capsule only after a fault is triggered.
- Flash erase/write does not enter the log hot path.
- Capsule framing is stable and decodable; wire v2 event semantics use the identity-matched bundle.

---

### Phase 6: Toolchain Complete

**Goal**: Binary logs must be readable, testable, and exportable.

**Deliverables**:
- Decoder (binary → structured object).
- Text Render (structured object → human-readable text, dictionary template filling).
- JSON Export (structured object → JSON file).
- Standard Metadata Bundle (`manifest.json`, `dictionary.json`, `source_map.json`, `build_info.json`, optional ELF/map artifacts).
- Bundle Generator (`axiom-bundle generate`) and CMake helper (`axiomtrace_add_bundle(...)`).
- Firmware identity matching for bundle-store based decode.
- Capsule Report (capsule binary → fault analysis report).
- Dictionary Validator (verify firmware payload matches dictionary template types).
- Golden Frame Updater (encoder generates standard frames for regression testing).
- Benchmark Tool (hot path cycle count measurement and reporting).
- Documentation governance: one canonical doc per topic, no ad-hoc Markdown.

**Acceptance**:
- binary → text (via decoder + render).
- binary → json (via decoder + json export).
- trace + bundle → semantic decode with source location when enabled.
- trace + bundle-store → exact bundle selected by firmware identity.
- capsule → report (via capsule decoder).
- golden → regression test (auto-run in CI).

---

### Phase 7: v1.0 Release Candidate

**Goal**: No new features, only bug fixing.

**Must Complete**:
- API review (naming, parameters, macro behavior consistency).
- Spec review (all `../../spec/*.md` consistent with code).
- Backend review (all templates compile).
- Capsule review (freeze/commit/dump full path test).
- Benchmark report (hot path cycle counts locked).
- Fuzz malformed frame (libFuzzer or custom fuzz target).
- Fault injection (simulate HardFault, verify capsule capture).
- Power-loss simulation (verify Flash capsule power-loss recovery).
- Docs complete (README as index, canonical specs, tool help, examples comments; no duplicate standalone Markdown).
- Examples complete (all compile and run).

---

## 4. v1.0 Release Threshold

Official v1.0 will only be released when **ALL** are met:

- [ ] Stable API (`AX_*` macros locked).
- [ ] Stable wire format (header, packed arguments, and metadata suffix definitions frozen).
- [ ] Stable event model (Event Record semantics unchanged).
- [ ] Stable backend contract (`axiom_backend_t` struct frozen).
- [ ] Stable capsule format (capsule layout frozen).
- [ ] Stable decoder (can parse current packed frames, legacy typed frames, and capsules).
- [ ] Stable golden tests (all passing).
- [ ] Stable examples (all compile and run).
- [ ] Stable benchmark report (hot path cycle count baseline locked).
- [ ] Stable docs (README, spec, api_reference complete).
- [ ] No P0/P1 issues.

---

## 5. Design Philosophy Summary

| Dimension | Choice | Reason |
| :--- | :--- | :--- |
| Log Entity | Event Record | Unified on firmware side, multi-form rendering on host side. |
| Default Storage | RAM Ring | Low latency in hot path, readable after fault without power loss. |
| Flash Usage | Fault Capsule Only | Avoid Flash wear in production and complex power-loss recovery. |
| API Design | Structured Macros + Profile Pruning | Easy to use during development (AX_LOG), zero overhead in production (PROD pruning). |
| Type Safety | C11 `_Generic` | Compile-time checks, zero runtime overhead. |
| Extensibility | Backend Contract + Weak Symbol Port | No library modification for new backends or platforms. |
| User Friendly | Single-file Library + Zero-config Start + Progressive Complexity | 5 minutes to start, upgrade toolchain as needed. |
| Trusted | Golden test + decoder regression + benchmark | Verifiable changes, traceable protocol. |
