> [English](fault_capsule.md) | [简体中文](fault_capsule_zh.md)

# AxiomTrace Fault Capsule 规范

> 版本：v1.0  |  状态：**已实现**  |  当前代码：`baremetal/core/axiom_capsule.c`, `baremetal/core/axiom_event.c`, `tool/src/axiomtrace_tools/capsule.py`

---

## 1. 概念

**Fault Capsule (故障舱)** 是用于事后分析的持久化快照格式。v1.0 中，`AX_FAULT` 会发出 fault-level Event Record、调用 `axiom_port_fault_hook()`、冻结保留的故障前窗口、捕获故障后窗口，并允许用户代码将 capsule v1 镜像流式提交到配置的 Flash 区域。

**核心规则**：在系统正常运行时，Flash 写入量为 **零**。只有在触发故障后，在非 ISR 上下文调用 `axiom_capsule_commit()` 时才会触碰 Flash。

---

## 2. 双区设计 (Dual-Zone Design)

日志系统维护两个逻辑区域：

| 区域          | 用途                                 | 策略                |
|---------------|--------------------------------------|---------------------|
| Core Ring | 常规事件分发 | 默认 DROP；可选记录感知 OVERWRITE |
| Capsule 帧环 | 保留完整历史帧 | 故障前按整帧淘汰；故障后只追加 |

### 2.1 生命周期

1. **正常运行**：事件流经 Core Ring；一个记录感知的 capsule 字节环保留最新完整故障前帧，同时满足数量和字节限制。
2. **故障触发**：调用 `AX_FAULT()`。
   - Core 调用 `axiom_port_fault_hook()` 并发出 fault Event Record。
   - capsule observer 冻结自身保留的 pre-window；独立的 Core Ring 继续按自身策略工作。
   - fault 帧作为 post-window 的第一条记录追加。
3. **故障后捕获**：最多 `M` 条 fault/故障后事件只追加写入，不淘汰已冻结的 pre-window。
4. **冻结完成**：当帧环满或达到故障后窗口 `M` 时，系统进入 **Capsule Frozen (故障舱冻结)** 状态。
5. **提交 (Commit)**：用户代码（或 Port 层）调用 `axiom_capsule_commit()`：
   - 擦除 Flash 故障舱扇区（非 ISR 环境）。
   - 写入故障舱报头（寄存器快照、复位原因、固件哈希、丢弃统计等）。
   - 分块写入 snapshot 与帧环内容。
   - 写入故障舱 CRC 校验值。
6. **重启转储**：复位后，用户代码调用 `axiom_capsule_present()` / `axiom_capsule_read()` 来获取故障舱内容进行分析。

---

## 3. 故障舱内容 (Capsule Content)

提交后的故障舱结构如下：

```
┌─────────────────────────────────────────────────────────────┐
│ Capsule Header (故障舱报头)                                 │
│   - magic (4B): "AXCP"                                      │
│   - version (1B)                                            │
│   - capsule_len (2B LE)                                     │
│   - reset_reason (1B)                                       │
│   - fault_type (1B)                                         │
│   - firmware_hash (4B)                                      │
│   - drop_count (4B LE)                                      │
│   - pre_window_count (1B)                                   │
│   - post_window_count (1B)                                  │
│   - 寄存器快照 (变长, 参见 §4)                              │
├─────────────────────────────────────────────────────────────┤
│ 事件记录 (从 Capsule Ring 拷贝)                             │
│   - 每个记录都是一个完整的受支持帧（当前为 v2.0）            │
├─────────────────────────────────────────────────────────────┤
│ 故障舱 CRC-32 (4B LE)                                       │
│   - 覆盖报头 + 所有事件记录                                 │
└─────────────────────────────────────────────────────────────┘
```

---

## 4. 寄存器快照 (Register Snapshot)

寄存器快照格式取决于硬件架构，由 `snapshot_id` 字节标识：

### 4.1 Cortex-M 快照 (`snapshot_id = 0x01`)

| 字段 | 大小 | 描述 |
|-------|------|-------------|
| `pc`  | 4B   | 故障时的程序计数器 |
| `lr`  | 4B   | 链接寄存器 |
| `sp`  | 4B   | 堆栈指针 |
| `xpsr`| 4B   | 程序状态寄存器 |
| `r0-r3`| 每个 4B | 参数寄存器 |
| `r12` | 4B   | 过程内调用临时寄存器 |

### 4.2 RISC-V 快照 (`snapshot_id = 0x02`)

| 字段 | 大小 | 描述 |
|-------|------|-------------|
| `mepc`| 4B   | 机器异常 PC |
| `mcause`| 4B | 机器原因 |
| `mtval`| 4B  | 机器陷阱值 |
| `ra`  | 4B   | 返回地址 |
| `sp`  | 4B   | 堆栈指针 |
| `a0-a7`| 每个 4B | 参数寄存器 |

**注意**：Port 层提供 `axiom_port_fault_snapshot(uint8_t *buf, uint8_t max_len)` 函数来填充寄存器快照。

---

## 5. 目标配置 (Target Configuration)

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

- 这些宏属于当前 core `axiom_config.h`。
- `AXIOM_CAPSULE_ENABLED == 0`：`AX_FAULT` 保持为 fault-level Event Record，不编译 capsule 后端逻辑。
- `AXIOM_CAPSULE_PRE_EVENTS`：故障前保留的事件数量。
- `AXIOM_CAPSULE_POST_EVENTS`：故障后保留的事件数量。
- `AXIOM_CAPSULE_RING_SIZE`：捕获 Event Record 流使用的最大字节数。
- `AXIOM_CAPSULE_FLASH_BASE` / `AXIOM_CAPSULE_FLASH_SIZE`：commit/read/clear 使用的 Port 侧 Flash 区域。

---

## 6. API

```c
/* 由用户代码在故障处理后、重启前调用 */
bool axiom_capsule_commit(void);

/* 在重启后调用，检查是否存在故障舱 */
bool axiom_capsule_present(void);

/* 将故障舱数据读入用户缓冲区。返回读取的字节数。 */
uint32_t axiom_capsule_read(uint8_t *out, uint32_t max_len);

/* 从 Flash 中清除故障舱（擦除扇区） */
void axiom_capsule_clear(void);
```

`axiom_capsule_read()` 直接在调用者缓冲区合成既有 capsule v1 镜像，不保留第二份完整 RAM image。`axiom_capsule_present()` 使用固定 64 字节 scratch 流式校验 Flash。两条 Flash 路径都校验 magic、编码长度与 CRC-32；提交失败时，保留的帧环仍可读取。

---

## 7. 约束

- Flash 擦除/写入 **绝不会** 在 ISR 或热路径中发生。
- `axiom_capsule_commit()` 必须从主循环或故障处理程序尾部调用，且中断由 Port 层管理。
- 故障舱 framing 与寄存器快照带版本控制。对于 wire v2 事件语义，decoder 需要 identity 匹配的 metadata bundle；未提供时，普通事件 payload 保持 raw 输出。
- 使用 bundle 生成语义报告时，捕获的事件记录中必须包含元数据身份事件。故障舱中的 `firmware_hash` 仍是固件诊断字段，不等同于 bundle 的 `metadata_id`。
- 如果 Flash commit 失败（例如 Flash 控制器忙、短写或写入中断），capsule 会保留在 RAM 中，并且 `commit()` 返回 `false`。
