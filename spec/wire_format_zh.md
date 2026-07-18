> [English](wire_format.md) | [简体中文](wire_format_zh.md)

# AxiomTrace 有线格式规范

> 版本：v2.0  |  状态：**已冻结**  |  对应代码：`baremetal/core/axiom_event.c`, `baremetal/core/axiom_crc.c`

---

## 1. 范围

本文定义了 AxiomTrace 事件记录如何序列化为字节流，以便通过 UART、USB、CAN-FD 或内存缓冲区进行传输。

Wire `v2.0` 是当前发出的格式。任何修改都必须同步更新规范、Golden Frame、解码器，并通过完整的回归测试。

---

## 2. 帧结构

有线传输帧由以下部分组成：

```
[ 帧主体 ]
  ├── 报头 (8 字节)
  ├── 时间戳 (1..5 字节, 差分压缩)
  ├── 有效载荷长度 (1 字节)
  ├── 有效载荷 (0..255 字节)
  └── CRC-16 (2 字节)
[ 可选传输封装 ]
```

### 2.1 时间戳

时间戳字段由 `axiom_write()` 为每个事件自动插入。它使用相对于前一个事件原始时间戳（来自 `axiom_port_timestamp()`）的差分压缩：

| 差分范围 | 字节数 | 编码方式 |
|---------------|-------|----------|
| 0 – 127 | 1 | `0b0xxxxxxx` (7 位值) |
| 128 – 16,383 | 2 | `0b10xxxxxx` + 1 字节 (13 位值) |
| 16,384 – 2,097,151 | 3 | `0b110xxxxx` + 2 字节 (21 位值) |
| 2,097,152 – 4,294,967,295 | 5 | `0xFE` + 4 字节 (完整 32 位) |

时间戳包含在 CRC-16 计算范围内（报头 + 时间戳 + 有效载荷长度 + 有效载荷）。解码器必须先解码变长的时间戳字段，才能确定 `payload_len` 在 `frame[8 + ts_len]` 的位置。

### 2.2 可选定位与元数据身份字段

Wire `v2.0` 保持 8 字节报头和时间戳布局不变。普通事件参数是依赖匹配 dictionary 确定长度和类型的 packed 值；主机解码 metadata 仍作为 tagged suffix 附加在该 payload 末尾：

| 标签 | 标签后的编码 | 用途 |
|------|--------------|------|
| `0x0A` | `mode=1, file_hash:u16, line:u16, func_hash:u16` 或 `mode=2, file_id:u16, line:u16` | 可选调用点定位 |
| `0x0B` | `metadata_id:8` | 选择精确匹配的主机 metadata bundle |

计入标签后，`0x0A` 在 `HASH` 模式增加 8 字节，在 `FILE_ID` 模式增加 6 字节。`0x0B` 总计增加 9 字节，通常发在系统元数据身份事件 `(module_id = 0, event_id = 2)` 中。

系统保留 payload 是应用 dictionary 解码规则的例外：`AX_PROBE` `(0,0)` 保留带 tag 的 `tag_hash`/value 字段，`DROP_SUMMARY` `(0,1)` 使用固定 packed `u32/u8/u16` 字段，metadata identity `(0,2)` 必须恰好包含其九字节 tagged identifier。

### 2.3 字节序

Frame Body 内所有多字节字段始终采用 **小端序 (Little-endian)**。外部传输封装不得改变 CRC 覆盖的 Frame Body。

### 2.4 对齐

有线格式是非对齐的（紧凑型）。编码器使用 `memcpy` 或按字节写入，以避免在不支持非对齐访问的架构上出现未定义行为。解码器也必须遵循此原则。

---

## 3. 流式成帧与重同步

Core 发出的 Wire v2 就是上述原始 Frame Body；Core 不执行 COBS，也不追加分隔符。流式解码器搜索同步字节 `0xA5`，依次校验 version、level、timestamp length、payload length、最大帧长与 CRC；候选无效时前进一个字节。Core flush 与 Deferred flush 使用同一套帧边界校验。

外部传输可以在 AxiomTrace 之外封装完整 Frame Body，但该封装不属于 Wire v2；使用标准 decoder 前必须先移除封装。

---

## 4. CRC-16

- **算法**：CRC-16/CCITT-FALSE (`poly = 0x1021`, `init = 0xFFFF`, 无反射, 最终 XOR `0x0000`)。
- **覆盖范围**：报头 (8B) + 时间戳 (变长) + 有效载荷长度 (1B) + 有效载荷 (N 字节)。
- **计算**：预置 256 字节 ROM 查找表，实现 O(n) 处理速度。
- **校验**：解码器在相同范围内重新计算 CRC，并与末尾的 2 字节进行比较。不匹配则标记为 `FRAME_INVALID`。
- **错误恢复**：CRC 失败会拒绝当前候选帧；随后逐字节搜索下一个有效同步点。

---

## 5. 传输差异

Memory 与 Deferred Backend 对完整 Frame Body 逐字节保真。仓库中的硬件传输源码属于 reference 集成；USB CDC、SWO/ITM 与 CAN-FD 契约不在 v1.0 范围。自定义 Backend 必须传递完整帧，且不得修改 CRC 覆盖的 Frame Body。

---

## 6. 解码器行为

### 6.1 有效帧

当且仅当满足以下条件时，帧才有效：
1. `sync == 0xA5`
2. `version` 的高 4 位（主版本号）受支持（当前发出 `0x2`，同时支持历史 `0x1`）
3. `level` 的高 4 位为 `0`
4. 时间戳可解码（报头后的第一个字节标示长度 1/2/3/5）
5. 在读取 CRC 前，实际读取的有效载荷字节数与 `payload_len` 相符
6. 在报头 + 时间戳 + 有效载荷长度 + 有效载荷上重新计算的 CRC-16 与末尾 2 字节一致

### 6.2 无效帧处理

在任何校验失败时，解码器必须：
1. 显式拒绝该帧（返回 `FRAME_INVALID` 或等效值）。
2. 前进一个字节并搜索下一个 `0xA5` 同步候选。
3. 从下一个候选帧恢复解码。
4. **绝不崩溃或进入未定义状态**。

### 6.3 Schema 与未知后缀

对于 wire `v2`，没有匹配 dictionary 时，decoder 只能将普通 payload 作为原始字节输出，因为无法推断字段边界。提供 dictionary 后，预期 packed 参数之后仅允许 `0x0A` 或 `0x0B` metadata suffix；未知 suffix 被标记为损坏 payload。对于历史 wire `v1.x`，decoder 继续按逐字段 type tag 解码，并在遇到未知 tag 时报告警告而不崩溃。

---

## 7. 版本控制

报头中的 `version` 字节编码为 `major << 4 | minor`。

- 解码器 **必须拒绝** 不支持的 **主版本号 (Major)**。
- **次版本号 (Minor)** 的增加必须在同一个主版本 payload 解释方式内保持兼容。

当前发出版本：**0x20** (v2.0)。主机 decoder 继续解析历史 typed-payload `v1.0` 与 `v1.1` frame body。

---

## 8. 帧常量（命名定义）

以下命名常量定义在 `baremetal/core/axiom_event.h` 中，用于替换硬编码魔数：

| 常量 | 值 | 描述 |
|------|-----|------|
| `AXIOM_SYNC_BYTE` | `0xA5u` | 帧起始同步字节 |
| `AXIOM_HEADER_LEN` | `8u` | 报头：sync(1) + version(1) + level(1) + module_id(1) + event_id(2) + seq(2) |
| `AXIOM_CRC_LEN` | `2u` | 尾部 CRC-16/CCITT-FALSE 字节 |
| `AXIOM_MAX_TIMESTAMP_LEN` | `5u` | 最大变长时间戳编码 |
| `AXIOM_MAX_PAYLOAD_LEN` | `128u` | 每个事件的最大有效载荷字节 |
| `AXIOM_TAG_SIZE` | `0u` | Wire v2 packed payload 中普通参数的 type-tag 开销 |
