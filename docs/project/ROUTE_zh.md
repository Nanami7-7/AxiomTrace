> [English](ROUTE.md) | [简体中文](ROUTE_zh.md)

# AxiomTrace 开发路线图

> 版本：v1.0 | 状态：**执行中** | 更新日期：2026-05-28

---

## v0.1-core — Core 可证明

**目标**：证明最小内核正确。无 malloc、无 printf、无阻塞、O(1)、ISR 可写。

- [x] 仓库目录结构按五平面重组。
- [x] `PLAN.md` / `ROUTE.md` / `RULES.md` 定稿。
- [x] `../../spec/event_model.md` 定稿（Event Record 语义与版本化 payload 解释）。
- [x] `../../spec/wire_format.md` 定稿（当前 wire frame 结构）。
- [x] `../../spec/backend_contract.md` 定稿（`axiom_backend_t` 接口）。
- [x] `../../spec/fault_capsule.md` 定稿（capsule 格式与生命周期）。
- [x] `../../spec/api_reference.md` 初稿（Frontend 宏 API）。
- [x] 无锁 ISR-safe RAM Ring `axiom_ring.c`。
- [x] Event Record 组装 `axiom_event.c`（header + payload_len + payload + crc）。
- [x] 二进制编码器 `axiom_encode.h`（`_Generic` 类型分发 + v2 packed 值）。
- [x] CRC-16/CCITT-FALSE 查表 `axiom_crc.c`。
- [x] 压缩相对时间戳 `axiom_timestamp.c`。
- [x] 级别过滤与丢弃统计 `axiom_filter.c`。
- [x] Memory Backend `axiom_backend_memory.c`。
- [x] Host Unit Tests：ring、encoder、CRC、event、filter、backend、location、profile、dynamic call-chain 与 benchmark 覆盖。
- [x] CMake 构建系统适配新目录。

**验收标准**：
- `ctest` 全部通过。
- 热路径 `axiom_write()` 周期数可测量且稳定。
- 无 placeholder 代码。

---

## v0.2-wire — Wire Format 可冻结

**目标**：协议能被长期维护。encoder → frame → decoder → expected.json 完全一致。

- [x] 冻结 binary frame 字段顺序与大小。
- [x] 生成首批 golden frames（覆盖 packed schema、metadata suffix 与边界 payload）。
- [x] 编写 `expected.json`（decoder 期望输出）。
- [x] Python decoder `../../tool/decoder/axiom_decoder.py`（解析 header/payload/crc）。
- [x] decoder 非法帧以结构化错误显式拒绝且不崩溃。
- [x] `../../tool/golden/update_golden.py`（encoder 生成 bin + expected.json）。
- [x] Host regression test：`tests/test_python_tools.py` 对 golden frames 全量比对。
- [x] 文档更新：`../../spec/wire_format.md` 标记为 **FROZEN**。

**验收标准**：
- `encoder -> frame -> decoder -> expected.json` 100% 一致。
- 篡改 crc、payload_len、version 的非法帧被显式拒绝。
- packed schema 或 metadata suffix 变化时，`update_golden.py` 能自动生成对应 golden frame。

---

## v0.3-frontend — Frontend 可用

**目标**：用户真正好上手。5 分钟打出第一条日志。

- [x] `AX_LOG(msg)` 宏实现（开发可读文本，运行时通过 `axiom_port_string_out()` 输出文本；PROD profile 展开为空）。
- [x] `AX_EVT(level, mod, evt, args...)` 宏实现（结构化事件，`_Generic` 编码）。
- [x] `AX_PROBE(tag, value)` 宏实现（高频低扰动探查，可选裁剪）。
- [x] `AX_FAULT(mod, evt, args...)` 宏实现（故障追溯，触发 fault hook）。
- [x] `AX_KV(level, mod, evt, "k", v, ...)` 宏实现（轻量键值对）。
- [x] `DEV / FIELD / PROD` Profile 编译期裁切宏。
- [x] `axiom_frontend.h` 统一入口（自动包含所有 Frontend 宏）。
- [x] Example：`example_minimal.c`（3 行代码起步）。
- [x] Example：`example_full.c`（多 API 组合 + filter + backend）。
- [x] Host tests：各 Profile 下宏展开行为验证。

**验收标准**：
- `example_minimal.c` 在主机 `gcc` 下零配置编译通过并输出预期结果。
- `PROD` profile 下 `AX_LOG` / `AX_PROBE` 编译为空，不产生代码/数据。
- 所有 Frontend API 最终调用同一个 `axiom_write()`。

---

## v0.4-backend — Backend 可插拔

**目标**：外设全兼容，但不绑芯片。新增 backend 零侵入 core。

- [x] `axiom_backend.c` 注册表与分发器实现。
- [x] `axiom_backend_t` 结构体与 `axiom_backend_register()` API。
- [x] Memory Backend 完善（ring buffer 区域直接导出）。
- [x] UART Backend Template（COBS 编码 + 0x00 delimiter，用户填 UART 发送函数）。
- [ ] USB CDC Backend Template（bulk IN endpoint，用户填 USB 发送函数）。
- [x] RTT Backend Template（SEGGER RTT up-channel，用户填 SEGGER RTT 函数）。
- [ ] SWO/ITM Backend Template（32-bit stimulus word 流，用户填 ITM 函数）。
- [ ] CAN-FD Backend Template（帧拆分与 ID 映射，用户填 CAN 发送函数）。
- [x] Backend drop callback 与限速/降级机制。
- [x] Host tests：`test_backend_ext.c` 与 `test_dynamic_call_chain.c`（注册、dispatch、drop、busy backend、降级、恢复、panic_write）。

**验收标准**：
- 新增一个 backend 只需：实现 3 个函数 + 调用 `axiom_backend_register()`，不修改 `core/`。
- backend busy 时返回 `-EAGAIN`，Core 正确累计 drop counter。
- 所有 template 在主机用 mock 函数编译通过。

---

## v0.5-probe — Probe 时序探查成熟

**目标**：高频信号追踪不扰动业务逻辑。

- [ ] `AX_PROBE(tag, value)` 高频优化（绕过 filter、极简 header、可选无 crc）。
- [ ] Probe ring 独立缓冲区（与主 ring 隔离，避免探查数据覆盖关键日志）。
- [ ] Probe 后端：SWO/ITM（推荐，无损输出）。
- [ ] Probe decoder 支持（将 probe 流还原为时序波形描述）。
- [ ] Benchmark：probe 写入周期数 < 100 周期（Cortex-M4 @ 80MHz）。
- [ ] Host tests：probe ring 独立性与覆写策略测试。

**验收标准**：
- 1000Hz probe 不丢主 ring 事件。
- probe 数据可通过 decoder 导出为 CSV/JSON 时序。

---

## v0.6-capsule — Fault Capsule 成熟

**目标**：故障可追溯，形成核心差异化。

- [x] Dual-zone ring 设计（Normal Ring + Capsule Ring）。
- [x] `AX_FAULT()` 触发 freeze：拷贝 pre-window 事件到 Capsule Ring。
- [x] post-window 事件继续写入 Capsule Ring 直至满或窗口结束。
- [x] 寄存器快照：`pc`、`lr`、`sp`、`xpsr`（通过 port 层获取）。
- [x] reset reason 记录（通过 port 层获取）。
- [x] firmware hash（编译期注入 `__attribute__((section))` 字符串，CRC 校验）。
- [x] capsule CRC（覆盖 capsule 全部内容）。
- [x] `axiom_capsule_commit()`：将 Capsule Ring 内容写入 Flash（非 ISR）。
- [x] `axiom_capsule_present()` / `axiom_capsule_read()` / `axiom_capsule_clear()`。
- [x] Flash 掉电恢复测试（擦除中断、写入中断后数据完整性）。
- [x] Host tests：`test_capsule.c`（freeze、pre/post 窗口、crc、read/clear）。

**验收标准**：
- 正常运行 0 Flash 写入。
- Fault 触发后 capsule 完整可读，pre/post 窗口事件无丢失。
- 掉电恢复后 capsule 数据通过 CRC 校验。

---

## v0.7-toolchain — Toolchain 完整

**目标**：二进制日志可读、可测、可导出。

- [x] decoder 完善：支持当前 packed frame、历史 typed frame、capsule 格式与 DROP_SUMMARY。
- [x] text render：dictionary 模板填充（`"motor current over limit: phase={u8}"`）。
- [x] json export：完整事件流导出为 JSON array。
- [x] 标准元数据包：`manifest.json`、`dictionary.json`、`source_map.json`、`build_info.json`，可选 `firmware.elf`/`.map`。
- [x] `axiom-bundle generate`：根据事件定义、ELF、compile database 生成标准 bundle。
- [x] CMake 集成：`axiomtrace_add_bundle(TARGET ... EVENTS ... OUTPUT_DIR ...)`。
- [x] Decoder bundle 模式：`--bundle`、`--bundle-store`、元数据身份匹配和 raw fallback。
- [x] capsule report：面向 draft capsule 格式的主机侧 JSON/Markdown/HTML 故障分析报告。
- [x] dictionary validator：校验 payload 类型与 dictionary 模板匹配。
- [x] `../../tool/scripts/amalgamate.py`：将 core+frontend+port 合并为单文件头；GNU11 include 烟测通过。
- [ ] amalgamated 产物作为发布制品通过完整 host test 矩阵。
- [x] `../../tool/scripts/extract_dict.py`：从 C 源码/X-Macro 提取 `dictionary.json`。
- [x] benchmark 工具：`tests/host/test_benchmark.c` 与 `../../tool/benchmark/host_benchmark.c` 测量编码/CRC/ring write 周期。
- [x] golden 回归：本地 `update_golden.py --check` 与 Python golden tests。
- [x] CI gate：相关变更自动运行 `update_golden.py --check` + Python decoder regression tests。
- [x] 文档治理：README 只做索引；详细契约归主规范文档；禁止未链接的临时 Markdown。

**验收标准**：
- `binary -> text` 通过全部 golden frames。
- `binary -> json` 结构正确、字段完整。
- `trace -> bundle -> text/jsonl` 在开启定位时能还原事件语义和源码位置。
- `trace -> bundle-store` 在固件身份不匹配时拒绝语义解码，除非显式 raw mode。
- `capsule -> report` 包含寄存器、事件序列、firmware hash。
- amalgamate 产物通过全部 host tests。

---

## v0.8-portability — 跨平台验证

**目标**：证明不止能在一种 MCU 上跑。

- [ ] Cortex-M port 完善（DWT_CYCCNT / SysTick / DisableIRQ / Flash HAL hook）。
- [ ] RISC-V port 完善（mtime / CLINT / 中断控制 / Flash hook）。
- [ ] Generic port（主机模拟）完善，作为 CI 默认运行环境。
- [ ] 在至少 2 个真实 MCU 项目（不同厂商）中集成验证。
- [ ] 集成文档：`../reference/porting_guide.md`。
- [x] Host 跨编译器验证：GCC + Clang。IAR 仍为可选且依赖目标工程。

**验收标准**：
- 同一套 `core/` + `frontend/` 代码，只换 `port/` 即可在不同平台编译通过。
- port 层未覆盖的函数提供弱符号默认实现，编译不报错。

---

## v0.9-rc — Release Candidate

**目标**：不再加功能，只清问题。

- [ ] API review（命名一致性、宏行为一致性、文档与代码一致）。
- [ ] spec review（所有 ../../spec/*.md 与代码实现逐条核对）。
- [ ] backend review（全部 template 用 mock 驱动编译通过）。
- [ ] capsule review（freeze / commit / dump 全路径测试通过）。
- [ ] benchmark report 定稿（热路径周期数锁定，记录于 `../reference/benchmark.md`）。
- [ ] fuzz malformed frame（自定义 fuzz 目标：随机篡改 frame 各字段，decoder 不崩溃）。
- [ ] fault injection（模拟 HardFault，验证 capsule 捕获完整性）。
- [ ] power-loss simulation（Flash 擦除/写入中途断电，重启后 capsule 可恢复）。
- [ ] docs complete（README、api_reference、porting_guide、examples 注释）。
- [ ] examples complete（全部独立可编译、可运行、输出符合预期）。
- [ ] 全量 P0/P1 清零。

---

## v1.0-stable — 稳定发布

**目标**：正式 v1.0。

- [ ] 满足 `PLAN.md` §4 全部发布门槛。
- [ ] 满足 `RULES.md` §10 全部发布门槛。
- [ ] 打 git tag：`git tag v1.0-stable`。
- [ ] 发布 Release Note（含 binary、单文件库 `axiomtrace.h`、decoder、docs）。
- [ ] 公告：开发可读、量产低扰动、故障可追溯、协议可冻结、工具链可信。
