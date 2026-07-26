# Changelog

本项目遵循语义化版本号。日期使用 `YYYY-MM-DD`。

## [Unreleased] - 2026-07-26

### Changed

- 完成 `e62ee2f` 配置重构在当前 MSPM0G3507 工程中的适配：以 `Config/project_config.h` 作为项目级统一入口，以 `Config/filter_tuning.h` 作为 EKF/KF 调参入口；
- 删除已无调用点的版本配置、测试配置和 IMU/BSP 兼容配置，保留协议/板卡/电机名称兼容别名以避免影响外部工具；
- 将 IMU 遥测周期、DMA 缓冲区、KF 静态缓冲区、任务参数和 MATHACL 矩阵开关纳入统一 `PRJ_*` 配置；
- 明确生产 Target 与 FactoryTest Target 的配置边界，避免通过修改公共配置文件切换测试模式。

### Verification

- 生产 Target `empty_LP_MSPM0G3507_nortos_keil`：Keil 重编译通过，0 Error(s), 0 Warning(s)；
- `PRJ_MATHACL_MATRIX_ENABLE=1` 的临时编译验证通过，验证后已恢复为默认值 `0`；
- FactoryTest Target 的链接区容量不足为迁移前已存在的问题，本次未改变其链接布局。
## [0.1.0] - 2026-07-18

### Added

- 统一固件版本头 `Config/project_config.h`；
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
