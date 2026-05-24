> [English](toolchain_ecosystem_design.md) | [简体中文](toolchain_ecosystem_design_zh.md)

# AxiomTrace 工具链与生态系统设计文档

> 版本：v0.4-draft  
> 作者：toolchain-ecosystem-designer  
> 目标：定义 Python 工具链、Golden Frame 验证流程及生态接入规范，确保主机端“一键恢复可读性”。

---

## 目录

1. [YAML 字典 Schema](#1-yaml-字典-schema)
2. [Codegen 工具设计](#2-codegen-工具设计)
3. [元数据包标准](#3-元数据包标准)
4. [Decoder 工具设计](#4-decoder-工具设计)
5. [Validator 工具设计](#5-validator-工具设计)
6. [Benchmark 工具设计](#6-benchmark-工具设计)
7. [RTOS / Linux 接入文档框架](#7-rtos--linux-接入文档框架)
8. [一键式工作流体验总结](#8-一键式工作流体验总结)

---

## 1. YAML 字典 Schema

### 1.1 顶层结构

```yaml
version: "1.0"                 # 必填，字典格式版本，当前仅支持 "1.0"
dictionary:
  modules: []                  # 必填，模块列表，至少包含一个模块
  metadata: {}                 # 可选，构建级元数据
```

### 1.2 字段定义（精确到类型与约束）

| 路径 | 类型 | 必填 | 约束 / 说明 |
|------|------|------|-------------|
| `version` | `string` | 是 | 枚举 `"1.0"` |
| `dictionary` | `object` | 是 | 根容器 |
| `dictionary.modules` | `array[module]` | 是 | 长度 `1..256` |
| `module.id` | `uint8`（YAML 支持 `0x03` 或 `3`） | 是 | 唯一，范围 `0x00..0xFF` |
| `module.name` | `string` | 是 | `^[A-Z][A-Z0-9_]{0,31}$`，大写下划线命名 |
| `module.description` | `string` | 否 | 长度 `0..256` |
| `module.events` | `array[event]` | 是 | 长度 `1..65535`（实际受 ROM 限制） |
| `event.id` | `uint16` | 是 | 模块内唯一，范围 `0x0000..0xFFFF` |
| `event.name` | `string` | 是 | `^[A-Z][A-Z0-9_]{0,63}$` |
| `event.level` | `string` | 是 | 枚举：`DEBUG`, `INFO`, `WARN`, `ERROR`, `FAULT` |
| `event.text` | `string` | 是 | 人类可读模板，含 `{name:type}` 占位符 |
| `event.args` | `array[arg]` | 否 | 若 `text` 含占位符则必填，且顺序与占位符一致 |
| `arg.name` | `string` | 是 | 与 `text` 中占位符名称严格匹配 |
| `arg.type` | `string` | 是 | 见下表 |

### 1.3 支持的参数类型

| `arg.type` | Payload Type Tag | C 类型 | Wire 大小 | 说明 |
|------------|------------------|--------|-----------|------|
| `bool` | `0x00` | `bool` | 1 | `0` 或 `1` |
| `u8` | `0x01` | `uint8_t` | 1 | 无符号 8-bit |
| `i8` | `0x02` | `int8_t` | 1 | 有符号 8-bit |
| `u16` | `0x03` | `uint16_t` | 2 | 小端 |
| `i16` | `0x04` | `int16_t` | 2 | 小端补码 |
| `u32` | `0x05` | `uint32_t` | 4 | 小端 |
| `i32` | `0x06` | `int32_t` | 4 | 小端补码 |
| `f32` | `0x07` | `float` | 4 | IEEE-754 单精度小端 |
| `ts` | `0x08` | `uint32_t` | 4 | 压缩相对时间戳 |
| `bytes` | `0x09` | `uint8_t[]` | `1+N` | 1-byte 长度前缀 + 原始字节 |

### 1.4 验证规则

1. **全局唯一性**：`(module.id, event.id)` 二元组在整个字典中必须唯一。
2. **名称唯一性**：`module.name` 全局唯一；`event.name` 在模块内唯一。
3. **占位符一致性**：`event.text` 中的所有 `{name:type}` 必须满足：
   - `name` 存在于 `event.args` 中；
   - `type` 与对应 `arg.type` 完全一致；
   - 占位符出现顺序与 `args` 数组顺序一致。
4. **级别匹配**：`event.level` 必须与事件语义一致（Validator 负责告警，Codegen 负责生成对应宏）。
5. **Payload 上限**：根据 `args` 计算出的最大 payload 长度不得超过 `255` 字节。

### 1.5 YAML → JSON Collateral 转换规则

Codegen 读取 YAML，输出 `dictionary.json`（Decoder 的 Collateral）。转换规则如下：

- 所有键名保持小写蛇形（snake_case）。
- `module.id` 与 `event.id` 以十进制整数输出（Decoder 侧便于索引）。
- `level` 转为小写字符串。
- `text` 中的占位符被提取为结构化数组 `placeholders`，保留原始 `text` 供人类阅读。
- 计算并附加 `max_payload_size` 到每个 event。

**示例转换：**

```yaml
# 输入片段
dictionary:
  modules:
    - id: 0x03
      name: MOTOR
      events:
        - id: 0x01
          name: START
          level: INFO
          text: "Motor started: rpm={rpm:u16}, ts={ts:ts}"
          args:
            - name: rpm
              type: u16
            - name: ts
              type: ts
```

```json
{
  "version": "1.0",
  "modules": {
    "3": {
      "name": "MOTOR",
      "description": "",
      "events": {
        "1": {
          "name": "START",
          "level": "info",
          "text": "Motor started: rpm={rpm:u16}, ts={ts:ts}",
          "args": [
            {"name": "rpm", "type": "u16", "type_tag": 3, "wire_size": 2},
            {"name": "ts", "type": "ts", "type_tag": 8, "wire_size": 4}
          ],
          "max_payload_size": 6
        }
      }
    }
  }
}
```

---

## 2. Codegen 工具设计

### 2.1 命令行接口

```bash
axiom-codegen \
    --events events.yaml \
    --out gen/ \
    [--lang c] \
    [--check] \
    [--watch]
```

| 选项 | 说明 |
|------|------|
| `--events` | 输入 YAML/JSON 字典路径（必填） |
| `--out` | 输出目录（默认 `gen/`） |
| `--lang` | 目标语言，目前仅 `c`（默认） |
| `--check` | 仅执行验证，不生成文件，退出码非 0 表示失败 |
| `--watch` | 监视 YAML 变更并自动重新生成（开发模式） |

### 2.2 生成产物清单

| 产物名 | 说明 | 消费者 |
|--------|------|--------|
| `axiom_events_generated.h` | C 宏：`MODULE_EVENT_NAME` → `(module_id, event_id)` | 固件编译 |
| `axiom_modules_generated.h` | Module ID 枚举 / 常量 | 固件编译 |
| `dictionary.json` | 紧凑 JSON Collateral | Decoder / Validator |
| `.axiom_codegen.d` | 依赖文件（记录输入时间戳） | 增量构建 |

### 2.3 `axiom_events_generated.h` 生成模板

```c
/* 自动生成，请勿手动编辑 */
#ifndef AXIOM_EVENTS_GENERATED_H
#ifndef AXIOM_EVENTS_GENERATED_H
#define AXIOM_EVENTS_GENERATED_H

#define _AXIOM_EVENTS_GENERATED_H 1

/* ---------------------------------------------------------------------------
 * Module IDs
 * --------------------------------------------------------------------------- */
#define _AXIOM_MODULE_ID_MOTOR   0x03u
#define _AXIOM_MODULE_ID_SENSOR  0x05u
/* ... */

/* ---------------------------------------------------------------------------
 * Event IDs & Convenience Macros
 * --------------------------------------------------------------------------- */
#define _AXIOM_EVENT_ID_MOTOR_START   0x0001u
#define _AXIOM_EVENT_ID_MOTOR_STOP    0x0002u

/* 带参数个数的元信息（用于静态断言） */
#define _AXIOM_EVENT_ARGC_MOTOR_START 2

/* 预编译级别掩码（可选优化） */
#define _AXIOM_EVENT_LEVEL_MOTOR_START AXIOM_LEVEL_INFO

/* 辅助：将 MODULE_EVENT 转换为字符串（用于调试） */
#define _AXIOM_EVENT_NAME_MOTOR_START "MOTOR_START"

#endif /* AXIOM_EVENTS_GENERATED_H */
```

**生成逻辑伪代码：**

```python
def emit_event_macro(module_name: str, event: Event) -> str:
    macro_base = f"_AXIOM_EVENT_ID_{module_name}_{event.name}"
    lines = [
        f"#define {macro_base} 0x{event.id:04X}u",
        f"#define _AXIOM_EVENT_ARGC_{module_name}_{event.name} {len(event.args)}",
        f"#define _AXIOM_EVENT_LEVEL_{module_name}_{event.name} AXIOM_LEVEL_{event.level}",
        f"#define _AXIOM_EVENT_NAME_{module_name}_{event.name} \"{module_name}_{event.name}\"",
    ]
    return "\n".join(lines)
```

### 2.4 增量构建支持

Codegen 在输出目录写入 `.axiom_codegen.d` 依赖文件：

```makefile
gen/axiom_events_generated.h gen/axiom_modules_generated.h gen/dictionary.json: \
    events.yaml \
    $(CODEGEN_DIR)/templates/c_header.j2
```

CMake 通过 `add_custom_command` 读取该依赖文件，实现真正的增量构建。

### 2.5 CMake `add_custom_command` 集成

```cmake
# 在固件 CMakeLists.txt 中
find_package(Python3 REQUIRED COMPONENTS Interpreter)

set(AXIOM_DICT_YAML "${CMAKE_SOURCE_DIR}/events.yaml")
set(AXIOM_GEN_DIR   "${CMAKE_BINARY_DIR}/generated")
file(MAKE_DIRECTORY ${AXIOM_GEN_DIR})

add_custom_command(
    OUTPUT
        ${AXIOM_GEN_DIR}/axiom_events_generated.h
        ${AXIOM_GEN_DIR}/axiom_modules_generated.h
        ${AXIOM_GEN_DIR}/dictionary.json
    COMMAND ${Python3_EXECUTABLE} -m axiomtrace_tools.cli codegen
            --events ${AXIOM_DICT_YAML}
            --out ${AXIOM_GEN_DIR}
    DEPENDS ${AXIOM_DICT_YAML}
    COMMENT "Generating AxiomTrace event dictionary..."
    VERBATIM
)

add_custom_target(axiom_codegen_target ALL
    DEPENDS
        ${AXIOM_GEN_DIR}/axiom_events_generated.h
        ${AXIOM_GEN_DIR}/axiom_modules_generated.h
        ${AXIOM_GEN_DIR}/dictionary.json
)

# 固件目标依赖生成产物
target_include_directories(axiomtrace PUBLIC ${AXIOM_GEN_DIR})
add_dependencies(axiomtrace axiom_codegen_target)
```

---

## 3. 元数据包标准

Decoder 应消费标准元数据包，而不是项目私有配置文件。元数据包是主机端恢复事件语义、源码定位和故障地址的唯一包格式。

### 3.1 Bundle 目录结构

```text
axiomtrace-bundle/
  manifest.json       # 入口：schema、固件身份、产物路径
  dictionary.json     # 事件语义：模块、事件、参数名和模板
  source_map.json     # 解码元数据：文件 ID/hash 与定位显示路径
  build_info.json     # 目标芯片、编译器、profile、git/build 信息
  firmware.elf        # 推荐：DWARF/addr2line 解析 PC/LR/函数名
  firmware.map        # 可选：符号和体积分析兜底
```

每个 v1 bundle 都包含 `manifest.json`、`dictionary.json`、`source_map.json` 和 `build_info.json`。关闭源码定位时，`source_map.json` 可以不含文件条目。生产诊断推荐保留 `firmware.elf`。

### 3.2 Manifest 契约

`manifest.json` 是 decoder 唯一需要显式接收的入口：

```json
{
  "schema": "axiomtrace.bundle.v1",
  "bundle_version": 1,
  "metadata": {
    "id": "7f3a9c2e1b4d6a80",
    "wire_version": "1.1",
    "identity_basis": ["wire_version", "location", "dictionary", "source_map"]
  },
  "firmware": {
    "name": "motor-controller",
    "version": "1.2.3"
  },
  "artifacts": {
    "dictionary": "dictionary.json",
    "source_map": "source_map.json",
    "build_info": "build_info.json",
    "elf": "firmware.elf",
    "map": "firmware.map"
  },
  "location": {
    "mode": "file_id",
    "line_width_bits": 16,
    "function_hash": false
  },
  "decoder": {
    "default_format": "text",
    "require_metadata_id_match": true
  }
}
```

规则：

- `schema` 在这一代固定为 `axiomtrace.bundle.v1`。
- `metadata.id` 标识解释 trace 所需的精确 wire 版本、定位契约、dictionary 与 source map；语义解码前必须匹配。
- `artifacts` 路径相对 `manifest.json` 所在目录。
- `location.mode` 取值为 `none`、`hash` 或 `file_id`。
- `location.function_hash` 只在 `hash` 模式生效；生产应优先使用 `file_id`，需要函数信息时由保留的 ELF/DWARF 产物重建。

### 3.3 源码定位 Profile

| 模式 | MCU Payload | MCU 开销 | 主机端要求 | 场景 |
|------|-------------|----------|------------|------|
| `none` | 无源码字段 | 最低 | 仅 dictionary | 极简生产固件 |
| `hash` | `file_hash`、`line`、`func_hash`（关闭时为零） | 运行时字符串 hash 和 ROM 字符串 | source map 或原始 hash 显示 | 构建系统无关开发 |
| `file_id` | `file_id`、`line` | 固定整数编码，无字符串扫描 | 自动生成 source map | 生产源码行号诊断 |

在 `hash` 模式下，`source_map.json` 的 `hash16` 基于构建系统传给编译器、固件通过 `__FILE__` 看到的精确源码路径拼写生成；用于展示的 `path` 可以独立归一化。

生产默认：

```c
#define AXIOM_CFG_LOCATION_MODE AXIOM_CFG_LOCATION_MODE_FILE_ID
#define AXIOM_CFG_LOCATION_FUNCTION 0
```

函数名通常应由主机端通过 `firmware.elf` 和行号元数据还原。生产 MCU 不应传输函数名字符串。

### 3.4 标准工作流

标准解码流程：

```bash
axiom-decoder trace.bin --bundle build/axiomtrace-bundle --format text
```

标准 bundle 生成流程：

```bash
axiom-bundle generate \
  --events events.yaml \
  --elf build/firmware.elf \
  --compile-db build/compile_commands.json \
  --out build/axiomtrace-bundle
```

构建系统集成必须生成这个标准 bundle，而不是项目私有 decoder 配置。CMake 应暴露一个稳定 helper：

```cmake
axiomtrace_add_bundle(
    TARGET firmware
    EVENTS ${CMAKE_SOURCE_DIR}/events.yaml
    OUTPUT_DIR ${CMAKE_BINARY_DIR}/axiomtrace-bundle
)
```

Decoder 查找顺序：

1. 显式提供 `--bundle` 时直接使用。
2. 提供 `--bundle-store` 时，根据 trace 内的 `metadata_id` 事件选择匹配 bundle。
3. 自动查找 `build/axiomtrace-bundle`、`axiomtrace-bundle`、`.axiom/bundle`。
4. 如果没有 bundle，则降级为 raw 结构化解码。

### 3.5 文档归属

本节是 bundle 语义的唯一详细规范。README 可以链接到本节，但不重复完整 schema。实现细节应放在工具模块 docstring 或 CLI help 中，不再新增临时 Markdown 文件。

---

## 4. Decoder 工具设计

### 4.1 输入源

| 输入类型 | 说明 | CLI 示例 |
|----------|------|----------|
| 原始二进制文件 | 从串口 / 仿真器 dump 的 `.bin` | `--input trace.bin` |
| 串口实时流 | 直接读取 UART / USB CDC | `--serial COM3 --baud 115200` |
| Hex dump | 文本格式的十六进制（如 `xxd` 输出） | `--hex-dump trace.hex` |

### 4.2 处理流水线

```
Input (bin / serial / hex)
    │
    ▼
[ COBS 解码 ]  ──►  按 0x00 分隔提取帧（Memory/SWO 输入跳过）
    │
    ▼
[ CRC-16/CCITT 校验 ]  ──►  失败则标记 CORRUPT，继续同步
    │
    ▼
[ Header 解析 ]  ──►  8-byte packed struct → 字段提取
    │
    ▼
[ Payload 反序列化 ]  ──►  按 type tag 逐个解析参数
    │
    ▼
[ 字典查询 ]  ──►  以 (module_id, event_id) 检索 dictionary.json
    │
    ▼
[ 格式化输出 ]
```

**Payload 反序列化伪代码：**

```python
def deserialize_payload(payload: bytes, schema: List[Arg]) -> List[Tuple[str, Any]]:
    pos = 0
    results = []
    for arg in schema:
        if arg.type == "u8":
            val = payload[pos]; pos += 1
        elif arg.type == "u16":
            val = int.from_bytes(payload[pos:pos+2], "little"); pos += 2
        elif arg.type == "f32":
            val = struct.unpack("<f", payload[pos:pos+4])[0]; pos += 4
        elif arg.type == "bytes":
            length = payload[pos]; pos += 1
            val = payload[pos:pos+length]; pos += length
        # ... 其他类型
        results.append((arg.name, val))
    return results
```

### 4.3 输出格式

#### A. 人类可读文本（默认，类似 `dmesg`）

```
[  0.001234] INFO  [MOTOR] START: Motor started: rpm=3200, ts=1234
[  0.002100] WARN  [SENSOR] TEMP_HIGH: temperature over threshold: adc=0x1A3F, limit=80
```

- 时间戳由 Decoder 根据首帧绝对时间 + 相对 `ts` 字段计算。
- 若字典缺失，则回退为原始十六进制：
  ```
  [  0.001234] RAW   [mod=0x03 evt=0x01] 40 0D 00 00 D2 04 00 00
  ```

#### B. JSON Lines（每行一个 JSON 对象）

```bash
axiom-decoder trace.bin --format jsonl
```

```json
{"timestamp_us":1234,"level":"INFO","module":"MOTOR","event":"START","args":{"rpm":3200,"ts":1234},"raw_hex":"a5 10 01 03 01 00 00 00 06 40 0d d2 04"}
{"timestamp_us":2100,"level":"WARN","module":"SENSOR","event":"TEMP_HIGH","args":{"adc":6719,"limit":80}}
```

#### C. 可视化时间轴输入格式

为后续前端 / 脚本生成兼容 Chrome Trace Event Format 的 JSON：

```json
{
  "traceEvents": [
    {"name": "MOTOR_START", "ph": "i", "ts": 1234, "pid": 1, "tid": 1, "args": {"rpm": 3200}}
  ]
}
```

### 4.4 命令行接口设计

```bash
axiom-decoder \
    trace.bin \
    --dictionary dictionary.json \
    --format text | jsonl | trace \
    [--start-time "2026-04-28T00:00:00Z"] \
    [--filter-level INFO] \
    [--filter-module MOTOR,SENSOR] \
    [--output decoded.log]
```

| 选项 | 说明 |
|------|------|
| `--input` / `-i` | 输入文件（或 `--serial`） |
| `--dictionary` / `-d` | Collateral JSON 路径 |
| `--format` / `-f` | 输出格式：`text`（默认）、`jsonl`、`trace` |
| `--start-time` | 首帧绝对时间（ISO 8601），用于计算真实时间戳 |
| `--filter-level` | 仅输出 >= 指定级别的日志 |
| `--filter-module` | 仅输出指定模块的日志，逗号分隔 |
| `--output` / `-o` | 输出文件（默认 stdout） |

---

## 5. Validator 工具设计

### 5.1 Golden Frame 目录结构

```
tests/golden/
├── cases/
│   ├── motor_start/
│   │   ├── frame.bin          # 原始二进制帧（COBS + CRC + 0x00 分隔）
│   │   ├── metadata.yaml      # 测试元数据
│   │   └── expected.txt       # 期望的 text 输出
│   │   └── expected.jsonl     # 期望的 jsonl 输出（可选）
│   ├── sensor_temp_high/
│   │   ├── frame.bin
│   │   ├── metadata.yaml
│   │   └── expected.txt
│   └── README.md              # 用例编写规范
└── baseline/
    └── v0.3.0/                # 基线输出，用于回归比对
```

### 5.2 测试用例格式（metadata.yaml）

```yaml
name: "MOTOR_START basic encoding"
description: "验证 MOTOR/START 事件的完整帧编码"
dictionary: "../../events.yaml"   # 相对路径或绝对路径

# 帧属性预期
golden:
  header:
    sync: 0xA5
    version: 0x11          # v1.1
    level: INFO            # 对应数值 1
    module_id: 0x03
    event_id: 0x01
    seq: 0x0000            # 序列号（允许通配或范围）
  payload_len: 6           # 预期 payload 长度
  crc_valid: true          # CRC 必须通过校验

# Decoder 输出预期
expected:
  text: "[  0.000000] INFO  [MOTOR] START: Motor started: rpm=3200, ts=1234"
  jsonl:
    level: "INFO"
    module: "MOTOR"
    event: "START"
    args:
      rpm: 3200
      ts: 1234
```

### 5.3 一致性检查规则

Validator 对每个 `frame.bin` 执行以下检查：

| 检查项 | 规则 | 失败级别 |
|--------|------|----------|
| **CRC 校验** | 计算 Header + PayloadLen + Payload 的 CRC-16，与帧尾比对 | ERROR |
| **字节序** | 多字节字段（`event_id`, `seq`, 数值参数）必须为小端 | ERROR |
| **对齐** | Header 为 packed 8-byte，无填充；Payload 按 type tag 顺序紧密排列 | ERROR |
| **类型匹配** | Payload 中的 type tag 序列必须与 dictionary 中 `args` 的 `type_tag` 完全一致 | ERROR |
| **级别匹配** | Header 中的 `level` 数值必须与 dictionary 中 `event.level` 一致 | WARN |
| **长度边界** | `payload_len` 不得超过 255；总帧长不得超过后端缓冲区限制 | ERROR |
| **同步恢复** | 若帧损坏，Decoder 必须能在下一个 `0x00` 分隔符处恢复同步 | WARN |

### 5.4 CI 集成方式

在 GitHub Actions / GitLab CI 中：

```yaml
# .github/workflows/validate.yml
name: Golden Frame Validation
on: [push, pull_request]
jobs:
  validate:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - uses: actions/setup-python@v5
        with:
          python-version: '3.11'
      - run: python -m pip install -e ./tool
      - run: |
          axiom-codegen --events events.yaml --out gen/
          axiom-validate \
              --dictionary gen/dictionary.json \
              --cases tests/golden/cases/ \
              --report junit.xml
      - uses: mikepenz/action-junit-report@v4
        with:
          report_paths: 'junit.xml'
```

Validator 输出 JUnit XML，便于 CI 系统展示失败用例。

---

## 6. Benchmark 工具设计

### 6.1 测量维度

| 维度 | 测量方法 | 工具 / 来源 |
|------|----------|-------------|
| **Cycles** | 每条日志的 CPU 周期数 | DWT CYCCNT（Cortex-M）或外部逻辑分析仪采样 |
| **RAM** | `.bss` + `.data` 中 AxiomTrace 相关符号 | map 文件解析，交叉工具链可选 |
| **ROM** | `.text` + `.rodata` 中 AxiomTrace 相关符号 | CMake/size 输出或 map 文件解析 |
| **Throughput** | 后端最大可持续帧率 | 逻辑分析仪统计单位时间内的完整帧数 |

### 6.2 报表格式

#### Markdown（默认，适合嵌入 README / PR）

```markdown
## AxiomTrace Benchmark Report

| Metric | Value | Unit | Baseline | Delta |
|--------|-------|------|----------|-------|
| Cycles per log | 42 | cycles | 38 | +10.5% :warning: |
| RAM footprint | 4,224 | bytes | 4,096 | +3.1% |
| ROM footprint | 8,192 | bytes | 8,192 | 0.0% |
| UART throughput | 125,000 | frames/sec | 120,000 | +4.2% |
```

#### JSON（适合机器消费 / CI 存档）

```json
{
  "timestamp": "2026-04-28T00:30:00Z",
  "git_sha": "abc1234",
  "results": {
    "cycles_per_log": {"value": 42, "unit": "cycles", "baseline": 38, "delta_pct": 10.5},
    "ram_bytes": {"value": 4224, "unit": "bytes", "baseline": 4096, "delta_pct": 3.1},
    "rom_bytes": {"value": 8192, "unit": "bytes", "baseline": 8192, "delta_pct": 0.0},
    "throughput_fps": {"value": 125000, "unit": "frames/sec", "baseline": 120000, "delta_pct": 4.2}
  }
}
```

#### HTML（可选，含趋势图）

由 `axiom_bench --format html --trend-db bench.db` 生成，使用嵌入式 Chart.js 折线图展示历史趋势。

### 6.3 回归检测与自动告警

```bash
axiom_bench \
    --elf firmware.elf \
    --map firmware.map \
    --backend uart_dma \
    --baseline baseline.json \
    --threshold cycles_per_log=5% \
    --threshold ram_bytes=5% \
    --threshold rom_bytes=2%
```

- 若任一指标超过阈值，工具以非 0 退出码退出，并在 Markdown 报表中插入 `:warning:` 标记。
- CI 中可配合 `continue-on-error: false` 阻断超标合并请求。

**Map 文件解析伪代码：**

```python
def parse_map_ram_rom(map_path: str) -> Dict[str, int]:
    sections = {"text": 0, "rodata": 0, "data": 0, "bss": 0}
    for line in open(map_path):
        if "axiom_" in line:
            size = int(line.split()[1], 16)
            if ".text" in line: sections["text"] += size
            elif ".rodata" in line: sections["rodata"] += size
            elif ".data" in line: sections["data"] += size
            elif ".bss" in line: sections["bss"] += size
    return {
        "rom": sections["text"] + sections["rodata"],
        "ram": sections["data"] + sections["bss"]
    }
```

---

## 7. RTOS / Linux 接入文档框架

所有生态接入文档统一放置在 `docs/rtos/` 与 `docs/linux/` 下，每篇文档遵循以下固定章节模板。

### 7.1 固定章节模板

```markdown
# <Platform> 集成指南

## 概述
- 一句话描述本集成方案的目标与价值。
- 支持的 AxiomTrace 版本范围。

## 前提条件
- 硬件 / 软件环境要求。
- 依赖的 SDK / 库版本。

## 配置步骤
1. 启用后端开关（CMake option 或 Kconfig）。
2. 配置字典 YAML 并运行 Codegen。
3. 链接 AxiomTrace 库到目标项目。
4. 平台特定初始化（如注册后端、设置时钟）。

## 示例代码
- 最小可运行示例（C 代码块）。
- 预期输出（Decoder text 格式）。

## 限制与已知问题
- 当前未支持的特性。
- 平台特定的性能瓶颈或内存约束。

## 参考链接
- 官方文档 / 社区讨论链接。
```

### 7.2 各平台文档概要

#### `docs/rtos/zephyr.md`

- **概述**：将 AxiomTrace 事件映射到 Zephyr Logging 后端，复用 `CONFIG_LOG` 基础设施。
- **关键步骤**：
  1. 在 `prj.conf` 中启用 `CONFIG_AXIOMTRACE=y`。
  2. 实现 `axiom_backend_flush()` 调用 `log_output_flush()`。
  3. 通过 Zephyr Devicetree 指定 UART DMA 通道。
- **示例**：展示在 `main.c` 中使用 `AXIOM_INFO(MOTOR, START, rpm)`，并在 `west build` 后通过 `axiom-decoder` 解析串口输出。

#### `docs/rtos/freertos.md`

- **概述**：FreeRTOS 集成，支持 Trace Hook 与 `printf` 重定向。
- **关键步骤** :
  1. 在 `FreeRTOSConfig.h` 中定义 `traceSTART()` 调用 `axiom_backend_init_all()`。
  2. 提供 `vApplicationIdleHook` 中周期性 `axiom_backend_dispatch()` 的示例。
  3. `printf` 重定向：将 `printf` 宏映射到 `AXIOM_INFO(USER, PRINTF, msg)`（仅限调试构建）。
- **限制**：`printf` 重定向会产生较大 payload，不建议用于生产环境。

#### `docs/linux/journald.md`

- **概述**：systemd journal 字段映射，将 AxiomTrace 解码后的结构化日志写入 `journald`。
- **关键步骤**：
  1. 使用 `axiom-decoder --format jsonl` 管道到 `systemd-cat`。
  2. 字段映射规则：
     - `module` → `AXIOM_MODULE`
     - `event` → `AXIOM_EVENT`
     - `level` → `PRIORITY`（`INFO`→6, `WARN`→4, `ERROR`→3, `FAULT`→2）
  3. 提供 `journalctl -o json` 查询示例。

#### `docs/linux/opentelemetry.md`

- **概述**：OTLP 日志转换，将 AxiomTrace 输出推送到 OpenTelemetry Collector。
- **关键步骤**：
  1. 使用 `axiom-decoder --format jsonl` 作为 OTLP Bridge 的输入。
  2. 将每条 JSON Line 转换为 `opentelemetry.proto.logs.v1.LogRecord`：
     - `timestamp` → `time_unix_nano`
     - `module` + `event` → `SeverityText`
     - `args` 扁平化为 `Attributes`
  3. 提供 Python OTLP Bridge 伪代码及 Collector 配置片段。
- **限制**：OTLP 的批量发送会引入延迟，不适合硬实时场景。

---

## 8. 一键式工作流体验总结

AxiomTrace 工具链的终极目标是：**开发者在主机端执行一条命令，即可获得从固件二进制到人类可读日志的完整链路。**

### 8.1 理想工作流

```bash
# 1. 定义事件（一次性或迭代）
vim events.yaml

# 2. 编译固件（CMake 自动生成固件头文件和标准元数据包）
cmake -B build -S . && cmake --build build

# 3. 烧录并运行固件（常规开发流程）
openocd -f board/stm32f4discovery.cfg -c "program build/firmware.bin reset exit"

# 4. 捕获串口输出到文件
python -m axiom_capture --serial COM3 --output trace.bin

# 5. 一键解码
axiom-decoder trace.bin --bundle build/axiomtrace-bundle --format text

# 6. 一键验证（CI 场景）
axiom-validate --dictionary gen/dictionary.json --golden tests/golden/

# 7. 一键基准测试
axiom_bench --elf build/firmware.elf --map build/firmware.map --baseline baseline.json
```

### 8.2 设计原则

1. **单一真相源**：`events.yaml` 是事件定义的唯一来源，固件、解码器、验证器全部从中派生。
2. **主机端恢复可读性**：固件只传 ID 和原始参数，绝不携带字符串或格式信息，最大限度节省 ROM/RAM。
3. **Bundle 机制**：`axiomtrace-bundle` 统一承载 dictionary、source map、固件身份和 ELF 产物。
4. **Collateral 机制**：`dictionary.json` 仍作为 bundle 内的事件语义核心，供所有主机端工具共享。
5. **管道友好**：Decoder 的 `jsonl` 输出可直接通过 `| jq`、`| systemd-cat`、`| otel-bridge` 进入下游系统。
6. **CI 原生**：Validator 和 Benchmark 均支持机器可读报告（JUnit XML / JSON），天然适配现代 DevOps 流水线。

---

*本文档为 AxiomTrace v0.4 工具链设计的权威参考，后续版本迭代时需同步更新 Schema 与 CLI 接口。*
