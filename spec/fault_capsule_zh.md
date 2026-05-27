> [English](fault_capsule.md) | [简体中文](fault_capsule_zh.md)

# AxiomTrace Fault Capsule 规范

> 版本：v1.0  |  状态：**冻结中（v0.6-capsule 定稿）**  |  对应代码：`baremetal/backend/axiom_capsule_flash.c`, `baremetal/core/axiom_event.c`

---

## 1. 概念

**Fault Capsule (故障舱)** 是当系统记录关键故障 (`AX_FAULT`) 时，对系统状态和近期事件进行冻结的快照。它使得开发者能够在不连接外部调试器的情况下进行事后分析 (Post-mortem analysis)。

**核心规则**：在系统正常运行时，Flash 写入量为 **零**。只有在触发故障后，在非 ISR 上下文调用 `axiom_capsule_commit()` 时才会触碰 Flash。

---

## 2. 双区设计 (Dual-Zone Design)

日志系统维护两个逻辑区域：

| 区域          | 用途                                 | 策略                |
|---------------|--------------------------------------|---------------------|
| Normal Ring   | 常规事件日志                         | 覆盖或丢弃          |
| Capsule Ring  | 预留用于故障捕获                     | 故障时冻结          |

### 2.1 生命周期

1. **正常运行**：事件流向 Normal Ring。Capsule Ring 保持为空。
2. **故障触发**：调用 `AX_FAULT()`。
   - 内核调用 `axiom_port_fault_hook()`。
   - Normal Ring 被 **冻结**（新的常规事件将被丢弃或重定向）。
   - 最近的 `N` 个故障前事件从 Normal Ring 尾部拷贝到 Capsule Ring。
3. **故障后捕获**：最多 `M` 个故障后事件被写入 Capsule Ring。
4. **冻结完成**：当 Capsule Ring 满或达到故障后窗口 `M` 时，系统进入 **Capsule Frozen (故障舱冻结)** 状态。
5. **提交 (Commit)**：用户代码（或 Port 层）调用 `axiom_capsule_commit()`：
   - 擦除 Flash 故障舱扇区（非 ISR 环境）。
   - 写入故障舱报头（寄存器快照、复位原因、固件哈希、丢弃统计等）。
   - 写入 Capsule Ring 的内容。
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

## 5. 配置 (Configuration)

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

- `AXIOM_CAPSULE_ENABLED == 0`：`AX_FAULT` 的行为与 `AX_ERROR` 完全一致，不编译任何故障舱逻辑。
- `AXIOM_CAPSULE_PRE_EVENTS`：故障前保留的事件数量。
- `AXIOM_CAPSULE_POST_EVENTS`：故障后保留的事件数量。

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

---

## 7. 约束

- Flash 擦除/写入 **绝不会** 在 ISR 或热路径中发生。
- `axiom_capsule_commit()` 必须从主循环或故障处理程序尾部调用，且中断由 Port 层管理。
- 故障舱 framing 与寄存器快照带版本控制。对于 wire v2 事件语义，decoder 需要 identity 匹配的 metadata bundle；未提供时，普通事件 payload 保持 raw 输出。
- 使用 bundle 生成语义报告时，捕获的事件记录中必须包含元数据身份事件。故障舱中的 `firmware_hash` 仍是固件诊断字段，不等同于 bundle 的 `metadata_id`。
- 如果 Flash 提交失败（例如 Flash 控制器忙），故障舱将保留在 RAM 中（如果保留 RAM 可用），且 `commit()` 返回 `false`。
