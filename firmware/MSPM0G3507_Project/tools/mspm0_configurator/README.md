# MSPM0 Configurator 0.1.0 / MSPM0 配置工具 0.1.0

PySide6 bilingual configuration, command console and real-time plotting tool for the MSPM0G3507 four-motor controller.

基于 PySide6 的中英双语四电机配置、串口命令和实时绘图工具。

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
python build_release.py --clean
# Output: tools/release/mspm0-configurator/
```

Connect at **115200 8-N-1**, select the port, then click Connect. The tool queries `Info?`, `Config?` and `Status=n`. Use Start telemetry to send `Stream=1`; this does not enable any motor.

以 **115200 8-N-1** 连接设备。软件会查询版本、配置和状态。"开始遥测"只发送 `Stream=1`，不会使能电机。

## Safety / 安全

Suspend the vehicle for first tests and use a current-limited supply. The GUI sends `StopAll` and `Stream=0` before a normal disconnect, but it cannot replace the hardware power switch.

首次测试必须悬空车轮并使用限流电源。GUI正常断开前会发送 `StopAll` 和 `Stream=0`，但不能替代硬件电源开关。

Version 0.1 stores settings only in a PC-side JSON file; it never writes MCU Flash.

0.1版本只保存PC端JSON，不写入MCU Flash。完整说明见：

```text
MSPM0G3507_FreeRTOS/Docs/GUI配置软件使用说明_v0.1.0.md
```

## CI Build / CI 构建

GitHub Actions automatically builds Windows and Linux executables on push to `main`.
See `.github/workflows/build-mspm0-configurator.yml` for details.

GitHub Actions 会在 push 到 `main` 时自动构建 Windows 和 Linux 可执行文件。
详见 `.github/workflows/build-mspm0-configurator.yml`。