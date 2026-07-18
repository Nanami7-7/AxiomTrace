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
- Release notes: [`MSPM0G3507_Project/RELEASE_NOTES_v0.1.0.md`](MSPM0G3507_Project/RELEASE_NOTES_v0.1.0.md)
- Keil project: `MSPM0G3507_Project/MSPM0G3507_FreeRTOS/keil/empty_LP_MSPM0G3507_nortos_keil.uvprojx`
- Motor backend guide: `MSPM0G3507_Project/MSPM0G3507_FreeRTOS/Docs/电机驱动双后端分层与切换指南.md`

## Safety status / 安全状态

- DRV8870 production and FactoryTest targets have passed full Keil rebuilds with zero errors and zero warnings.
- TB6612FNG has passed source-level and compile-gate verification, but has not been validated on the current hardware.
- DRV8870 remains the default backend. Do not switch to TB6612FNG until a matching SysConfig board configuration and current-limited hardware test are completed.