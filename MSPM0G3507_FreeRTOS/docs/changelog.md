# 变更日志 / Changelog

All notable changes to this project will be documented in this file.

---

## [v2.2] - 2026-05-07

### Changed
- **Target RPM 上限调整** / **Target RPM limit adjustment**
  - `VOFA_TARGET_RPM_MAX` 从 500.0 调整为 800.0
  - VOFA+ 协议文档同步更新
  - VOFA_TARGET_RPM_MAX increased from 500.0 to 800.0

---

## [v2.1] - 2026-05-06

### Fixed
- **setpoint 持久性修复** / **setpoint persistence fix**
  - `app_motor_stop()` 和 `app_motor_stop_all()` 不再清零 PID setpoint
  - 停机时保留目标 RPM，重启时可立即以原目标恢复运行
  - 解决了 VOFA+ Stop→Run 需要重新设置 Target 的问题
  - Removed setpoint clearing from `app_motor_stop()` and `app_motor_stop_all()`
  - Target RPM is preserved across stop/start cycles

### Added
- **完整开发与使用文档** / **comprehensive development and usage documentation**
  - `docs/README.md` — 文档索引 / Documentation index
  - `docs/architecture.md` — 系统架构详解 / System architecture deep dive
  - `docs/pid_controller.md` — PID 算法详解 / PID algorithm deep dive
  - `docs/vofa_protocol.md` — VOFA+ 协议详解 / VOFA+ protocol specification
  - `docs/menu_guide.md` — 菜单交互指南 / Menu & operation guide
  - `docs/development_guide.md` — 开发指南 / Development guide
  - `docs/hardware_setup.md` — 硬件配置 / Hardware setup
  - `docs/troubleshooting.md` — 故障排除 / Troubleshooting guide
  - 所有文档中英双语 / All documents bilingual (Chinese/English)

---

## [v2.0] - 2026-05-05

### Added
- **VOFA+ 远程调参** / **VOFA+ remote tuning**
  - FireWater 上行格式：12 通道 RPM/Target/Output 实时输出
  - JustFloat 二进制格式支持
  - 下行命令：Kp/Ki/Kd/Target/Run/Stop/StopAll/Motor
  - NaN/Inf 安全保护和参数限幅
- **增量式 PID** / **incremental PID**
  - 从位置式 PID 重构为增量式 PID
  - 天然抗饱和、故障安全、输出平滑
  - 支持位置式/增量式双模式
- **FreeRTOS 任务模型** / **FreeRTOS task model**
  - 控制任务 (priority=5, 5ms) + 菜单任务 (priority=2, 100ms/30ms)
  - OSAL 抽象层支持裸机/RTOS 切换
- **四层架构** / **four-layer architecture**
  - App → BSP → HAL → OSAL
  - 硬件无关的 App 层
- **AxiomTrace 调试日志** / **AxiomTrace debug logging**
  - AX_LOG_INFO/WARN/ERROR 宏
  - DEV profile 配置

### Fixed
- **FireWater 格式修正** / **FireWater format fix**
  - 从错误的 `channel:float\n` 格式修正为标准 `val0,val1,...\n` 格式
  - VOFA+ 现在能正确解析波形数据
- **VOFA+ 带宽溢出** / **VOFA+ bandwidth overflow**
  - 输出周期从 10ms 调整为 30ms
  - 带宽占用率从 135% 降至 45%

### Changed
- **PID 重构** / **PID refactoring**
  - 从全局 `pid_lf/lb/rf/rb` 改为 `app_pid_t` 多实例
  - 去除 `GetUs()` 硬件依赖，由调用者传入 dt
  - 修复旧代码中局部变量遮蔽 bug
- **编码器测速** / **encoder speed measurement**
  - 使用 int64_t 中间变量防止溢出
  - 完善方向修正和 2 倍频支持
- **菜单状态机** / **menu state machine**
  - 5 状态状态机：MAIN/MOTOR_SELECT/SET_RPM/TUNING_PID/RUNNING
  - 行输入支持退格、回显
  - VOFA+ 命令集成到 RUNNING 状态

---

## [v1.0] - 初始版本

### Features
- 基本电机 PWM 控制
- 编码器脉冲计数
- 简单 P 控制
- 串口菜单原型

---