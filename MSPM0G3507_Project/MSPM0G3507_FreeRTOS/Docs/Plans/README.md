# 阶段计划与历史审计

本目录保存安全整改、硬件验证和后续重构计划，用于追溯工程决策。当前有效的发布接口和使用方法应以父目录中的v0.1.0文档为准。

| 文档 | 状态 | 说明 |
|---|---|---|
| [phase1_safety_realtime_baseline.md](phase1_safety_realtime_baseline.md) | 已执行基线 | 第一阶段安全性、实时性和UART路径审计记录 |
| [phase1_gate0_hardware_validation_protocol.md](phase1_gate0_hardware_validation_protocol.md) | 验证规程 | 硬件映射和安全门禁的可重复验证步骤 |
| [phase2_phase3_controlled_remediation_plan.md](phase2_phase3_controlled_remediation_plan.md) | 后续计划 | Phase 2/3优先级、执行逻辑、风险和回滚方案 |
| [phase_all.md](phase_all.md) | 历史草稿 | 早期全阶段架构审计输入，仅供追溯，不作为当前规范 |

## 使用原则

- 执行任何计划前，先创建Git检查点并确认可烧录的回滚HEX；
- 计划内容与v0.1.0发布文档冲突时，以发布文档和已构建代码为准；
- 实测结果必须记录硬件版本、固件提交、供电电压、负载条件和串口日志；
- 未完成硬件验证门禁前，不进入高功率、堵转、短路或热保护测试。
