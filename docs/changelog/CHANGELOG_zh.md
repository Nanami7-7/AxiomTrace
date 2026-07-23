> [English](CHANGELOG.md) | [简体中文](CHANGELOG_zh.md)

# 更新日志

所有 AxiomTrace 的重要变更都将记录在此文件中。

格式基于 [Keep a Changelog](https://keepachangelog.com/en/1.1.0/)，
本项目遵循 [语义化版本 (Semantic Versioning)](https://semver.org/spec/v2.0.0.html)。

## [未发布]

### 变更
- 顶层 Port 选择收敛为明确的 `host`、`cortex-m` 或 `riscv` 架构路径；依赖 SDK 的厂商适配保持自包含。
- 文档与实际源码树同步，并明确区分核心构建与厂商包。

### 移除
- 未接入构建的根目录 examples、占位 `ports/soc` 选择器和未使用的架构级 CMake 包装文件。

## [1.0.0] - 2026-07-15

### 新增
- 公开诊断计数：过滤、Core ring 压力、前端溢出、非法输入与 Backend 丢失。
- 发布级单头文件：单 implementation TU、可选默认 Port、Memory/Deferred Backend 与多 TU 测试。
- 可安装的 `AxiomTrace::axiomtrace` CMake 包，以及 add-subdirectory/install 消费者夹具。

### 变更
- Core ingress 统一校验公开输入，默认丢弃新帧；显式 OVERWRITE 时只覆盖完整旧帧。
- `axiom_flush()` 先排空并校验 Core ring，再级联 Backend flush callback。
- Deferred 缓存与下游就绪状态解耦，失败帧保留待重试。
- Fault Capsule 使用一个记录感知帧环，无需第二份完整 RAM image 即可流式生成 v1 格式。
- README 与工具链契约明确区分 Host 实测、仅编译 reference Port 与实验性平台。

## [0.7.0] - 2026-05-30

### 修复
- **并发**：修复 `axiom_filter_drop()` 竞态条件 — 在 `axiom_write()` 的两个代码路径（`AXIOM_SHORT_CS=0` 和 `AXIOM_SHORT_CS=1`）中将调用移入临界区，保护 `drop_count` 的 read-modify-write 免受并发 ISR 抢占。
- **正确性**：修复 `axiom_selftest.c` 编码器测试索引偏移 — `test_encoder_roundtrip()` 中的验证索引与实际编码布局不匹配。
- **正确性**：修复 `axiom_selftest.c` 中 `axiom_port_log()` 隐式声明 — 改用已有的 `axiom_port_string_out()`，并包含 `axiom_port.h`。
- **正确性**：修复 `axiom_selftest.c` overflow 测试逻辑 — 旧逻辑用 `AXIOM_MAX_PAYLOAD_LEN` 填满后检查不等式方向错误。
- **并发**：修复 `axiom_timestamp_encode()` 竞态条件 — 将调用移入 `axiom_write()` 的临界区内，防止高优先级 ISR 抢占时破坏 `s_ts_ctx.last_raw`。
- **并发**：修复丢弃摘要报告的竞态条件 — 在临界区内快照 `drop_count`/`drop_module`/`drop_event` 后再清零，防止被抢占的 ISR 破坏数据。
- **正确性**：为 `axiom_filter_t` 中的 `level_mask` 和 `module_mask` 添加 `volatile` 限定符，确保 ISR 上下文中的读取正确。
- **正确性**：修复 `axiom_timestamp_decode_len()` 对 `0xFE` 返回 3 而非 5 的错误 — 导致 delta 超过 21 位时帧解析错位和 CRC 校验失败。
- **正确性**：修复 `axiom_backend_deferred_init()` 初始化了错误的 ring 实例，导致 `ring.mask = 0`，所有 deferred 写入都覆盖 `buf[0]`。
- **正确性**：修复 `AXIOM_MAX_PAYLOAD_LEN` 重复定义 — `axiom_event.h`（64）与 `axiom_config.h`（128）冲突。现在 `axiom_event.h` 包含 `axiom_config.h` 作为唯一定义来源。
- **正确性**：修复 `axiom_enc_timestamp()` 写入双份类型标签（`[0x08][0x05][val...]` 而非 `[0x08][val...]`）。
- **文档**：修复时间戳编码注释中的过时 `0xFF` 引用 — 编码器/解码器使用 `0xFE` 作为 5 字节大 delta 标记。已修正 `axiom_event.h`、`wire_format.md` 和 `wire_format_zh.md`。
- **链接**：为 `axiom_event.c` 中的 `s_filter` 全局变量添加 `static` 限定符，防止意外符号导出。

### 变更
- **重大变更 / Wire v2.0**：普通事件参数改为由 dictionary 定义的 packed 值；源码定位与 metadata identity suffix 仍显式带标签。主机 decoder 继续兼容解析历史 v1.0/v1.1 typed-payload frame。
- **性能**：优化 `axiom_flush()` 和 `deferred_flush()`，使用新增的 `axiom_ring_consume()` 替代冗余的 `axiom_ring_read()`，每帧减少一次 `memcpy`。
- **API**：新增 `axiom_ring_consume()` 环形缓冲区 API — 仅移动 tail 指针，不拷贝数据。
- **文档**：修正环形缓冲区描述，从"无锁"改为"IRQ-safe SPSC，带临界区保护"。
- **文档**：为 `axiom_port_generic.c` 中的默认空实现添加生产环境警告注释。
- **文档**：为 `module_id < 32` 过滤器限制、未使用的 `baseline` 字段以及外部 `drop_summary_ready` / `drop_count_get_and_clear` API 用途添加文档注释。
- **文档**：将 `axiom_backend_deferred.h` 中所有中文注释英文化，为 `AXIOM_BACKEND_CAP_*` 标志添加行内描述。
- **文档**：为 `axiom_backend_register()` 添加线程安全说明 — 必须在任何 `axiom_write()` 调用之前调用。
- **重大变更**：完成 v1.0 架构重构。项目从 v2.0 "文本优先、默认 Flash" 设计转向 v1.0 "以 Event Record 为唯一事实来源" 的微内核模型。
- 将所有 v2.0 占位符实现替换为 v1.0 五平面架构（语义层 / 前端层 / 核心层 / 后端层 / 工具层）。
- 重命名并重组了 `baremetal/` 目录，以匹配 v1.0 平面架构。

### 新增
- **API**：新增 `axiom_type_t` 枚举——命名类型标签（`AXIOM_TYPE_BOOL`、`AXIOM_TYPE_U8` 等），用类型安全的枚举替代原始 `#define` 常量，同时保持源代码级向后兼容。
- **API**：新增 `axiom_backend_err_t` 枚举——命名错误码（`AXIOM_BACKEND_OK`、`AXIOM_BACKEND_ERR_NULL`、`AXIOM_BACKEND_ERR_FULL`、`AXIOM_BACKEND_ERR_STRUCT`），替代整数返回值。
- **API**：新增 `AXIOM_SYNC_BYTE`、`AXIOM_HEADER_LEN`、`AXIOM_CRC_LEN`、`AXIOM_MAX_TIMESTAMP_LEN`、`AXIOM_TAG_SIZE` 命名常量，替换编码器和规范中的硬编码魔数。
- **API**：新增 `axiom_backend_count()` 查询已注册后端数量，与已有的 `axiom_backend_active_count()` 配对使用。
- **构建**：新增 CMake 选项 `AXIOM_DEBUG`（默认 OFF）— 控制所有模块的调试日志输出，全局统一开关。
- **测试**：新增 4 个扩展测试：`test_encoder`（14 组）、`test_ring_ext`（9 组）、`test_event_ext`（7 组）、`test_backend_ext`（8 组），覆盖编码器边界、环形缓冲区满/追加/消耗、事件序列号/帧头格式/过滤器掩码、后端注册/多后端分发/panic 写入等。
- **测试**：扩展 `test_filter.c` 至 9 组，覆盖全部日志级别/模块掩码/丢弃记录/运行时 mask set&get API。
- **测试**：扩展 `test_crc.c` 新增 CRC-16 增量更新与整帧 CRC 一致性验证。
- **API**：新增库版本宏（`AXIOMTRACE_VERSION_MAJOR/MINOR/PATCH`）和编译时 `AXIOMTRACE_VERSION_CHECK()` 宏，支持下游版本检查。
- **API**：新增 `AXIOM_MODULE_MAX` 可配置常量（默认 32）— 替换过滤器逻辑中的硬编码 `32`。
- **API**：新增 `AXIOM_DEPRECATED(msg)` 跨编译器宏（GCC/Clang/MSVC/IAR），用于未来废弃标注。
- **API**：为 `axiom_backend_t` 新增 `size` 字段和 `AXIOM_BACKEND_INIT(...)` 指定初始化器包装宏 — 支持结构体前向兼容演进。旧的零初始化后端保持 ABI 兼容（size==0 视为旧版布局）。`axiom_backend_register()` 对过小的结构体返回 `-3` 拒绝注册。
- `../project/RULES.md`：强制执行开发规则，包括热路径禁令（无 malloc、无 printf、无 Flash 擦除）、丢弃摘要要求，以及强制性的 issue→design→spec→golden→impl→decoder→tests→benchmark→docs→changelog 工作流。
- `../project/PLAN.md` v1.0：冻结目标架构、发布门槛和设计哲学。
- `../project/ROUTE.md` v1.0：从 v0.1-core 到 v0.9-rc 再到 v1.0-stable 的开发阶段。
- `../../spec/api_reference.md`：`AX_LOG`、`AX_EVT`、`AX_PROBE`、`AX_FAULT`、`AX_KV` 的前端 API 参考。
- `../../spec/decoder_protocol.md`：解码器输入/输出协议和字典格式规范。

### 移除
- v2.0 `axiom_storage_t` 统一存储抽象（由 Backend Contract 替代）。
- v2.0 `AXIOM_LOG("fmt", ...)` 类 printf 运行时字符串哈希（由结构化 Event Record 宏替代）。
- v2.0 链接器段自动注册后端系统（由显式 `axiom_backend_register()` 替代）。

## [0.6.0] - 2026-05-27

### 新增
- **API**：新增 `axiom_type_t` 枚举 — 命名类型标签（`AXIOM_TYPE_BOOL`、`AXIOM_TYPE_U8` 等），用类型安全的枚举替代原始 `#define` 常量。
- **API**：新增 `AXIOM_SYNC_BYTE`、`AXIOM_HEADER_LEN`、`AXIOM_CRC_LEN`、`AXIOM_MAX_TIMESTAMP_LEN`、`AXIOM_TAG_SIZE` 命名常量，替换编码器和规范中的硬编码魔数。
- **测试**：新增 4 个扩展测试套件 — `test_encoder`（14 组）、`test_ring_ext`（9 组）、`test_event_ext`（7 组）、`test_backend_ext`（8 组）。
- **测试**：扩展 `test_filter.c` 至 9 组，覆盖全部日志级别、模块掩码、丢弃记录及运行时 mask set&get API。
- **测试**：扩展 `test_crc.c` 新增 CRC-16 增量更新与整帧 CRC 一致性验证。
- **基准测试**：新增 MCU 真实基准套件，含 P99.9/P99.99 百分位和 ARM 指令周期估算。
- **工具**：新增 metadata bundle 系统 — `axiom-bundle generate`、`axiom-codegen`、字典校验器、source map、capsule 报告。
- **工具**：新增文本渲染器（`render.py`），支持字典模板填充。
- **CMake**：新增 `AxiomTraceTools.cmake` 用于 CMake bundle 集成。
- **测试**：新增源码定位测试（`test_location.c`）和 golden frame 生成器（`generate_golden.c`）。

### 变更
- **重大变更 / Wire v2.0**：事件 payload 迁移为 dictionary 定义的 packed 值；metadata identity 和源码定位 suffix 仍显式带标签。
- **工具**：Python CLI 重构为 `axiom-codegen`、`axiom-bundle`、`axiom-decoder` 子命令。
- **工具**：decoder 重构，新增 payload 解析器支持 packed v2 和历史 typed frame。
- **文档**：所有规范文档与最新 API 同步（中英文）。

### 修复
- **并发**：修复 `axiom_filter_drop()` 竞态条件 — 将调用移入 `axiom_write()` 的临界区。
- **并发**：修复丢弃摘要报告竞态条件 — 在临界区内快照后再清零。

## [0.4.0] - 2026-05-07

### 新增
- **API**：新增 `axiom_backend_err_t` 枚举 — 命名错误码替代整数返回值。
- **API**：新增 `axiom_ring_consume()` 环形缓冲区 API — 仅移动 tail 指针，不拷贝数据。
- **API**：新增库版本宏（`AXIOMTRACE_VERSION_MAJOR/MINOR/PATCH`）和编译时 `AXIOMTRACE_VERSION_CHECK()` 宏。
- **API**：新增 `AXIOM_MODULE_MAX` 可配置常量（默认 32）。
- **API**：新增 `AXIOM_DEPRECATED(msg)` 跨编译器宏。
- **API**：为 `axiom_backend_t` 新增 `size` 字段和 `AXIOM_BACKEND_INIT(...)` 支持结构体前向兼容演进。

### 变更
- **性能**：优化 `axiom_flush()`，使用 `axiom_ring_consume()` 替代冗余的 `axiom_ring_read()`。
- **文档**：修正环形缓冲区描述，从"无锁"改为"IRQ-safe SPSC，带临界区保护"。
- **文档**：所有规范文档与最新 API 同步。

### 修复
- **正确性**：修复 `axiom_timestamp_decode_len()` 对 `0xFE` 返回 3 而非 5 的错误。
- **正确性**：修复 `axiom_timestamp_encode()` 竞态条件。
- **正确性**：修复 `axiom_enc_timestamp()` 写入双份类型标签。
- **正确性**：修复 `axiom_backend_deferred_init()` 初始化了错误的 ring 实例。
- **正确性**：修复 `AXIOM_MAX_PAYLOAD_LEN` 重复定义。
- **正确性**：为 `axiom_filter_t` 中的 `level_mask` 和 `module_mask` 添加 `volatile` 限定符。
- **链接**：为 `axiom_event.c` 中的 `s_filter` 全局变量添加 `static` 限定符。

## [0.3.0] - 2026-05-02

### 修复
- **正确性**：修复时间戳编码使用 `0xFF` 作为大 delta 标记 — 改为 `0xFE`。
- **正确性**：为环形缓冲区大小添加 2 的幂次断言。

## [0.2.0] - 2026-05-01

### 新增
- **CI**：新增 GitHub Actions 流水线，集成 cppcheck 和 scan-build 静态分析。
- **CI**：新增 Cortex-M QEMU 交叉编译 CI 目标。
- **文档**：新增 `DIR_STRUCTURE.md` 目录概览文档。
- **文档**：项目文档重组到 `docs/` 子目录。

### 变更
- **许可证**：从 MIT 切换到 GPL-3.0。
- **CI**：精简 CI 为最小化构建-测试流水线以提高可靠性。

### 修复
- **CI**：修复 cppcheck 在三层 port 架构下的包含路径。
- **CI**：修复 Cortex-M QEMU 测试 — 添加 ARM 交叉编译器标志。
- **文档**：修复所有 markdown 文件中的双语链接。
- **文档**：修复 README 许可证描述错误。

## [0.1.0] - 2026-05-01

### 新增
- **核心**：无锁 ISR 安全 RAM 环形缓冲区（`axiom_ring.c`），采用盲写策略。
- **核心**：Event Record 组装（`axiom_event.c`），含帧头、payload_len、payload 和 CRC-16。
- **核心**：二进制编码器（`axiom_encode.h`），支持 `_Generic` 类型分发。
- **核心**：CRC-16/CCITT-FALSE 查表实现（`axiom_crc.c`）。
- **核心**：压缩相对时间戳，支持 delta 编码（`axiom_timestamp.c`）。
- **核心**：级别过滤和丢弃统计（`axiom_filter.c`）。
- **后端**：后端注册表和分发器（`axiom_backend.c`），提供 `axiom_backend_register()` API。
- **后端**：Memory Backend，用于主机测试。
- **后端**：Deferred Backend，基于环形缓冲区的延迟分发。
- **前端**：`AX_LOG`、`AX_EVT`、`AX_PROBE`、`AX_FAULT`、`AX_KV` 宏 API，支持 `DEV/FIELD/PROD` 编译期 Profile 裁剪。
- **移植层**：三层 port 架构 — 通用主机、Cortex-M、RISC-V、ESP32、nRF52、STM32。
- **工具**：Python decoder、golden frame 生成器和合并脚本。
- **测试**：主机单元测试覆盖 ring、encoder、CRC、event、filter、backend、location、profile 和 benchmark。
- **构建**：CMake 构建系统，支持 `AXIOM_PRESET` 资源预设（tiny/prod/field/dev）。
- **规范**：Event model、wire format、backend contract、fault capsule、API reference、decoder protocol、event dictionary、versioning 和 toolchain ecosystem design 规范文档（全部中英双语）。
- **文档**：`PLAN.md`、`ROUTE.md`、`RULES.md` 项目治理文档（全部中英双语）。
- **示例**：`example_minimal.c`（3 行快速开始）和 `example_full.c`（多 API 组合）。

### 变更
- **性能**：D2R（Direct-to-Ring）O(1) 写入路径，增量 CRC — 单临界区完成整帧写入。
