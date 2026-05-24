> [English](PLAN.md) | [简体中文](PLAN_zh.md)

# AxiomTrace v1.0 目标规划

> 版本：v1.0 | 状态：**构建中** | 更新日期：2026-04-29

---

## 1. v1.0 定位

**AxiomTrace v1.0** = MCU 裸机可观测微内核

构成：Event Core + Log Facade + Realtime Probe + Fault Capsule + Backend Contract + Toolchain

**不是**：
- 不是 printf 替代品。
- 不是 RTOS 日志库。
- 不是 Linux 日志库。
- 不是全量文件系统。
- 不是芯片 HAL 封装库。

**最终定义**：AxiomTrace v1.0 不是功能堆满，而是这个裸机可观测微内核已经证明——开发可读、量产低扰动、故障可追溯、协议可冻结、工具链可信。

---

## 2. v1.0 最终架构

```text
AxiomTrace/
  Semantic Plane   // 事件语义：level / module / event / payload / schema
  Frontend Plane   // 用户入口：AX_LOG / AX_EVT / AX_PROBE / AX_FAULT / AX_KV
  Core Plane       // 核心：ring / encode / filter / drop / timestamp / crc
  Backend Plane    // 后端：memory / RTT / USB / UART / SWO / CAN / flash capsule
  Tool Plane       // 工具：decoder / viewer / codegen / golden / benchmark
```

### 核心原则

> **Event Record 是日志本体。Text / JSON / Binary 都只是渲染或传输形态。**

---

## 3. v1.0 路线图

### 阶段 0：冻结目标规格

**目标**：先把 v1.0 长什么样写死。

**交付**：
- `PLAN.md`
- `ROUTE.md`
- `RULES.md`
- `../../spec/event_model.md`
- `../../spec/wire_format.md`
- `../../spec/backend_contract.md`
- `../../spec/fault_capsule.md`
- `../../spec/api_reference.md`

**验收**：
- 所有功能都能归入 `LOG / EVT / PROBE / FAULT / BACKEND / TOOL`。
- 没有游离模块。
- 没有双协议。
- 没有 RTOS/Linux 实现分支。

---

### 阶段 1：Core 可证明

**目标**：证明最小内核正确。

**交付**：
- RAM Ring（无锁、IRQ-safe、单生产者单消费者）。
- Event Record（固定 8B header + 1B payload_len + payload + 2B crc16）。
- Encoder（`_Generic` 类型安全分发 + 类型标签写入）。
- CRC-16/CCITT-FALSE（256B ROM 查表）。
- 压缩相对 Timestamp（delta 编码）。
- Memory Backend（直接写 RAM Ring 区域）。
- Host Unit Test（ring、encode、crc、event 组装）。

**规则**：
- 无 malloc。
- 无 printf。
- 无阻塞。
- O(1) 写入。
- ISR 可写。

---

### 阶段 2：Wire Format 可冻结

**目标**：协议能被长期维护。

**交付**：
- binary frame 定义冻结。
- payload_len (1B)。
- event_id (2B LE)。
- module_id (1B)。
- seq (2B LE)。
- timestamp (4B LE, 压缩 delta)。
- crc16 (2B LE)。
- Golden frames (*.bin)。
- expected.json（解码器期望输出）。
- Python decoder 初版。

**验收**：
- `encoder -> frame -> decoder -> expected.json` 完全一致。
- 非法帧必须显式拒绝（返回 `FRAME_INVALID`）。
- 每次协议变化必须更新 golden frame 并运行回归测试。

---

### 阶段 3：Frontend 可用

**目标**：用户真正好上手。

**交付**：
- `AX_LOG(msg)` — 开发可读，量产时通过 Profile 裁剪为空。
- `AX_EVT(level, mod, evt, args...)` — 量产结构化事件。
- `AX_PROBE(tag, value)` — 时序探查，高频低扰动。
- `AX_FAULT(mod, evt, args...)` — 故障追溯，触发 Capsule。
- `AX_KV(level, mod, evt, "k", v, ...)` — 轻量键值。
- `DEV / FIELD / PROD` Profile 编译期裁切。

**边界**：
- `AX_LOG`：开发可读，PROD profile 下展开为空。
- `AX_EVT`：量产结构化事件，所有 profile 保留。
- `AX_PROBE`：时序探查，FIELD/PROD 可裁剪。
- `AX_FAULT`：故障追溯，所有 profile 保留。

**禁止**：
- `AX_LOG / AX_EVT / AX_PROBE / AX_FAULT` 各自定义不同协议。
- 所有 Frontend API 最终都调用同一个 `axiom_write()`。

---

### 阶段 4：Backend 可插拔

**目标**：外设全兼容，但不绑芯片。

**交付**：
- Memory Backend。
- UART Backend Template (COBS + 0x00 delimiter)。
- USB CDC Backend Template (bulk IN endpoint)。
- RTT Backend Template (SEGGER RTT up-channel)。
- SWO/ITM Backend Template (32-bit stimulus word 流)。
- CAN-FD Backend Template (帧拆分与 ID 映射)。
- Flash Capsule Backend (故障时 commit，正常不写 Flash)。

**Backend Contract**:

```c
typedef struct {
    const char *name;
    uint32_t caps;

    int  (*write)(const uint8_t *buf, uint16_t len, void *ctx);
    int  (*ready)(void *ctx);
    int  (*flush)(void *ctx);
    int  (*panic_write)(const uint8_t *buf, uint16_t len, void *ctx);

    void (*on_drop)(uint32_t lost, void *ctx);
    void *ctx;
} axiom_backend_t;
```

**规则**：
- 用户负责外设驱动（填充 `write/ready` 函数指针）。
- AxiomTrace 负责缓存、限速、降级、丢弃统计、协议一致性。
- 新增 backend 零侵入 core (不修改 `core/`、`frontend/` 任何文件)。

---

### 阶段 5：Fault Capsule 成熟

**目标**：形成核心差异化。

**交付**：
- Pre-window freeze（故障前 N 条事件保留）。
- Post-window capture（故障后 M 条事件捕获）。
- Reset reason 记录。
- Fault type 分类。
- PC / LR / SP / xPSR 寄存器快照。
- Firmware hash（编译期注入）。
- Capsule CRC（完整性校验）。
- Flash commit（非 ISR 调用）。
- Reboot dump 支持。

**规则**：
- 正常运行默认不写 Flash。
- 故障触发后才 commit capsule。
- Flash erase/write 不进入日志热路径。
- Capsule 数据格式稳定、自描述、可解码。

---

### 阶段 6：Toolchain 完整

**目标**：二进制日志必须可读、可测、可导出。

**交付**：
- decoder（binary → 结构化对象）。
- text render（结构化对象 → 人类可读文本，支持 dictionary 模板填充）。
- json export（结构化对象 → JSON 文件）。
- 标准元数据包（`manifest.json`、`dictionary.json`、`source_map.json`、`build_info.json`，可选 ELF/map 产物）。
- bundle 生成器（`axiom-bundle generate`）和 CMake helper（`axiomtrace_add_bundle(...)`）。
- 基于固件身份的 bundle-store 精确匹配解码。
- capsule report（capsule binary → 故障分析报告）。
- dictionary validator（校验固件 payload 与 dictionary 模板类型匹配）。
- golden frame updater（编码器生成标准帧，供回归测试）。
- benchmark tool（热路径周期数测量与报告）。
- 文档治理：每个主题只有一个主文档，禁止临时 Markdown。

**验收**：
- binary → text（通过 decoder + render）。
- binary → json（通过 decoder + json export）。
- trace + bundle → 开启定位时可还原事件语义和源码位置。
- trace + bundle-store → 根据固件身份精确选择 bundle。
- capsule → report（通过 capsule decoder）。
- golden → regression test（每次 CI 自动运行）。

---

### 阶段 7：v1.0 Release Candidate

**目标**：不再加功能，只清问题。

**必须完成**：
- API review（命名、参数、宏行为一致性）。
- spec review（所有 `../../spec/*.md` 与代码一致）。
- backend review（全部 template 编译通过）。
- capsule review（freeze/commit/dump 全路径测试）。
- benchmark report（热路径周期数锁定）。
- fuzz malformed frame（libFuzzer 或自定义 fuzz 目标）。
- fault injection（模拟 HardFault，验证 capsule 捕获）。
- power-loss simulation（验证 Flash capsule 掉电恢复）。
- docs complete（README 只做入口、主规范文档、工具 help、examples 注释完整；无重复独立 Markdown）。
- examples complete（全部可编译、可运行）。

---

## 4. v1.0 发布门槛

只有 **全部满足** 才发正式 v1.0：

- [ ] stable API（`AX_*` 宏锁定）。
- [ ] stable wire format（header 结构、type tag 定义冻结）。
- [ ] stable event model（Event Record 语义不变）。
- [ ] stable backend contract（`axiom_backend_t` 结构体冻结）。
- [ ] stable capsule format（capsule 布局冻结）。
- [ ] stable decoder（能解析全部 type tag 与 capsule）。
- [ ] stable golden tests（全部通过）。
- [ ] stable examples（全部可编译、可运行）。
- [ ] stable benchmark report（热路径周期数基准锁定）。
- [ ] stable docs（README、spec、api_reference 完整）。
- [ ] 无 P0/P1 问题。

---

## 5. 设计哲学摘要

| 维度 | 选择 | 理由 |
| :--- | :--- | :--- |
| 日志本体 | Event Record | 固件侧统一，主机侧多形态渲染。 |
| 默认存储 | RAM Ring | 热路径低延迟、不掉电可事后读取。 |
| Flash 用途 | 仅 Fault Capsule | 避免量产中 Flash 磨损、掉电恢复复杂。 |
| API 设计 | 结构化宏 + Profile 裁剪 | 开发时好用 (AX_LOG)，量产时零开销 (PROD 裁剪)。 |
| 类型安全 | C11 `_Generic` | 编译期检查，零运行时开销。 |
| 扩展性 | Backend Contract + 弱符号 Port | 新增后端不修改库，新平台只需覆盖 port 函数。 |
| 用户友好 | 单文件库 + 零配置起步 + 渐进式复杂度 | 5 分钟上手，按需升级工具链。 |
| 可信 | Golden test + decoder 回归 + benchmark | 每次变更可验证，协议可追溯。 |
