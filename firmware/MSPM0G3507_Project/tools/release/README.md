# MSPM0 Configurator Release / 发布产物

This directory contains pre-built standalone executables of the MSPM0 Configurator GUI tool.

本目录存放 MSPM0 Configurator GUI 工具的预编译独立可执行文件。

## Build Sources / 构建来源

- **Local build**: Run `python build_release.py` in `tools/mspm0_configurator/`
- **CI build**: Automatically built by GitHub Actions on push to `main`

## Directory Layout / 目录结构

```
tools/release/
├── README.md                        # This file
├── RELEASE_INFO.txt                # Build metadata (generated)
├── mspm0-configurator/              # PyInstaller onedir output
│   ├── mspm0-configurator.exe       # Windows executable
│   └── _internal/                   # Bundled libraries
├── mspm0-configurator-windows.zip   # Windows archive (CI generated)
└── mspm0-configurator-linux.tar.gz  # Linux archive (CI generated)
```

## Usage / 使用方法

### Windows
1. Download `mspm0-configurator-windows.zip`
2. Extract to any directory
3. Run `mspm0-configurator/mspm0-configurator.exe`

### Linux
1. Download `mspm0-configurator-linux.tar.gz`
2. Extract: `tar xzf mspm0-configurator-linux.tar.gz`
3. Run: `./mspm0-configurator/mspm0-configurator`

## Connect / 连接

- Baud rate: 115200
- Format: 8-N-1
- The tool queries `Info?`, `Config?` and `Status=n` on connect
- Use "Start telemetry" to send `Stream=1` (does not enable motors)

## Safety / 安全

Suspend the vehicle for first tests and use a current-limited supply.
The GUI sends `StopAll` and `Stream=0` before a normal disconnect, but it
cannot replace the hardware power switch.

首次测试必须悬空车轮并使用限流电源。
GUI 正常断开前会发送 `StopAll` 和 `Stream=0`，但不能替代硬件电源开关。