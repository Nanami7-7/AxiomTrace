> [English](fault_capsule.md) | [简体中文](fault_capsule_zh.md)

# AxiomTrace Fault Capsule Specification

> 版本：v1.0  |  状态：**冻结中（v0.6-capsule 定稿）**  |  对应代码：`baremetal/backend/axiom_capsule_flash.c`, `baremetal/core/axiom_event.c`

---

## 1. Concept

A **Fault Capsule** is a frozen snapshot of system state and recent events captured when a critical fault (`AX_FAULT`) is logged. It enables post-mortem analysis without external debugger attachment.

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
   - Core invokes `axiom_port_fault_hook()`.
   - Normal Ring is **frozen** (new normal events are dropped or redirected).
   - The last `N` pre-fault events are copied from Normal Ring tail to Capsule Ring.
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
│   - Each record is a complete supported frame (current v1.1) │
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

## 5. Configuration

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
#define AXIOM_CAPSULE_RING_SIZE (2048u)
#endif
```

- `AXIOM_CAPSULE_ENABLED == 0`: `AX_FAULT` behaves exactly like `AX_ERROR`. No capsule logic is compiled.
- `AXIOM_CAPSULE_PRE_EVENTS`: Number of events to retain before fault.
- `AXIOM_CAPSULE_POST_EVENTS`: Number of events to retain after fault.

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

---

## 7. Constraints

- Flash erase/write **never** occurs in ISR or hot path.
- `axiom_capsule_commit()` must be called from main loop or fault handler tail, with interrupts managed by the port layer.
- Capsule data format is self-describing and versioned; decoder can parse capsules without external schema.
- Bundle-backed semantic reports require a metadata identity event within the captured Event Records. The capsule `firmware_hash` remains a firmware diagnostic field and is not the bundle `metadata_id`.
- If Flash commit fails (e.g., Flash controller busy), the capsule remains in RAM (if retention RAM is available) and `commit()` returns `false`.
