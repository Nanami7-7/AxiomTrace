> [English](fault_capsule.md) | [简体中文](fault_capsule_zh.md)

# AxiomTrace Fault Capsule Specification

> Version: v1.0 candidate  |  Status: **Implemented in portable RAM/Flash backend**  |  Current code: `baremetal/core/axiom_capsule.c`, `baremetal/core/axiom_event.c`, `tool/src/axiomtrace_tools/capsule.py`

---

## 1. Concept

A **Fault Capsule** is the persisted snapshot format for post-mortem analysis. In v1.0, `AX_FAULT` emits a normal fault-level Event Record, calls `axiom_port_fault_hook()`, freezes the RAM pre-window, captures the post-window, and allows user code to commit the assembled capsule image to the configured Flash region.

**Core rule**: Normal operation writes **zero** bytes to Flash. Flash is only touched after a fault trigger, during the non-ISR `axiom_capsule_commit()` call.

---

## 2. Dual-Zone Design

The logging system maintains two logical zones:

| Zone         | Purpose                              | Policy              |
|--------------|--------------------------------------|---------------------|
| Normal Ring  | Regular event logging                | Overwrite or Drop   |
| Capsule Ring | Reserved for fault capture           | Freeze on fault     |

### 2.1 Lifecycle

1. **Normal operation**: Events flow into the Normal Ring. The Capsule Ring remains empty.
2. **Fault trigger**: `AX_FAULT()` is called.
   - Core invokes `axiom_port_fault_hook()` and emits the fault Event Record.
   - The capsule observer freezes the normal pre-window and captures the pre/post windows described here.
   - Normal Ring is **frozen** (new normal events are dropped or redirected).
   - The last `N` pre-fault Event Records are copied from the RAM pre-window to the Capsule stream.
3. **Post-fault capture**: Up to `M` post-fault events are written into Capsule Ring.
4. **Freeze complete**: When Capsule Ring is full or post-window `M` is reached, the system enters **capsule frozen** state.
5. **Commit**: User code (or port layer) calls `axiom_capsule_commit()`:
   - Erase Flash capsule sector(s) (non-ISR)
   - Write capsule header (register snapshot, reset reason, firmware hash, drop stats)
   - Write Capsule Ring contents
   - Write capsule CRC
6. **Reboot dump**: After reset, user code calls `axiom_capsule_present()` / `axiom_capsule_read()` to retrieve the capsule for analysis.

---

## 3. Capsule Content

The committed capsule consists of:

```
┌─────────────────────────────────────────────────────────────┐
│ Capsule Header                                               │
│   - magic (4B): "AXCP"                                      │
│   - version (1B)                                            │
│   - capsule_len (2B LE)                                     │
│   - reset_reason (1B)                                       │
│   - fault_type (1B)                                         │
│   - firmware_hash (4B)                                      │
│   - drop_count (4B LE)                                      │
│   - pre_window_count (1B)                                   │
│   - post_window_count (1B)                                  │
│   - register snapshot (variable, see §4)                    │
├─────────────────────────────────────────────────────────────┤
│ Event Records (copied from Capsule Ring)                     │
│   - Each record is a complete supported frame (current v2.0) │
├─────────────────────────────────────────────────────────────┤
│ Capsule CRC-32 (4B LE)                                       │
│   - Covers header + all event records                        │
└─────────────────────────────────────────────────────────────┘
```

---

## 4. Register Snapshot

The register snapshot format is architecture-dependent, identified by a `snapshot_id` byte:

### 4.1 Cortex-M Snapshot (`snapshot_id = 0x01`)

| Field | Size | Description |
|-------|------|-------------|
| `pc`  | 4B   | Program Counter at fault |
| `lr`  | 4B   | Link Register |
| `sp`  | 4B   | Stack Pointer |
| `xpsr`| 4B   | Program Status Register |
| `r0-r3`| 4B each | Argument registers |
| `r12` | 4B   | Intra-procedure call scratch |

### 4.2 RISC-V Snapshot (`snapshot_id = 0x02`)

| Field | Size | Description |
|-------|------|-------------|
| `mepc`| 4B   | Machine Exception PC |
| `mcause`| 4B | Machine Cause |
| `mtval`| 4B  | Machine Trap Value |
| `ra`  | 4B   | Return Address |
| `sp`  | 4B   | Stack Pointer |
| `a0-a7`| 4B each | Argument registers |

**Note**: The port layer provides `axiom_port_fault_snapshot(uint8_t *buf, uint8_t max_len)` to fill the register snapshot.

---

## 5. Target Configuration

```c
#ifndef AXIOM_CAPSULE_ENABLED
#define AXIOM_CAPSULE_ENABLED 1
#endif

#ifndef AXIOM_CAPSULE_PRE_EVENTS
#define AXIOM_CAPSULE_PRE_EVENTS 32u
#endif

#ifndef AXIOM_CAPSULE_POST_EVENTS
#define AXIOM_CAPSULE_POST_EVENTS 16u
#endif

#ifndef AXIOM_CAPSULE_RING_SIZE
#define AXIOM_CAPSULE_RING_SIZE 4096u
#endif

#ifndef AXIOM_CAPSULE_MAX_SNAPSHOT_LEN
#define AXIOM_CAPSULE_MAX_SNAPSHOT_LEN 64u
#endif

#ifndef AXIOM_CAPSULE_FLASH_BASE
#define AXIOM_CAPSULE_FLASH_BASE 0u
#endif

#ifndef AXIOM_CAPSULE_FLASH_SIZE
#define AXIOM_CAPSULE_FLASH_SIZE 8192u
#endif

#ifndef AXIOM_CAPSULE_FIRMWARE_HASH
#define AXIOM_CAPSULE_FIRMWARE_HASH 0u
#endif

#ifndef AXIOM_CAPSULE_SNAPSHOT_ID
#define AXIOM_CAPSULE_SNAPSHOT_ID 0u
#endif
```

- These macros are part of the current core `axiom_config.h`.
- `AXIOM_CAPSULE_ENABLED == 0`: `AX_FAULT` remains a fault-level Event Record and no capsule backend logic is compiled.
- `AXIOM_CAPSULE_PRE_EVENTS`: Number of events to retain before fault.
- `AXIOM_CAPSULE_POST_EVENTS`: Number of events to retain after fault.
- `AXIOM_CAPSULE_RING_SIZE`: Maximum bytes used for captured Event Records.
- `AXIOM_CAPSULE_FLASH_BASE` / `AXIOM_CAPSULE_FLASH_SIZE`: Port-specific Flash region used by commit/read/clear.

---

## 6. API

```c
/* Called by user code after fault handling, before reboot */
bool axiom_capsule_commit(void);

/* Called after reboot to check if a capsule exists */
bool axiom_capsule_present(void);

/* Read capsule data into user buffer. Returns bytes read. */
uint32_t axiom_capsule_read(uint8_t *out, uint32_t max_len);

/* Clear capsule from Flash (erase sector) */
void axiom_capsule_clear(void);
```

`axiom_capsule_present()` and `axiom_capsule_read()` validate the capsule magic, encoded length, and CRC-32. If Flash commit fails, the RAM capsule remains readable.

---

## 7. Constraints

- Flash erase/write **never** occurs in ISR or hot path.
- `axiom_capsule_commit()` must be called from main loop or fault handler tail, with interrupts managed by the port layer.
- Capsule framing and register snapshots are versioned. For wire v2 event semantics, the decoder requires the identity-matched metadata bundle; without it, normal event payload values remain raw.
- Bundle-backed semantic reports require a metadata identity event within the captured Event Records. The capsule `firmware_hash` remains a firmware diagnostic field and is not the bundle `metadata_id`.
- If Flash commit fails (e.g., Flash controller busy, short write, or interrupted write), the capsule remains in RAM and `commit()` returns `false`.
