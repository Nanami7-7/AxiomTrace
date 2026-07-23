> [English](event_dictionary.md) | [简体中文](event_dictionary_zh.md)

# AxiomTrace 事件字典规范

## 1. 目的

事件字典是从 `(module_id, event_id)` 到人类可读元数据的权威映射。它存在于主机（构建机器/PC）上，绝不会以文本形式烧录到目标设备中。

## 2. 生成的 JSON Schema

`dictionary.json` 是 decoder collateral。`axiom-codegen` 可直接接受此 JSON 形态的事件源；安装可选 `PyYAML` 依赖后也可接受 YAML 事件源。

```json
{
  "version": "1.0",
  "enums": {
    "ENUM_NAME": {
      "0": "LABEL_OK",
      "1": "LABEL_ERR"
    }
  },
  "dictionary": {
    "0x01": {
      "name": "MODULE_NAME",
      "description": "人类可读的模块描述",
      "events": {
        "0x01": {
          "level": "INFO",
          "name": "EVENT_NAME",
          "text": "带 {type} 占位符的人类可读模板",
          "args": [
            {
              "name": "arg0_name",
              "type": "bool|u8|i8|u16|i16|u32|i32|f32|timestamp|bytes",
              "enum": "ENUM_NAME"
            }
          ]
        }
      }
    }
  }
}
```

## 3. 占位符语法

`text` 字段使用 `{name:type}` 占位符。支持的类型：

| 占位符       | C前端类型        | 描述                   |
|--------------|------------------|------------------------|
| `{name:u8}`  | `uint8_t`        | 无符号 8 位            |
| `{name:i8}`  | `int8_t`         | 有符号 8 位            |
| `{name:u16}` | `uint16_t`       | 无符号 16 位           |
| `{name:i16}` | `int16_t`        | 有符号 16 位           |
| `{name:u32}` | `uint32_t`       | 无符号 32 位           |
| `{name:i32}` | `int32_t`        | 有符号 32 位           |
| `{name:f32}` | `float`          | IEEE-754 浮点数        |
| `{name:timestamp}`| `uint32_t`    | 固定宽度时间戳参数     |
| `{name:bytes}`| 字节序列          | 带长度前缀的字节数据   |

对于 wire `v2.0` 中的 `AX_KV` 事件，dictionary `args` 列表必须按精确发送顺序交替声明 `u16` key-hash 字段与其对应的 packed 值字段。

## 4. 枚举映射 (Enum Mapping)

枚举提供了一种在解码期间将原始整数值转换为描述性字符串的方法。

- **定义**: 在 `enums` 键下全局定义。键是枚举名称，值是整数（作为键）到字符串的映射。
- **映射**: 在固件中，枚举作为标准整数类型（`u8`、`u32` 等）传递。
- **解析**: 解码器使用参数定义中的 `enum` 字段来查找适当的映射。如果收到的值在枚举中未定义，解码器应回退到显示原始整数。

## 5. 代码生成 (Code Generation)

`codegen` 工具读取 JSON 事件源，或在已安装 `PyYAML` 时读取 YAML 事件源，并输出：

1. `axiom_events_generated.h` — 将 `MODULE_EVENT_NAME` 映射到 `(module_id, event_id)` 的 C 宏。
2. `axiom_modules_generated.h` — 模块 ID 枚举。
3. `dictionary.json` — 供解码器使用的紧凑型 JSON 文件。

## 6. 校验规则

- 每个 `(module_id, event_id)` 必须唯一。
- `level` 必须与字典条目匹配；固件编码器不强制执行此操作，但校验器会在不匹配时发出警告。
- `args` 的类型序列必须与编码后的有效载荷字段顺序一致。
- `text` 中的占位符必须精确引用声明的 `args`。
- 引用的 `enum` 名称必须存在于 `enums` 部分。
