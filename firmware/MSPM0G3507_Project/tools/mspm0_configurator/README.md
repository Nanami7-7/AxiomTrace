# MSPM0 Control Studio 0.3.1

Safety-gated bilingual control, diagnostics and real-time telemetry studio for the MSPM0G3507 four-motor controller.

基于 PySide6 的中英双语四电机控制、诊断与实时遥测工具，包含协议握手门禁、状态回读和急停优先通道。

## Highlights / 核心能力

- Strict device/protocol handshake before motor controls are enabled / 严格设备与协议握手后才开放电机控制
- Priority emergency stop (`Ctrl+Shift+Space`) that discards queued normal commands / 急停优先并丢弃尚未发送的普通命令
- All 11 FireWater channels, live values, pause, filtering and CSV export / 完整 11 通道曲线、实时数值、暂停、筛选与导出
- **Collapsible sidebars with icon rails** — collapsed left rail shows tab icons, right rail shows live RPM summary / **侧栏收起后显示图标导航条，左侧可快速切换 Tab，右侧显示实时转速**
- Full-screen telemetry focus mode (`F11`, `Esc`) / 全屏遥测视图（F11，Esc 恢复）
- **Sidebar toggle shortcuts** — `Ctrl+[` toggles left, `Ctrl+]` toggles right / **侧栏快捷键** — `Ctrl+[` 左侧，`Ctrl+]` 右侧
- **Window geometry persistence** — position, size, sidebar state saved/restored / **窗口状态记忆** — 位置、大小、侧栏状态自动保存恢复
- **Connection duration timer** in status bar / **状态栏连接时长计时器**
- **Pulse animations** for connecting state and emergency button / 连接中和急停按钮的脉冲动画
- **Centralised theme system** — all colours, spacing, radii and typography in `theme.py` / **统一主题系统** — 所有颜色、间距、圆角、字体层级集中管理
- Typed status backfill, bounded data/console memory and motion confirmation / 类型化状态回填、内存有界与运动确认
- Bilingual (中文 / English) with complete tooltip coverage / 中英双语，完整 tooltip 覆盖

## What's New in 0.3.1 / 0.3.1 新特性

### Console Enhancement / 串口终端增强
- Command history navigation (Up/Down arrow) / 命令历史导航（上/下箭头浏览）
- Quick command buttons (Info?, Config?, Status?, Stream=0, Stream=1) / 快捷命令按钮
- Timestamp prefix (HH:MM:SS) on all console lines / 控制台时间戳前缀
- Clear console button / 清空控制台按钮
- Auto-scroll toggle / 自动滚动开关
- RX/TX byte counters / 收发字节计数器

### Motor State Card / 电机状态卡片
- Motor colour indicator bar (left border) / 电机颜色指示条（左边框）
- RPM colour by direction (forward=blue, reverse=orange, stopped=grey) / RPM 方向颜色渐变
- Target vs actual RPM deviation percentage / 目标与实际 RPM 偏差百分比

### Connection Bar / 连接栏
- Connection button state colours (grey/orange/green/blue) / 连接按钮状态色
- Serial port icons (device available vs empty) / 串口图标指示
- DTR/RTS status indicator dots / DTR/RTS 状态指示灯

### Tab Improvements / 标签页改进
- Unsaved changes badge on Motor tab / 电机标签未保存修改徽章
- Tab switch shortcuts (Ctrl+1/2/3, Ctrl+Shift+1/2) / 标签切换快捷键

### Status Bar / 状态栏
- Current motor indicator / 当前电机指示
- Serial port name display / 串口名显示

### Safety / 安全性
- Emergency stop confirmation dialog (when triggered via shortcut with inactive window) / 急停确认对话框（快捷键触发且窗口未激活时）
- Disconnect confirmation when motors are running / 电机运行时断开确认
- PID parameter summary in run confirmation / 运行确认中显示 PID 参数摘要

### Visual Details / 视觉细节
- Emergency button inner glow effect / 急停按钮内发光效果
- Motor state box colour-coded left border / 电机状态框颜色编码左边框
- Gradient divider styling / 渐变分割线样式

### Build Fix / 构建修复
- build_release.py now reads version from __init__.py / build_release.py 从 __init__.py 读取版本号

## What's New in 0.3.0 / 0.3.0 新特性

### UI/UX Deep Optimization / 深度优化

1. **Sidebar collapse to icon rails / 侧栏收起为图标导航条**
   - Left sidebar collapses to a narrow rail with motor/motion/device tab icons
   - Right sidebar collapses to a thin strip showing live RPM and motor state
   - Rail expand buttons restore full sidebar with smooth animation
   - Shortcuts: `Ctrl+[` (left), `Ctrl+]` (right)

2. **Detail refinements / 细节完善**
   - QGroupBox titles with accent underline decoration
   - Tab bar with Unicode icon prefixes (⚙ ➤ ℹ)
   - QLabel.setBuddy associations for all input fields
   - Compact icon-style toolbar buttons in telemetry view
   - Pulse animation on status badge during connecting/handshake
   - Breathing animation on emergency stop button when connected
   - Zebra stripe table rows with improved contrast
   - Window title shows connection status and selected motor
   - Comprehensive tooltips on all interactive elements
   - Logical Tab order for keyboard navigation

3. **Normalisation / 规范化**
   - `theme.py` centralises all design tokens (colours, spacing, radii, typography)
   - `DarkPalette` class with named colour constants
   - Spacing constants: XS/SM/MD/LG/XL/2XL/3XL
   - Radii: 6px (controls), 8px (containers), 10px (panels)
   - Typography scale: H1/H2/H3/Body/Caption with font weight constants
   - `Icons` class with Unicode symbol set (no external resources)
   - Future light-theme support: swap `Palette` reference in `theme.py`

4. **Additional improvements / 额外改进**
   - Connection duration timer in status bar (HH:MM:SS / MM:SS)
   - Window geometry state saved/restored (position, size, maximised, sidebar visibility, splitter sizes)
   - Motor combo box with colour indicator tooltips
   - Refresh button with ↻ icon prefix

## Install and run / 安装与运行

### Option A: Development mode / 开发模式

```powershell
cd tools\mspm0_configurator
python -m venv .venv
.venv\Scripts\python.exe -m pip install -e .
.venv\Scripts\python.exe -m mspm0_configurator
```

### Option B: Standalone executable / 独立可执行文件

Pre-built executables are available in `tools/release/`:

- Windows: `tools/release/mspm0-configurator/mspm0-configurator.exe`
- Linux: `tools/release/mspm0-configurator/mspm0-configurator`

Or download the latest archive from CI:
- `mspm0-configurator-windows.zip`
- `mspm0-configurator-linux.tar.gz`

### Option C: Build from source / 从源码构建

```powershell
cd tools\mspm0_configurator
python -m pip install -e ".[build]"
python build_release.py --clean
# Output: tools/release/mspm0-configurator/
```

Run the protocol/settings regression tests without GUI dependencies:

```powershell
$env:PYTHONPATH="src"
python -m unittest discover -s tests -v
```

Connect at **115200 8-N-1**, select the port, then click Connect. The tool queries `Info?`, `Config?` and `Status=n`. Use Start telemetry to send `Stream=1`; this does not enable any motor.

以 **115200 8-N-1** 连接设备。软件会查询版本、配置和状态。"开始遥测"只发送 `Stream=1`，不会使能电机。

## Keyboard Shortcuts / 键盘快捷键

| Shortcut | Action |
|---|---|
| `Ctrl+Shift+Space` | Emergency stop all motors |
| `Ctrl+[` | Toggle left control sidebar |
| `Ctrl+]` | Toggle right values sidebar |
| `Ctrl+1` / `Ctrl+2` / `Ctrl+3` | Switch control tabs (Motor / Motion / Device) |
| `Ctrl+Shift+1` / `Ctrl+Shift+2` | Switch plot tabs (Telemetry / Console) |
| `F11` | Toggle full-screen telemetry view |
| `Esc` | Exit full-screen view |

## Safety / 安全

Suspend the vehicle for first tests and use a current-limited supply. The GUI sends `StopAll` and `Stream=0` before a normal disconnect, but it cannot replace the hardware power switch.

首次测试必须悬空车轮并使用限流电源。GUI正常断开前会发送 `StopAll` 和 `Stream=0`，但不能替代硬件电源开关。

Version 0.3 stores settings only in a PC-side JSON file; it never writes MCU Flash.

0.3 版本只保存 PC 端 JSON，不写入 MCU Flash。固件协议基础说明见：

```text
MSPM0G3507_FreeRTOS/Docs/GUI配置软件使用说明_v0.1.0.md
```

## CI Build / CI 构建

GitHub Actions runs tests first, then builds downloadable Windows and Linux artifacts. It does not commit generated binaries back to `main`.
See `.github/workflows/build-mspm0-configurator.yml` for details.

GitHub Actions 会先运行测试，再构建可下载的 Windows 和 Linux 产物，不会把生成的二进制文件自动提交回 `main`。
详见 `.github/workflows/build-mspm0-configurator.yml`。
