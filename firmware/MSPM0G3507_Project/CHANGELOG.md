# Changelog

本项目遵循语义化版本号。日期使用 `YYYY-MM-DD`。

## [0.1.0] - 2026-07-18

### Added

- 统一固件版本头 `Config/project_version.h`；
- 通信协议 v1 的 `Info?`、`Config?`、`Status?`、`Status=0..3`、`Stream=1/0`；
- 稳定的 11 通道 FireWater 遥测契约；
- PySide6 中英双语配置、控制、串口终端、实时绘图与 CSV 导出软件；
- 架构报告、公共接口参考、通信协议、GUI 使用说明及版本化发布说明；
- 生产固件和 FactoryTest 的版本化 HEX 发布产物。

### Changed

- DRV8870 控制命令统一为 `-500..+500`；
- PWM 映射统一为：40%以下反转、40%～55%停止死区、55%以上正转；
- PPR、减速比、编码倍频、轮径、轮距、电机方向和编码器方向集中到 `project_config.h`；
- 四路电机与编码器映射收敛为 A/M1/RB、B/M2/RF、C/M3/LF、D/M4/LB；
- 位置/角度控制目标顺序与上述物理映射一致；
- 命令解析改为严格完整匹配，避免 `runXYZ`、`stopping` 等误触发；
- GUI 断开连接时先尝试发送 `StopAll` 和 `Stream=0`，再关闭串口；
- 文档按快速使用、架构接口、硬件安全和阶段计划分类，并增加统一索引；
- 重写 `.gitignore`，停止跟踪CodeGraph数据库、Keil用户状态和重复构建HEX。

### Fixed

- 修复 DRV8870 多通道映射与编码器对应关系不一致的问题；
- 修复模型辨识 PWM 范围与 DRV8870 有符号命令范围不一致的问题；
- 修复 GUI 串口选择恢复、中文标签、QtCharts 点集替换和安全关闭队列问题。

### Known limitations

- 机械参数和各轮安装方向仍需实车复核；
- ADC 电流采样尚未标定为安培，不能替代硬件 OCP；
- 参数配置不会持久化到 MCU Flash；
- 单 PWM 锁相硬件不能提供独立 Coast/Brake；
- 文本日志与 FireWater 仍共享 UART0；
- FactoryTest 只适用于悬空轮和限流电源下的短时诊断。
