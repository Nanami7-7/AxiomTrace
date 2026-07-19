# Firmware integrations / 固件集成

This directory contains target-specific firmware projects that consume or integrate with AxiomTrace while remaining isolated from the host library, tools, tests, and root build system.

本目录用于存放与 AxiomTrace 配套的目标板固件。固件工程与主仓库的库、工具、测试及根目录构建系统保持隔离，避免无关 Git 历史或板级工具链污染主项目结构。

## MSPM0G3507 motor controller

- Path: [`MSPM0G3507_Project/`](MSPM0G3507_Project/)
- Source branch: `codex/motor-dual-backend`
- Source snapshot: `20cba056ab456b5768d9d1326a37e3caf62f6cae`
- Firmware release base: `CXY` / `v0.1.0`
- Default motor backend: DRV8870
- Compatibility backend: TB6612FNG

The firmware is imported as a content snapshot under this subdirectory because the firmware branch and `main` have unrelated Git histories. No `--allow-unrelated-histories` merge is used, and the existing AxiomTrace root layout is not overwritten.

由于固件分支与 `main` 没有共同的 Git 历史，本次采用独立子目录内容快照集成，不使用 `--allow-unrelated-histories`，也不会覆盖 AxiomTrace 主工程的根目录结构。

## Entry points / 使用入口

- Firmware overview: [`MSPM0G3507_Project/README.md`](MSPM0G3507_Project/README.md)
- Architecture directory map: [`MSPM0G3507_Project/MSPM0G3507_FreeRTOS/README_ARCH.md`](MSPM0G3507_Project/MSPM0G3507_FreeRTOS/README_ARCH.md)
- Release notes: [`MSPM0G3507_Project/RELEASE_NOTES_v0.1.0.md`](MSPM0G3507_Project/RELEASE_NOTES_v0.1.0.md)
- Keil project: `MSPM0G3507_Project/MSPM0G3507_FreeRTOS/keil/empty_LP_MSPM0G3507_nortos_keil.uvprojx`
- Motor backend guide: `MSPM0G3507_Project/MSPM0G3507_FreeRTOS/Docs/电机驱动双后端分层与切换指南.md`
- GUI tool source: [`MSPM0G3507_Project/tools/mspm0_configurator/`](MSPM0G3507_Project/tools/mspm0_configurator/)
- GUI pre-built release: [`MSPM0G3507_Project/tools/release/`](MSPM0G3507_Project/tools/release/)

## GUI tool / GUI 工具

The MSPM0 Configurator is a PySide6 bilingual configuration and real-time plotting tool.

MSPM0 Configurator 是基于 PySide6 的中英双语配置与实时绘图工具。

### Run pre-built executable / 运行预编译可执行文件

```text
MSPM0G3507_Project/tools/release/mspm0-configurator/mspm0-configurator.exe
```

### Build locally / 本地构建

```powershell
cd MSPM0G3507_Project\tools\mspm0_configurator
python build_release.py --clean
```

### CI build / CI 构建

GitHub Actions automatically builds Windows and Linux executables on push to `main`.
See [`.github/workflows/build-mspm0-configurator.yml`](../.github/workflows/build-mspm0-configurator.yml).

GitHub Actions 会在 push 到 `main` 时自动构建 Windows 和 Linux 可执行文件。
详见 [`.github/workflows/build-mspm0-configurator.yml`](../.github/workflows/build-mspm0-configurator.yml)。

## Safety status / 安全状态

- DRV8870 production and FactoryTest targets have passed full Keil rebuilds with zero errors and zero warnings.
- TB6612FNG has passed source-level and compile-gate verification, but has not been validated on the current hardware.
- DRV8870 remains the default backend. Do not switch to TB6612FNG until a matching SysConfig board configuration and current-limited hardware test are completed.