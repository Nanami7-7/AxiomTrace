> [English](RULES.md) | [简体中文](RULES_zh.md)

# AxiomTrace RULES.md

> 版本：v1.0 | 状态：**强制执行** | 更新日期：2026-04-29

---

## 1. 项目宗旨（不可动摇）

AxiomTrace 是一个 **MCU 裸机可观测微内核**。我们的坚定目标：

- **好用**：开发者能在 5 分钟内打出第一条结构化日志，API 语义清晰、命名直观、文档完善。
- **轻量高效**：热路径 O(1)、无 malloc、无 printf、无阻塞、ISR 可写；最小配置 Flash < 4KB、RAM < 2KB。
- **扩展性**：Backend 可插拔、Payload 类型可扩展、Profile 可裁剪、协议向前兼容；新增后端不修改 Core。
- **用户友好**：零配置起步、单文件库快速入门、渐进式 API（从硬编码 ID 到 X-Macro 到 YAML）、错误信息明确、示例覆盖全部场景。
- **可信可发布**：协议可冻结、工具链可验证、golden test 可回归、故障可追溯。

**不是**：
- 不是 printf 替代品。
- 不是 RTOS/Linux 日志库。
- 不是全量文件系统。
- 不是芯片 HAL 封装库。

---

## 2. 热路径铁律（禁止项）

以下行为在 `axiom_write()` 及所有 ISR 调用路径中 **绝对禁止**：

| 禁止项 | 理由 | 违反后果 |
| :--- | :--- | :--- |
| `malloc` / `free` | 堆分配非确定性、可能阻塞、碎片。 | 拒绝合入。 |
| `printf` / `sprintf` / `vsnprintf` | 代码体积大、栈消耗大、非确定性。 | 拒绝合入。 |
| `snprintf` 格式化 | 同上。 | 拒绝合入。 |
| Flash `erase` | 耗时数百毫秒，阻塞系统。 | 拒绝合入；仅 Capsule commit 允许在非热路径调用。 |
| Flash `write` | 同上；仅 Capsule commit 允许。 | 拒绝合入。 |
| 阻塞等待 backend ready | backend 忙时应返回 `-EAGAIN`，由 Core 更新 drop counter。 | 拒绝合入。 |
| 循环拷贝大数据 | 必须固定帧长，单次临界区完成。 | 拒绝合入。 |
| 浮点运算（热路径） | 部分 MCU 无 FPU，软件浮点极慢。 | 允许 `_Generic` 接收 `float`，但编码时用 `memcpy` 按位拷贝。 |
| 字符串比较/查找 | 禁止运行时解析字符串。 | 拒绝合入。 |

**允许项**：
- Direct-to-Ring (D2R) 编码（直接流式写入环形缓冲区分段）。
- 盲覆盖（Blind Overwrite）策略，确保环满时具备 O(1) 确定性延迟。
- 增量式 CRC（流式计算，无需重复读取内存）。
- 写 RAM Ring（单次临界区）。
- 更新全局 drop counter (`volatile uint32_t` 自增)。
- 轻量 timestamp（32-bit 计数器读取或 delta 编码）。
- CRC-16 查表（256B ROM 表，单次循环）。
- `_Generic` 类型分发（编译期解析，零运行时开销）。

---

## 3. 存储规则

| 规则 | 说明 |
| :--- | :--- |
| 默认 RAM Ring | 所有事件先进入 RAM Ring，这是唯一热路径存储。 |
| Flash 默认关闭 | 不允许默认开启 Flash 写入。 |
| Flash 仅用于 Fault Capsule | 正常运行时 **绝不** 写 Flash；仅在故障触发后，由用户代码在非 ISR 上下文调用 `axiom_capsule_commit()` 时写 Flash。 |
| 显式持久化 | 若用户需要一般事件持久化到 Flash，必须通过独立 backend 模板实现，且该 backend 必须实现限速与磨损策略，不进入热路径。 |
| 掉电恢复必须测试 | 任何涉及 Flash/FRAM/NVRAM 的 backend 必须提供掉电恢复测试用例。 |
| 磨损策略必须说明 | Flash backend 必须文档说明擦写次数预估、磨损均衡策略、寿命预期。 |

---

## 4. 协议规则

| 规则 | 说明 |
| :--- | :--- |
| Event Record 是唯一日志本体 | 固件中不存在"文本日志"或"二进制日志"的区分，只有 Event Record。 |
| Backend 不得定义私有协议 | 所有 backend 接收的 buffer 必须是统一的 Event Record frame；backend 只能做传输适配（如 COBS 编码、CAN-FD 分帧），不能改变帧内结构。 |
| Text 只是渲染 | 文本输出由主机 decoder 根据 dictionary 模板渲染生成。 |
| JSON 只是导出 | JSON 由主机工具从 binary 转换生成。 |
| Binary 是存储/传输形态 | Binary frame 是唯一穿越固件-主机边界的数据形态。 |
| Payload 自描述 | 每个 payload 字段必须带类型标签（`0x01=u8, 0x02=i8...`），decoder 无需外部 schema 即可解析结构；dictionary 只负责语义映射（ID → 名称/模板）。 |
| 协议向前兼容 | Minor version 增加只追加新 type tag 或新 flag，不修改已有字段语义；Major version 变化必须同步更新 decoder、golden、spec、docs。 |

---

## 5. 丢弃规则

| 规则 | 说明 |
| :--- | :--- |
| 允许丢 DEBUG | DEBUG 级别事件在资源紧张时可丢弃。 |
| 允许限速 | 全局或 per-backend 超速时丢弃。 |
| 允许 backend busy | backend `ready()` 返回 false 或 `write()` 返回 `-EAGAIN` 时丢弃。 |
| **禁止静默丢弃** | 任何丢弃行为必须通过 `on_drop` callback 通知 backend，且 Core 必须累计 drop counter。 |
| **必须生成 DROP_SUMMARY** | 当从丢弃状态恢复为可写状态时，Core 必须自动生成一条 `AXIOM_LEVEL_WARN` 级别的 `DROP_SUMMARY` 事件，包含 `lost_count`、`last_module_id`、`last_event_id`。 |
| 丢弃统计持久化 | drop counter 在 Capsule commit 时必须随 capsule 一起保存，供事后分析。 |

---

## 6. 扩展性规则

| 规则 | 说明 |
| :--- | :--- |
| Backend 零侵入扩展 | 新增 backend 只需实现 `axiom_backend_t` 接口并调用 `axiom_backend_register()`；不允许修改 `core/` 或 `frontend/` 任何文件。 |
| Payload 类型可扩展 | `0x0A` 与 `0x0B` 已保留给定位和元数据身份；后续标准 type tag 使用剩余 `0x0C~0x7F` 区间。必须同步更新 encoder、decoder、spec、golden tests、docs；旧 decoder 遇到未知 type tag 应跳过并标记 `UNKNOWN_TYPE` 而非崩溃。 |
| Profile 可裁剪 | `DEV / FIELD / PROD` Profile 通过宏控制编译期裁剪；新增 Profile 需在 `frontend/` 中增加对应宏分支，不得改变已有 Profile 语义。 |
| Port 层弱符号 | 所有 port 函数提供 `__attribute__((weak))` 默认实现；新增 port（如新 MCU 系列）只需覆盖所需函数，无需修改库代码。 |
| 工具链可扩展 | decoder 采用插件化字典加载（支持 JSON/YAML/X-Macro 提取）；新增导出格式（如 CSV、PCAP）通过新增 render 模块实现。 |
| API 稳定性 | `AX_*` 宏与 `axiom_backend_t` 结构体从 v1.0 起 ABI 锁定；内部 `_axiom_*` 函数允许在 minor 版本变更。 |

---

## 7. 用户友好规则

| 规则 | 说明 |
| :--- | :--- |
| 5 分钟起步 | 新用户复制 `axiomtrace.h` 单文件到项目，写 3 行代码即可编译通过并输出第一条日志。 |
| API 命名直观 | 宏命名遵循 `AX_<LEVEL>(MOD, EVT, ...)` 或 `AX_LOG(msg)` 模式；禁止晦涩缩写。 |
| 渐进式复杂度 | Layer 0（硬编码 ID）→ Layer 1（X-Macro 定义）→ Layer 2（YAML + codegen）；用户按需升级，不被迫接受复杂工具链。 |
| 错误信息明确 | 编译期配置错误必须通过 `#error "AxiomTrace: xxx"` 给出清晰英文提示；运行时错误（如 ring 满）通过 `on_drop` callback 暴露，禁止静默失败。 |
| 示例全覆盖 | 每个 Frontend API、每个 Backend Template、每个 Profile 必须有独立可编译的 example。 |
| 文档同步 | 任何 API 变更必须同步更新 `../../spec/api_reference.md`；任何协议变更必须同步更新 `../../spec/wire_format.md` 和 decoder。 |
| 单文件库自动化 | 提供 `../../tool/scripts/amalgamate.py`，一键将多文件库合并为单文件 `axiomtrace.h`；合并产物必须通过全部 host tests。 |

### 文档治理

| 规则 | 说明 |
| :--- | :--- |
| 每个主题只有一个主文档 | API 归 `spec/api_reference.md`，wire 数据归 `spec/wire_format.md`，事件语义归 `spec/event_model.md`，工具链和 bundle 归 `spec/toolchain_ecosystem_design.md`，项目流程归 `docs/project/*.md`。 |
| 禁止临时 Markdown | 除非主题无法并入现有主文档，并且新文件已从 README 和 `docs/reference/DIR_STRUCTURE.md` 建立入口，否则不得新增 `.md` 文件。 |
| README 只做入口 | README 负责定位、快速开始和文档索引，不重复 schema、长 CLI 契约或实现计划。 |
| 中英文同步 | 已有中英文版本的公开文档必须在同一次变更中同步更新。 |
| 工具细节优先放 CLI help | CLI 参数细节优先写在工具 help/docstring 中；Markdown 描述稳定契约和工作流，不重复堆参数。 |
| 大范围工作先更新路线 | 大型工具链或文档规范化工作，必须先更新 `PLAN.md` / `ROUTE.md`，让范围在实现前可见。 |

---

## 8. 开发流程（强制执行）

任何功能新增、Bug 修复、协议变更必须严格按以下流程执行：

```text
Issue 开启
    ↓
设计笔记（在 Issue 中说明设计决策、影响面、替代方案）
    ↓
Spec 更新（修改对应 spec/*.md，说明变更后语义）
    ↓
Golden Frame 更新（若涉及 wire format，更新 golden/*.bin 与 expected.json）
    ↓
代码实现（遵循本 RULES.md 全部规则）
    ↓
Decoder 更新（若涉及协议或 type tag，同步更新 Python decoder）
    ↓
测试（C host unit tests + Python decoder regression tests）
    ↓
Benchmark（确认热路径周期数无退化，记录于 benchmark report）
    ↓
文档更新（更新 README、api_reference.md、examples 注释）
    ↓
Changelog 更新（按 Keep a Changelog 格式记录）
    ↓
PR 评审（至少 1 人评审，检查是否违反 RULES.md）
    ↓
合并
```

**跳过任何一步 = 拒绝合入。**

---

## 9. 维护流程

### 9.1 日常维护

- **每轮迭代开始**：检查 `ROUTE.md` 当前阶段目标，确认无范围蔓延。
- **每次 commit**：本地运行 `ctest` (host tests) 通过后再 push。
- **每次协议微调**：运行 `../../tool/golden/update_golden.py` 并提交新的 golden frames。
- **每周**：检查 `tests/` 覆盖率，未覆盖的新增代码必须补测试。

### 9.2 版本发布流程

1. 从 `ROUTE.md` 确认当前阶段所有任务已完成。
2. 运行全量测试：`ctest --output-on-failure` + `python ../../tool/tests/test_decoder.py`。
3. 运行 benchmark：`../../tool/benchmark/host_benchmark` 并更新报告。
4. 检查无 P0/P1 问题（见 §10）。
5. 更新 `../changelog/CHANGELOG.md`。
6. 更新 `PLAN.md` 状态。
7. 打 git tag：`git tag v0.x-stage`。
8. 运行 amalgamate 脚本生成单文件库，验证通过测试。
9. 发布 Release Note（含 binary、单文件库、decoder、docs）。

### 9.3 问题分级与响应

| 级别 | 定义 | 响应时间 | 处理方式 |
| :--- | :--- | :--- | :--- |
| **P0** | 数据丢失、静默丢弃、协议不兼容、编译失败。 | 24h | 立即修复，阻断发布。 |
| **P1** | 性能退化 > 20%、API 行为与文档不符、backend 泄漏。 | 72h | 必须修复，延迟发布。 |
| **P2** | 文档错误、示例无法编译、非核心功能缺陷。 | 1 周 | 排期修复。 |
| **P3** | 代码风格、优化建议、增强需求。 | 不定 | 讨论后决定。 |

---

## 10. 发布门槛（v1.0-stable）

只有 **全部满足** 才允许打 `v1.0` tag：

- [ ] stable API (`AX_*` 宏锁定)。
- [ ] stable wire format (header 结构、type tag 定义冻结)。
- [ ] stable event model (Event Record 语义不变)。
- [ ] stable backend contract (`axiom_backend_t` 结构体冻结)。
- [ ] stable capsule format (capsule 布局冻结)。
- [ ] stable decoder (Python decoder 能解析全部 type tag 与 capsule)。
- [ ] stable golden tests (全部通过，无 flakiness)。
- [ ] stable examples (全部可编译、可运行、输出符合预期)。
- [ ] stable benchmark report (热路径周期数基准锁定)。
- [ ] stable docs (README、spec、api_reference 完整、无 TODO)。
- [ ] 无 P0/P1 问题。
- [ ] CI 通过 (host tests + decoder tests + format check)。

---

## 11. 版本命名规则

开发过程中的版本命名（禁止叫 v3）：

| 版本 | 含义 |
| :--- | :--- |
| v0.1-core | Core Plane 可证明。 |
| v0.2-wire | Wire Format 可冻结。 |
| v0.3-frontend | Frontend 可用。 |
| v0.4-backend | Backend 可插拔。 |
| v0.5-probe | Probe 时序探查成熟。 |
| v0.6-capsule | Fault Capsule 成熟。 |
| v0.7-toolchain | Toolchain 完整。 |
| v0.8-portability | 跨平台验证完成。 |
| v0.9-rc | Release Candidate。 |
| v1.0-stable | 稳定发布。 |

---

## 12. 架构合规检查清单

新增代码提交前，作者必须自评以下清单：

- [ ] 所有功能都能归入 `LOG / EVT / PROBE / FAULT / BACKEND / TOOL` 之一。
- [ ] 没有游离模块（新增 .c/.h 必须归入五平面之一）。
- [ ] 没有双协议（不存在 backend 私有协议）。
- [ ] 没有 RTOS/Linux 实现分支（port 层隔离，core 无 OS 依赖）。
- [ ] 热路径无 malloc、无 printf、无 Flash erase、无阻塞。
- [ ] 新增 backend 未修改 core 代码。
- [ ] 新增 type tag 同步更新了 decoder 与 spec。
- [ ] 新增 API 同步更新了 docs、examples、tests。
- [ ] 无静默丢弃（有 drop counter + DROP_SUMMARY）。
- [ ] 示例可独立编译（不依赖未文档化的内部符号）。

**勾选不全 = PR 拒绝合并。**
