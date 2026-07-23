> [English](versioning.md) | [简体中文](versioning_zh.md)

# AxiomTrace 版本管理与兼容性

## 1. 语义化版本 (Semantic Versioning)

AxiomTrace 遵循 [SemVer 2.0.0](https://semver.org/) 规范：

- **MAJOR (主版本号)**: 不兼容的 ABI 或传输格式 (Wire Format) 变更。
- **MINOR (次版本号)**: 向后兼容的功能新增。
- **PATCH (修订号)**: 错误修复、文档更新、性能优化。

## 2. 传输格式版本 (Wire Format Version)

事件头部 (Event Header) 中的 `version` 字节编码为 `major << 4 | minor`。

- 解码器 (Decoder) 必须拒绝不支持的 **主版本号**。
- **次版本号** 的新增必须在当前主版本的 payload 解释方式内保持兼容。

当前发出的 wire 版本为 `v2.0` (`0x20`)：普通参数按照 metadata dictionary 紧凑编码。Decoder 同时支持历史 `v1.0` 与 `v1.1` typed-payload frame。

## 3. API 稳定性

| API 层面                        | 稳定性保证                        |
|---------------------------------|-----------------------------------|
| `AXIOM_*` 日志宏                | 自 v0.1 起稳定                    |
| `axiom_port_*` 弱符号接口       | 自 v0.1 起稳定                    |
| `axiom_backend_register` 函数   | 自 v0.1 起稳定                    |
| 内部 `_Generic` 编码函数        | 可能会在次版本号更新中变更        |
| `axiom_ring_*` 内部 API         | 可能会在次版本号更新中变更        |

## 4. ABI 兼容性

- 每个主版本号内的头部大小 (Header Size) 和字段偏移量 (Field Offsets) 是固定的。
- Wire v2 的 metadata suffix 标签追加在 packed 参数之后；普通参数类型由匹配 dictionary 定义。
- 后端描述符结构体可能会在次版本号更新中向后扩展；推荐使用 `AXIOM_BACKEND_INIT(...)` 实现结构体前向兼容初始化。旧的零初始化后端（size==0）视为旧版布局。
