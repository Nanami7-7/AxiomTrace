> [English](README.md) | **简体中文**

# AxiomTrace

> **为 MCU 裸机环境设计的工业级可观测性微内核**
>
> 零 printf、零 malloc、零动态内存 —— 用确定性的 O(1) 时间，
> 将结构化事件直接写入环形缓冲区，让固件热路径保持纯净与极速。

<!-- 简洁徽章行（后续可补充 CI/License 徽章） -->

---

## 为什么选择 AxiomTrace？

| 痛点 | 传统方案 | AxiomTrace 方案 |
|:---|:---|:---|
| `printf` 格式串占用 Flash | 每个 `%d` / `%s` 占 4~12 字节 | **固件只存 `(module_id, event_id)` 两个整数**，格式化在主机端完成 |
| 中断中调用日志导致栈溢出 | printf 栈帧可达 1KB+ | **D2R（Direct-to-Ring）** 技术，栈使用恒定在 ~50 字节 |
| 日志缓冲区满后丢帧 | 阻塞或无限追加 | **盲覆盖（Blind Overwrite）** O(1) 策略，自动丢弃最旧帧 |
| CRC 校验计算耗时 | 每帧从头算 CRC | **增量 CRC（Incremental CRC）**，边写边算，零额外开销 |
| 环形缓冲区搜索帧边界 | O(N) 扫描 | **位掩码帧边界**，O(1) 定位同步字节 |
| 多后端分发低效 | 逐个拷贝再发送 | **单次写入环形缓冲区 → 批量分发**，零拷贝路由 |

---

## 当前协议与工具链变更

- Wire `v2.0` 将普通事件参数编码为由 dictionary 描述的 packed 值；metadata identity 与可选源码位置仍以带标签的 suffix 表示。保留的 `AX_PROBE` 系统事件因其值类型不受应用事件 schema 约束，继续携带 typed fields；`AX_KV` 的 dictionary 必须按发送顺序声明每组 packed key-hash/value。
- 主机 decoder 继续解析历史 `v1.x` 自描述 typed payload；`v2` 的语义解码必须使用匹配 metadata identity 的 bundle 或 dictionary。
- RISC-V port 使用可嵌套的临界区，并在最外层进入/退出时保存和恢复原始 `mstatus.MIE` 状态。
- JSON 事件定义不依赖额外解析器；使用 YAML 输入时安装可选依赖：`python -m pip install -e "./tool[yaml]"`。

---

## 架构总览

```text
┌──────────────────────────────────────────────────────────────────┐
│                      固件侧（MCU）                                │
│                                                                  │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────────────┐ │
│  │  AX_LOG  │  │  AX_EVT  │  │ AX_PROBE │  │    AX_FAULT      │ │
│  │ 文本日志  │  │ 结构化事件 │  │ 性能探针  │  │ 故障冻结舱        │ │
│  └────┬─────┘  └────┬─────┘  └────┬─────┘  └────────┬─────────┘ │
│       │              │              │                  │           │
│  ┌────┴──────────────┴──────────────┴──────────────────┴────────┐ │
│  │            前端平面 (Frontend Plane)                          │ │
│  │  C11 _Generic 类型安全分发 / 去重 / PROD 裁剪                 │ │
│  └──────────────────────────┬───────────────────────────────────┘ │
│                             │                                     │
│  ┌──────────────────────────┴───────────────────────────────────┐ │
│  │            核心平面 (Core Plane)                              │ │
│  │  ┌─────────┐  ┌──────────┐  ┌────────┐  ┌──────────────┐   │ │
│  │  │ D2R 编码 │→│ 增量 CRC │→│ 模块滤波│→│ 位掩码环形缓冲 │   │ │
│  │  └─────────┘  └──────────┘  └────────┘  └──────────────┘   │ │
│  └──────────────────────────┬───────────────────────────────────┘ │
│                             │                                     │
│  ┌──────────────────────────┴───────────────────────────────────┐ │
│  │            后端平面 (Backend Plane)                           │ │
│  │  UART │ USB │ RTT │ SWO │ CAN-FD │ Flash Capsule │ 自定义    │ │
│  └─────────────────────────────────────────────────────────────┘ │
└──────────────────────────────────────────────────────────────────┘
                              │
                              │  二进制流
                              ▼
┌──────────────────────────────────────────────────────────────────┐
│                      主机侧（PC）                                 │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐              │
│  │   元数据包   │  │ Python 解码器 │  │ JSON/Text 输出 │             │
│  └─────────────┘  └─────────────┘  └─────────────┘              │
└──────────────────────────────────────────────────────────────────┘
```

---

## 快速开始

### 最小示例（3 行代码）

```c
#include "axiomtrace.h"

int main(void) {
    axiom_init();  /* 初始化环形缓冲区、时间戳、过滤器 */

    /* 结构化事件：O(1)、中断安全、零 printf */
    AX_EVT(INFO, MOTOR, START, (uint16_t)3200);
    AX_LOG(INFO, MOTOR, "Motor started at %d RPM", 3200);
    AX_PROBE("adc_sample", (uint16_t)0x1A3F);

    return 0;
}
```

### 编译与运行

```bash
# 克隆仓库
git clone https://github.com/loopgad/AxiomTrace.git
cd AxiomTrace

# 配置并编译（启用测试）
cmake -B build -S . -DAXIOM_BUILD_TESTS=ON
cmake --build build

# 运行全套单元测试与 MCU 可靠性基准压测
ctest --test-dir build --output-on-failure
```

`test_benchmark` 已注册为 CTest 用例；它提供宿主环境中的回归门槛，不替代目标 MCU 上的时序测量与认证验证。


### Python 工具链

```bash
# 安装解码器
python -m pip install -e ./tool

# 使用本机 CMake/Ninja 工具链构建和测试
cmake -B build -S . -G Ninja -DAXIOM_BUILD_TESTS=ON -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build
ctest --test-dir build --output-on-failure

# 使用标准元数据包解码
axiom-codegen --events events.yaml --out build/generated
axiom-bundle generate --events events.yaml --compile-db build/compile_commands.json --out build/axiomtrace-bundle
axiom-decoder trace.bin --bundle build/axiomtrace-bundle --format text
```

---

## 事件格式

### 帧结构（有线格式）

每一帧在环形缓冲区中的二进制布局：

```text
┌─────────┬─────────┬─────────┬────────────┬─────────────┬──────────┬────────────┬────────┐
│  Sync   │ Version │  Level  │ Module_ID  │  Event_ID   │   Seq    │  Timestamp │   CRC  │
│  (1B)   │  (1B)   │  (1B)   │   (1B)     │   (2B)      │  (2B)    │  (1~5B)    │  (2B)  │
│  0xA5   │  0x01   │  enum   │  uint8     │  uint16 LE  │ uint16LE │ 变长编码    │ LE CRC │
└─────────┴─────────┴─────────┴────────────┴─────────────┴──────────┴────────────┴────────┘
                                                                       │
                                                            ┌──────────┴──────────┐
                                                            │ Payload (0~255B)    │
                                                            │ [v2 Packed Values]  │
                                                            │ [arg0][arg1]...     │
                                                            │ [Metadata Tags]*    │
                                                            └─────────────────────┘
```

> **注意**：Wire `v2.0` 的普通参数按照 Event Dictionary 中的类型与顺序紧凑编码，不携带逐字段 type tag。该变化不兼容于 `v1.x`，因此使用新的 wire major version。`v2` 语义解析需要匹配的 bundle/dictionary；decoder 仍支持读取历史 `v1.x` typed payload。

### 元数据扩展标签（位于 Payload 末尾）

| 标签值 | 类型 | 大小 | 说明 |
|:---:|:---|:---:|:---|
| `0x0A` | location metadata | 5B / 7B | 包含源文件行号与 ID/Hash 等定位材料 |
| `0x0B` | metadata identity | 8B | 元数据身份事件（唯一关联主机端字典） |

### 编码示例

```c
/* 发送一条 INFO 级别的电机启动事件 */
/* 根据 events.yaml 定义：第一个参数为 uint16_t (2B)，第二个参数为 bool (1B) */
uint8_t payload[AXIOM_MAX_PAYLOAD_LEN];
uint8_t pos = 0;

axiom_enc_u16(payload, &pos, 3200);  /* RPM */
axiom_enc_bool(payload, &pos, true); /* 启用状态 */

AX_EVT_RAW(INFO, MOTOR, MOTOR_START, payload, pos);
/* v2 Payload 为 3 字节：[0x80, 0x0C] (3200 LE) + [0x01] (true)。 */
```

---

## 核心 API 参考

### 初始化

```c
void axiom_init(void);
```
初始化环形缓冲区（4KB RAM）、时间戳上下文、模块过滤器。调用一次即可。

### 事件写入

```c
/* 原始载荷写入 */
void axiom_write(axiom_level_t level, uint8_t module_id,
                 uint16_t event_id, const uint8_t *payload,
                 uint8_t payload_len);
```
将事件直接写入环形缓冲区。ISR 安全，无 malloc，无 printf。

### 前端宏（类型安全）

```c
AX_LOG(level, module, fmt, ...)      /* 文本日志（编译期格式检查） */
AX_EVT(level, module, event, ...)    /* 结构化事件（C11 _Generic） */
AX_PROBE(tag, value)                 /* 性能探针（编译期可裁剪） */
AX_FAULT(module, event, ...)         /* 故障冻结（触发 Fault Capsule） */
```

### 后端注册

```c
/* 注册一个后端 */
int axiom_backend_register(const axiom_backend_t *backend);

/* 刷新所有后端 */
void axiom_backend_flush_all(void);
```

### 滤波控制

```c
/* 运行时滤波控制 */
void axiom_level_mask_set(uint32_t mask);
uint32_t axiom_level_mask_get(void);
void axiom_module_mask_set(uint32_t mask);
uint32_t axiom_module_mask_get(void);
```

### 自检

```c
bool axiom_selftest(void);
/* 验证 CRC、环形缓冲区、编码器，返回 true 表示全部通过 */
```

---

## 设计哲学

### 1. 协议即本体（Protocol as Ontology）

> 文本/JSON/二进制只是视图；事件记录（Event Record）是唯一的真理。

固件只存储原始 `(module_id, event_id)` 和整数参数。人类可读的名称、文本模板、枚举映射全部由主机端字典提供。这意味着：

- 固件二进制体积最小化
- 格式串不会泄露到 ROM
- 主机端可以随时切换语言/格式

### 2. D2R 直接入环（Direct-to-Ring）

传统记录器会在栈上组装完整帧，然后一次性拷贝到环形缓冲区。AxiomTrace 采用 **直接入环** 策略：每个字段（header、timestamp、payload、CRC）逐一写入环形缓冲区，通过增量 CRC 避免任何栈上缓冲区。

```
传统方案：栈帧 = AXIOM_MAX_FRAME_LEN + CRC + header ≈ 300B
AxiomTrace：栈帧 ≈ 50B（仅增量 CRC 和临时变量）
```

### 3. 盲覆盖（Blind Overwrite）

环形缓冲区满时，O(1) 直接推进尾指针丢弃最旧帧，不进行任何帧边界搜索或内存拷贝。这确保了即使在最坏情况下，写入延迟也是恒定的。

### 4. 双轨时间同步

```c
/* 相对时间戳：高精度、单调递增，用于时序分析 */
/* Unix 时间戳：主机端注入，用于真实世界时间对齐 */
```

固件侧维护一个 32 位相对计数器（基于 `axiom_port_timestamp()`），主机端通过周期性注入的 SYNC 事件将其映射到真实时间。两个轨道互不干扰，分别服务于不同的分析场景。

### 5. PROD 裁剪（零成本抽象）

```c
#if AXIOM_PROFILE == AXIOM_PROFILE_PROD
    #define AX_PROBE(tag, value)
    /* 编译期移除所有探针，零运行时开销 */
#endif
```

在生产固件中，`AX_PROBE` 宏被编译为空操作，不产生任何代码。

---

## 配置项速查

所有配置通过 `baremetal/axiom_config.h` 或编译器 `-D` 宏覆盖：

| 宏定义 | 默认值 | 说明 |
|:---|:---:|:---|
| `AXIOM_RING_BUFFER_SIZE` | `4096` | 环形缓冲区大小（字节，必须是 2 的幂） |
| `AXIOM_RING_BUFFER_POLICY` | `DROP` | 满时策略：`DROP` 丢新帧 / `OVERWRITE` 覆旧帧 |
| `AXIOM_MAX_PAYLOAD_LEN` | `128` | 单事件最大载荷（字节） |
| `AXIOM_MODULE_MAX` | `32` | 最大模块数（0~31），控制滤波器位掩码宽度 |
| `AXIOM_CFG_LOCATION_MODE` | `NONE` | 定位模式：`NONE` / `HASH` / `FILE_ID` |
| `AXIOM_CFG_LOCATION_FUNCTION` | `0` | `HASH` 模式下附加 `__func__` hash |
| `AXIOM_SOURCE_FILE_ID` | `0` | `FILE_ID` 模式下由构建系统按源文件注入的 ID |
| `AXIOM_CFG_USE_LOCATION` | `0` | 兼容开关；旧集成启用时映射到 `HASH` 模式 |
| `AXIOM_CFG_TIME_SYNC_ENABLED` | `1` | 启用双轨时间同步 |
| `AXIOM_ENCODE_OVERFLOW_DETECTION` | `1` | 编码器溢出保护 |
| `AXIOM_SHORT_CS` | `1` | 短临界区模式（牺牲 ~255B 栈，换取更低中断延迟） |
| `AXIOM_SELFTEST` | `0` | 启用运行时自检 |
| `AXIOM_BACKEND_DEGRADATION` | `1` | 后端自动降级与恢复 |
| `AXIOM_BACKEND_MAX` | `4` | 最大后端数量 |
| `AXIOM_BACKEND_FAIL_THRESHOLD` | `5` | 连续失败多少次后降级 |
| `AXIOM_BACKEND_RECOVERY_US` | `1000000` | 降级恢复时间（微秒） |

---

## 主机端元数据包

AxiomTrace 推荐将每个固件版本的主机端解析材料打包为统一的 `axiomtrace-bundle`，而不是让每个工程维护私有解析配置。

```text
axiomtrace-bundle/
  manifest.json
  dictionary.json
  source_map.json
  build_info.json
  firmware.elf
  firmware.map
```

固件侧继续只输出轻量二进制字段；主机端通过 bundle 还原事件名称、参数名、源码文件、行号和故障地址。完整规范归入 [工具链生态设计](spec/toolchain_ecosystem_design_zh.md#3-元数据包标准)。

使用 bundle 进行语义解码时，trace 必须通过生成头文件 `axiom_metadata_id_generated.h` 中的 `AXIOM_EMIT_METADATA_ID()` 至少发出一次元数据身份事件。

---

## 极限内存

| 指标 | 数值 | 说明 |
|:---|:---:|:---|
| Flash 占用 | < 2 KB | 核心逻辑 + CRC 查表 + 编码器 |
| RAM 占用 | 4.2 KB | 环形缓冲区 4KB + 滤波器 + 时间戳上下文 |
| 栈使用 | ~50 B | D2R 直接入环，无栈上缓冲区 |
| 最大帧长 | 12B + N | 8 header + 1 ts + 1 len + N payload + 2 CRC |

---

## 工程铁律（Hot Path Iron Rules）

固件热路径（`axiom_write()` 及所有 ISR 调用路径）**严格禁止**：

| 禁止项 | 原因 |
|:---|:---|
| `malloc` / `free` | 非确定性堆分配 |
| `printf` / `sprintf` / `vsnprintf` | 代码体积大、栈深、非确定性 |
| Flash `erase` / `write` | 耗时数百毫秒，阻塞系统 |
| 等待后端就绪 | 后端必须在忙时返回 `-EAGAIN` |
| 循环中大量数据拷贝 | 必须使用固定帧长，单次临界区 |
| 浮点运算（热路径） | 无 FPU 的 MCU 上软件浮点极慢 |
| 字符串比较/搜索 | 运行时字符串解析禁止 |

**允许**：D2R 编码、盲覆盖 O(1) 策略、增量 CRC、`_Generic` 类型分发、单次临界区写入。

---

## 故障冻结舱（Fault Capsule）

当发生 `AX_FAULT` 事件时，系统自动冻结故障前/后窗口并提交至非易失性存储：

```c
AX_FAULT(MOTOR, OVERCURRENT, (uint16_t)current_ma);
/* 自动触发：
 *   1. 冻结故障前 N 帧历史记录
 *   2. 冻结故障后 M 帧快照
 *   3. 打包为 Fault Capsule 写入 Flash
 *   4. 主机端可通过 axiom-decoder 解码 Capsule 文件
 */
```

---

## 移植到新平台

只需实现 `baremetal/port/axiom_port.h` 中的弱函数：

```c
/* 必须实现（除非覆盖默认弱实现） */
uint32_t axiom_port_timestamp(void);   /* 返回微秒级时间戳 */
void     axiom_port_critical_enter(void);
void     axiom_port_critical_exit(void);

/* 可选实现 */
void     axiom_port_fault_hook(uint8_t module, uint16_t event,
                                const uint8_t *payload, uint8_t len);
int      axiom_port_panic_write(const uint8_t *frame, uint16_t len);
```

已有平台移植：
- ✅ **STM32 HAL** — 使用 `DWT->CYCCNT` 或 `HAL_GetTick()`
- ✅ **ESP-IDF** — 使用 `esp_timer_get_time()`
- ✅ **POSIX 主机** — 使用 `clock_gettime()`（用于测试）

---

## 项目结构

```text
AxiomTrace/
├── baremetal/
│   ├── core/                  核心实现
│   │   ├── axiom_event.c      事件写入 + 临界区管理
│   │   ├── axiom_encode.h     类型安全编码器（inline）
│   │   ├── axiom_crc.h        CRC-16/CCITT 查表 + 增量
│   │   ├── axiom_ring.h       位掩码环形缓冲区
│   │   ├── axiom_filter.h     模块/级别滤波器
│   │   ├── axiom_timestamp.h  双轨时间戳
│   │   └── axiom_selftest.h   运行时自检
│   ├── backend/               后端抽象层
│   │   ├── axiom_backend.h    后端接口 + 降级恢复
│   │   └── axiom_backend.c    注册/分发/降级逻辑
│   ├── frontend/              前端宏
│   │   └── axiom_frontend.h   AX_LOG / AX_EVT / AX_PROBE
│   ├── port/                  平台移植层
│   │   └── axiom_port.h       弱函数接口
│   ├── ports/                 已有移植
│   │   ├── generic/           POSIX 通用平台
│   │   ├── stm32/             STM32 HAL
│   │   └── esp32/             ESP-IDF
│   ├── include/               公共头文件
│   ├── examples/              使用示例
│   └── axiom_config.h         全局配置
├── tool/                      Python 工具链
│   ├── decoder/               二进制解码器
│   ├── golden/                黄金测试向量
│   └── benchmark/             性能基准测试
├── tests/                     集成测试
├── docs/                      扩展文档
├── spec/                      规范文档
└── examples/                  端到端示例
```

---

## 规范文档

| 文档 | 说明 |
|:---|:---|
| [API 参考](spec/api_reference_zh.md) | 前端宏与核心控制 API 完整说明 |
| [有线格式](spec/wire_format_zh.md) | 二进制序列化与帧结构详细规范 |
| [事件模型](spec/event_model_zh.md) | 报头布局、时间戳与 D2R 机制深度解析 |
| [字典规范](spec/event_dictionary_zh.md) | YAML Schema 与枚举映射方案 |
| [工具链生态设计](spec/toolchain_ecosystem_design_zh.md) | 解码器、元数据包、codegen、验证器与主机端工作流规范 |
| [故障胶囊](spec/fault_capsule_zh.md) | 故障冻结、提交与非易失性存储规范 |
| [极简架构分析](spec/minimalist_architecture_analysis_zh.md) | 架构演进思想与精简论证 |
| [静态分析集成](spec/static_analysis_zh.md) | Cppcheck / clang-tidy / MISRA 集成 |
| [目录结构](docs/reference/DIR_STRUCTURE.md) | 完整文件树与平面标注 |
| [工程规则](docs/project/RULES.md) | 工程标准与热路径铁律 |
| [移植指南](docs/reference/porting_guide.md) | 如何将 AxiomTrace 移植到新 MCU 平台 |

---

## 贡献指南

AxiomTrace 遵循严格的工程标准。贡献代码前请阅读 [RULES.md](docs/project/RULES.md)。

### 提交规范

```bash
# 功能开发
git commit -m "feat(module): description"

# Bug 修复
git commit -m "fix(module): description"

# 文档更新
git commit -m "docs: description"
```

### 代码质量要求

- 所有新代码必须通过 `clang-format` 和 `clang-tidy` 检查
- 热路径代码禁止 `malloc`、`printf`、浮点运算
- 新后端必须实现 `AXIOM_BACKEND_INIT(...)` 宏
- 所有公共 API 必须包含 Doxygen 注释

---

## 许可证

[GNU General Public License v3.0](LICENSE)

---

> **AxiomTrace** — 让 MCU 的可观测性，从第一天起就是工业级的。
