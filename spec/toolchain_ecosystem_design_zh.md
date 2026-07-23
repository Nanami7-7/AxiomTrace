> [English](toolchain_ecosystem_design.md) | [简体中文](toolchain_ecosystem_design_zh.md)

# 工具链契约 — AxiomTrace 1.0

本文只描述已经实现的主机工具。实时串口捕获、独立 benchmark CLI、RTOS 集成包、GUI、OTLP 导出和传输配置不属于 1.0 契约。

## 仅固件路径

普通固件构建只需要 GNU11 GCC/Clang；消费模块化 target 时再使用 CMake。Python、YAML、codegen 与 bundle 都是可选能力。生成的单头文件采用：

```c
#define AXIOMTRACE_IMPLEMENTATION
#include "axiomtrace.h"
```

硬件工程需要自行实现全部 `axiom_port_*` 时，在实现 TU 定义 `AXIOMTRACE_NO_DEFAULT_PORT`，以排除 generic weak defaults。

## 已安装命令

Python 3.12 包 `axiomtrace-tools` 安装四个命令：

| 命令 | 已实现职责 |
| --- | --- |
| `axiom-decoder` | 读取二进制文件、重同步 Wire 帧、校验 framing/CRC，并输出 `raw`、`text`、`json` 或 `jsonl` |
| `axiom-codegen` | 将 JSON 或可选 YAML 事件定义生成 C 头文件、dictionary JSON、source map 与 metadata identity |
| `axiom-bundle` | 生成和检查包含 dictionary、source map、build info 与 identity 的版本化目录 |
| `axiom-validate` | 校验事件定义、dictionary、bundle、golden vector 与 trace |

YAML 支持是可选依赖；JSON 不依赖 YAML 解析器。

## 推荐学习路径

1. 先做结构化解码：`axiom-decoder trace.bin --format raw`。
2. 通过 `--dictionary` 增加手写 JSON 字典，或从有约定的 X-Macro 提取字典。
3. 当生成 ID 与头文件有明确价值时再使用 `axiom-codegen`。
4. 当 trace 必须与构建元数据和源码位置确定性匹配时，再使用 metadata identity 与 `axiom-bundle`。

Wire v2 普通 payload 是 schema 驱动的 packed values。raw 模式只报告帧结构与 payload 字节，不虚构字段语义；语义模式需要匹配 dictionary 或 bundle。保留系统事件使用 event-model 规范中的固定 schema。

## 验证权威

- `tests/golden` 是正常 Wire v2 事件字节兼容性的权威。
- `axiom-validate` 是 dictionary/bundle/trace 一致性的权威。
- CTest 覆盖 C 库、示例、单头文件多 TU 链接和 CMake 消费者。
- Pytest 覆盖 Python decoder 与元数据工具。

Host 时序只作为回归信号。目标 Flash、BSS、栈和时序必须来自选定的交叉编译器与硬件集成；CI 仅编译任务不构成硬件认证。
