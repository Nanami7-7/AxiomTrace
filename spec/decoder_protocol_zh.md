> [English](decoder_protocol.md) | **简体中文**

# AxiomTrace Decoder 协议规范

> Wire 版本：v2.0 | 状态：**构建中**

---

## 1. 概览

AxiomTrace decoder 工具链运行在主机端，将 MCU 输出的二进制 Event Records / Fault Capsules 转换为人类可读格式。固件侧只输出二进制事实，语义解释由主机端 dictionary 和标准 metadata bundle 完成。

**组件：**

- `axiom-decoder`：解析二进制帧并输出 text/json/jsonl/raw。
- `axiom-codegen`：从事件定义生成 C 头文件和 `dictionary.json`。
- `axiom-bundle`：生成标准 `axiomtrace-bundle`。
- `axiom-validate`：校验 dictionary、事件源、bundle 和 golden 输入。

---

## 2. 输入格式

### 2.1 原始二进制流

由完整 v2.0 frame 串接而成；历史 v1.0/v1.1 typed-payload frame 仍可进行结构解码。UART/USB 传输可以额外使用 COBS 和 `0x00` 分隔，decoder 负责按输入模式恢复帧。

```bash
axiom-decoder trace.bin --bundle build/axiomtrace-bundle --format text
```

### 2.2 内存转储

内存转储包含 ring metadata 和 raw ring 数据。首版工具链可以先以 raw 结构化输出为目标，后续补全 memory 输入模式。

```bash
axiom-decoder trace_ram.bin --bundle build/axiomtrace-bundle --format raw
```

### 2.3 Fault Capsule

Fault Capsule 二进制格式见 `spec/fault_capsule_zh.md`。首版工具链先保证 raw decode 和 bundle 匹配，完整 fault report 在 capsule report 阶段实现。

```bash
axiom-decoder capsule.bin --bundle build/axiomtrace-bundle --format raw
```

---

## 3. 输出格式

### 3.1 文本输出

默认面向人阅读，使用 dictionary 模板填充参数：

```text
[   42] [INFO ] [MOTOR] START: rpm=3200
[   43] [WARN ] [MOTOR] CURRENT_OVER_LIMIT: phase=2, current=1520, limit=1200
```

### 3.2 JSON / JSON Lines

`json` 用于小型文件整体导出，`jsonl` 用于流式处理和管道集成：

```json
{
  "metadata_id": "7f3a9c2e1b4d6a80",
  "seq": 42,
  "level": "INFO",
  "module": "MOTOR",
  "event": "START",
  "args": {"rpm": 3200}
}
```

### 3.3 Raw 输出

`raw` 保留 frame 结构字段，不依赖 dictionary 或 bundle，适合固件身份不匹配或调试损坏输入时使用。

---

## 4. Dictionary 与 Bundle

`dictionary.json` 仍是事件语义核心；标准 `axiomtrace-bundle` 是推荐入口，统一承载：

```text
manifest.json
 dictionary.json
 source_map.json
 build_info.json
 firmware.elf
 firmware.map
```

规则：

- `axiom-decoder --bundle` 优先使用 bundle。
- `axiom-decoder --bundle-store` 通过 trace 中的 `metadata_id` 事件查找匹配 bundle。
- 找不到 bundle 时可降级 raw decode。
- `--dictionary/-d` 保留为兼容路径。

---

## 5. CLI 接口

```bash
# 推荐：bundle 语义解码
axiom-decoder trace.bin --bundle build/axiomtrace-bundle --format text

# JSON 导出
axiom-decoder trace.bin --bundle build/axiomtrace-bundle --format json > trace.json

# 兼容路径：只使用 dictionary
axiom-decoder trace.bin --dictionary dictionary.json --format jsonl

# 校验 bundle/golden
axiom-validate --bundle build/axiomtrace-bundle --golden golden/

# 生成 codegen 和 bundle
axiom-codegen --events events.yaml --out build/generated
axiom-bundle generate --events events.yaml --compile-db build/compile_commands.json --out build/axiomtrace-bundle
```

---

## 6. 错误处理

| 错误码 | 描述 | Decoder 行为 |
|--------|------|--------------|
| `FRAME_INVALID` | CRC、同步字节或版本错误 | 跳过损坏帧并继续寻找下一个同步点 |
| `UNKNOWN_EVENT_SCHEMA` | v2 dictionary 中不存在 `(module_id,event_id)` | 输出原始 ID 和 payload；语义校验失败 |
| `TRUNCATED_PAYLOAD` | payload 长度超过剩余输入 | 标记截断并跳过该帧 |
| `UNKNOWN_METADATA_SUFFIX` | v2 packed 参数后存在未知 metadata suffix | 标记损坏；语义校验失败 |
| `INVALID_METADATA_IDENTITY` | 系统 metadata identity payload 不是恰好一个 tag 加 8 字节 ID | 标记损坏；语义校验失败 |
| `UNKNOWN_TYPE_TAG` | 历史 v1 中存在未知 type tag | 标记 unknown，decoder 不崩溃 |

关键规则：无论输入如何损坏，decoder 都不能崩溃或进入未定义状态。
