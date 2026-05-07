> [English](README.md) | [简体中文](README_zh.md)

# AxiomTrace v0.1 — 工业级 MCU 可观测微内核

AxiomTrace 是一个专为 MCU 裸机环境设计的高性能、确定性可观测性核心。它通过将复杂性推向主机端，并保持固件热路径纯净、快速且具备 O(1) 特性，彻底改变了嵌入式日志的处理方式。

---

## 🚀 快速开始

```c
#include "axiomtrace.h"

int main(void) {
    axiom_init();
    /* 结构化事件：O(1)、中断安全、零 malloc、零 printf */
    AX_EVT(INFO, MOTOR, START, (uint16_t)3200); 
    return 0;
}
```

## 💎 工业级四大支柱

### ⚡ 确定性的 O(1) 性能
AxiomTrace 保证每个日志调用的执行时间都是恒定的。通过采用 **“盲覆盖 (Blind Overwrite)”** 策略和位掩码环形缓冲区，它避免了中断期间昂贵的帧边界搜索。

### 🧬 直接入环 (D2R) 技术
与在栈上组帧的传统记录器不同，AxiomTrace 将数据 **直接写入环形缓冲区段**。配合 **增量 CRC (Incremental CRC)**，它消除了冗余的内存拷贝和巨大的栈空间占用。

### 🌐 双轨时间同步
支持高分辨率相对计数器，用于精确的时序分析；同时允许周期性注入 Unix 时间戳，以便在主机端实现真实世界的“墙上时间”对齐。

### 🎨 丰富的宿主侧语义
通过仅存储原始 ID 和整数，保持固件二进制文件的精简。利用 **主机字典 (Host Dictionary)** 将 ID 映射回人类可读的文本、枚举、物理单位和丰富的元数据。

---

## 🛠️ 核心特性

- **协议即本体架构**：文本/JSON/二进制只是视图；事件记录 (Event Record) 是唯一的真理。
- **可插拔后端**：UART、USB、RTT、SWO 或 Flash 舱 (Capsule) — 无需触碰核心即可添加新后端。使用 `AXIOM_BACKEND_INIT(...)` 实现结构体前向兼容初始化。
- **故障舱 (Fault Capsule)**：在发生严重故障期间自动冻结故障前/后窗口，并提交至非易失性存储。
- **基于 Profile 的裁剪**：`PROD` 配置会在编译期自动移除调试探针和日志，实现零成本运行。
- **库版本管理**：通过 `AXIOMTRACE_VERSION_CHECK(major, minor, patch)` 实现编译时版本检查。
- **可配置模块上限**：`AXIOM_MODULE_MAX`（默认 32）控制模块过滤器位掩码宽度。
- **双语文档**：英文与简体中文之间的无缝切换，助力全球化协作。

---

## 🏗️ 架构

```text
AxiomTrace/
  前端平面 (Frontend)   AX_LOG / AX_EVT / AX_PROBE / AX_FAULT / AX_KV
  核心平面 (Core)       直接入环 (D2R) → 增量 CRC → 过滤器 → 位掩码环
  后端平面 (Backend)    UART / RTT / USB / SWO / Flash 舱 / CAN-FD
  工具平面 (Tool)       Python 解码器 / 文本渲染 / JSON 导出 / Golden 测试
```

---

## 📦 开始使用

### 1. 构建与测试

```bash
cmake -B build -S . -DAXIOM_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

### 2. Python 工具链

```bash
# 安装解码器
pip install ./tool

# 解码二进制流
axiom-decoder trace.bin -d events.yaml -o text
```

---

## 📚 文档索引

| 规范文档 | 描述 |
| :--- | :--- |
| [目录结构](docs/reference/DIR_STRUCTURE.md) | 完整文件树与各平面标注 |
| [API 参考手册](spec/api_reference.md) | 前端宏与核心控制 API |
| [有线格式规范](spec/wire_format.md) | 二进制序列化与成帧 (COBS) |
| [事件模型规范](spec/event_model.md) | 报头布局、时间戳与 D2R 机制 |
| [字典规范](spec/event_dictionary.md) | YAML Schema 与枚举映射 |
| [工程规则与准则](docs/project/RULES.md) | 工程标准与热路径铁律 |
| [故障胶囊规范](spec/fault_capsule.md) | 故障冻结、提交与非易失性存储 |
| [移植指南](docs/reference/porting_guide.md) | 如何将 AxiomTrace 移植到新 MCU 平台 |

---

## 许可证

[GNU General Public License v3.0](LICENSE)
