> [English](CHANGELOG.md) | [简体中文](CHANGELOG_zh.md)

# 更新日志

所有 AxiomTrace 的重要变更都将记录在此文件中。

格式基于 [Keep a Changelog](https://keepachangelog.com/en/1.1.0/)，
本项目遵循 [语义化版本 (Semantic Versioning)](https://semver.org/spec/v2.0.0.html)。

## [未发布]

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

## [0.1.0] - 待定

### 新增
- 核心层：IRQ-safe SPSC RAM Ring 缓冲区（临界区保护），支持编译期配置大小和策略（丢弃/覆盖）。
- 核心层：二进制 Event Record 编码器，支持 C11 `_Generic` 类型安全 payload 编码和自描述类型标签。
- 核心层：CRC-16/CCITT-FALSE，带 256 字节 ROM 查表。
- 核心层：压缩相对时间戳（delta 编码）。
- 核心层：运行时级别/模块过滤和丢弃统计，支持生成 DROP_SUMMARY。
- 前端层：支持 DEV/FIELD/PROD Profile 编译期裁剪的 `AX_LOG`、`AX_EVT`、`AX_PROBE`、`AX_FAULT`、`AX_KV` 宏。
- 后端层：统一的 `axiom_backend_t` 合约，包含 `write/ready/flush/panic_write/on_drop`。
- 后端层：Memory、UART、USB CDC、RTT、SWO/ITM、CAN-FD 后端模板。
- Fault Capsule：故障前窗口冻结、故障后窗口捕获、寄存器快照、固件哈希、Capsule CRC、Flash commit。
- 工具层：Python 解码器、文本渲染器、JSON 导出器、故障报告器、Golden 帧管理器、基准测试工具。
- 主机单元测试和 Python 回归测试。
- 单文件库生成器 (`../../tool/scripts/amalgamate.py`)。
