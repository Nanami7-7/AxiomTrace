> [English](event_model.md) | [简体中文](event_model_zh.md)

# AxiomTrace 事件模型规范

> 版本：v2.0  |  状态：**已冻结**  |  对应代码：`baremetal/core/axiom_event.h`

---

## 1. 概述

AxiomTrace 事件是 **结构化的二进制记录**。每个事件由固定大小的报头和紧凑 payload 组成。Wire `v2.0` 将普通参数按 dictionary 定义的顺序编码为 packed 值；固件热路径不存储格式字符串或人类可读文本。主机 decoder 仍可读取历史 `v1.x` 的 typed payload。

**核心原则**：事件记录 (Event Record) 是唯一的日志实体。文本 (Text) / JSON / 二进制 (Binary) 仅仅是渲染或传输形态。

---

## 2. 事件报头 (Event Header)

报头严格限制为 **8 字节**（在有线传输中为紧凑排列、小端序）：

| 偏移 | 字段       | 类型     | 位数 | 描述                                     |
|------|------------|----------|------|------------------------------------------|
| 0    | `sync`     | uint8_t  | 8    | 同步字节 `0xA5`（帧同步提示）            |
| 1    | `version`  | uint8_t  | 8    | 有线格式版本 (`major << 4 | minor`)       |
| 2    | `level`    | uint8_t  | 4    | 日志级别（参见 §3），位于低 4 位         |
| 2    | `reserved` | uint8_t  | 4    | 保留位，必须为零                         |
| 3    | `module_id`| uint8_t  | 8    | 模块标识符（编译期分配）                 |
| 4    | `event_id` | uint16_t | 16   | 模块内事件标识符（小端序）               |
| 6    | `seq`      | uint16_t | 16   | 全局序列号（小端序，自然溢出回绕）       |

**编译期大小断言**（在 `axiom_event.h` 中强制执行）：
```c
_Static_assert(sizeof(axiom_event_header_t) == 8, "header size");
_Static_assert(_Alignof(axiom_event_header_t) == 1, "header align");
```

---

## 3. 日志级别 (Log Levels)

| 值   | 名称      | 语义说明                                | Profile 行为              |
|------|-----------|-----------------------------------------|---------------------------|
| 0    | `DEBUG`   | 详细的诊断信息                          | 可丢弃，支持限速          |
| 1    | `INFO`    | 常规运行事件                            | 默认输出级别              |
| 2    | `WARN`    | 异常但可恢复的情况                      | 始终保留                  |
| 3    | `ERROR`   | 可能会影响运行的故障                    | 始终保留                  |
| 4    | `FAULT`   | 关键故障；触发 Fault Capsule 冻结       | 始终保留；调用 `axiom_port_fault_hook()` |
| 5-15 | 保留      | 预留未来使用                            | —                         |

**级别过滤**：一个 32 位的掩码控制哪些级别的事件可以被发出。在内存压力或限速情况下，`DEBUG` 级别可能会被丢弃。

---

## 4. 有效载荷模型 (Payload Model)

报头之后紧跟 **有效载荷长度** (`1 字节`，最大 255)，然后是 **有效载荷数据**。

在 wire `v2.0` 中，普通参数按 dictionary 顺序紧凑排列：

| Dictionary 类型 | 值编码 |
|-----------------|--------|
| `bool`, `u8`, `i8` | 1 字节 |
| `u16`, `i16` | 2 字节，小端序 |
| `u32`, `i32`, `f32`, `timestamp` | 4 字节，小端序 |
| `bytes` | 1 字节长度加原始字节 |

主机必须使用匹配 bundle 的 dictionary 才能定位并解释普通参数。以下 metadata suffix 仍带有显式标签：

| 标签 | 标签后的编码 | 用途 |
|------|--------------|------|
| `0x0A` | `mode` 加固定宽度源码定位字段 | 可选调用点定位 |
| `0x0B` | `metadata_id:8` | 选择匹配的 metadata bundle |

`定位元数据` 不改变固定报头，只在启用时附加到 packed payload：

- `FILE_ID`（`mode = 2`）：`[0x0A][mode:u8][file_id:u16][line:u16]`，总计 6 字节。
- `HASH`（`mode = 1`）：`[0x0A][mode:u8][file_hash:u16][line:u16][func_hash:u16]`，总计 8 字节。
- `NONE`：不发出定位字段。

`元数据身份` 以 `[0x0B][metadata_id:8]` 形式发在系统保留事件 `(module_id = 0, event_id = 2)` 中。主机在语义解码前使用它选择并校验匹配的 metadata bundle。

系统保留事件具有固定协议 schema，不依赖应用 dictionary：

| 系统事件 | Payload 规则 |
|----------|--------------|
| `(0, 0)` `AX_PROBE` | Typed fields `[u16 tag][tag_hash:u16][value tag][value]`，因为每个探针调用点的值类型可能不同 |
| `(0, 1)` `DROP_SUMMARY` | 固定 packed 字段 `[lost_count:u32][last_module_id:u8][last_event_id:u16]` |
| `(0, 2)` metadata identity | 必须恰为 `[0x0B][metadata_id:8]`；尾随字节视为损坏 |

**兼容性原则**：
1. Wire `v2.0` 通过将匹配 dictionary/bundle 纳入语义解码契约，减少普通参数的逐字段开销。
2. 未提供 dictionary 时，v2 decoder 仍校验 frame/CRC、输出普通 payload 原始字节，并可解析上述固定系统事件。
3. 历史 `v1.x` 帧仍按逐字段 type tag 进行结构解码。

---

## 5. 完整帧布局与编码

```
┌────────┬───────────┬─────────┬────────┬──────────┬──────┬────────┬─────────┬────────────┐
│ 报头   │ 时间戳    │ 有效载荷│ 有效载荷│ CRC-16 │
│ 8B     │ 1..5B     │ 长度 1B │ N B    │ 2B LE  │
└────────┴───────────┴─────────┴────────┘──────────┘
```

- **报头**：参见 §2
- **时间戳**：变长差分压缩时间戳。
- **有效载荷长度**：1 字节，取值范围 `0..255`
- **有效载荷**：`payload_len` 字节，包含 v2 packed 值及可选 tagged metadata suffix
- **CRC-16**：CCITT-FALSE (`poly = 0x1021`, `init = 0xFFFF`, 无反射, 最终 XOR `0x0000`)

### 5.1 Direct-to-Ring (D2R) 机制

AxiomTrace 采用 **Direct-to-Ring (D2R)** 机制，以最大程度减少 RAM 开销和延迟：

1. **零栈缓冲 (Zero Stack Buffering)**：编码器不再在临时栈缓冲区中组装整个帧，而是直接在环形缓冲区中申请空间并按字段写入。
2. **增量 CRC (Incremental CRC)**：在写入每个字节时同步增量计算 CRC-16。这消除了对帧进行二次遍历的需求，并避免了在栈上存储完整帧。
3. **原子提交 (Atomic Commit)**：环形缓冲区的“写指针 (Head)”仅在完整帧（包括 CRC）成功写入后才更新，确保部分写入或中断的写入不会损坏流数据。

对于字节流传输（UART, USB CDC），整个帧可能会进行 COBS 编码并附加 `0x00` 分隔符。传输差异请参见 `spec/wire_format.md`。

---

## 6. 事件字典映射

每个 `(module_id, event_id)` 对都映射到一个字典条目：

```json
{
  "0x0305": {
    "module": "MOTOR",
    "level": "WARN",
    "name": "MOTOR_CURRENT_OVER_LIMIT",
    "text": "motor current over limit: phase={u8}, current={i16}, limit={i16}"
  }
}
```

`text` 字段使用带类型提示的命名占位符，这些类型定义 v2 packed 参数布局。校验工具会拒绝损坏的 payload 或不匹配的 bundle identity。

**字典来源**（每个项目选其一）：
1. **X-Macro** (`axiom_events.h`)：在编译期展开的 C 宏；由 `extract_dict.py` 提取生成 JSON。
2. **YAML**：Layer 3 高阶工作流；由 `codegen.py` 生成 C 宏和字典 JSON。
3. **手动 JSON**：针对小型项目，直接编辑 `dictionary.json`。

---

## 7. 约束

- 协议的有效载荷长度字段容量为 **255 字节**；当前固件配置中 `AXIOM_MAX_PAYLOAD_LEN` 默认为 **128 字节**。
- 最大模块数量：**256**（`module_id` 为 `uint8_t`）。
- 每个模块最大事件数：**65536**（`event_id` 为 `uint16_t`，实际上受限于 ROM 大小）。
- 报头、时间戳和有效载荷始终受 CRC 保护。
- **栈效率**：通过使用 D2R 机制，无论 `AXIOM_MAX_PAYLOAD_LEN` 设置为何值，栈空间占用都减少到几十个字节。不再需要大型局部数组。

---

## 8. 版本控制

- `version` 字节 = `major << 4 | minor`
- **主版本号 (Major)** 变更 = 不兼容的报头或 payload 解释变化。
- **次版本号 (Minor)** 变更 = 同一主版本内向后兼容的增加。

当前发出的有线格式版本：**v2.0** (`version = 0x20`)。主机 decoder 保留对历史 **v1.0/v1.1** typed payload 的结构解码能力。
