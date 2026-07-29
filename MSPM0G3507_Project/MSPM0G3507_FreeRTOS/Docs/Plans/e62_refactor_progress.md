# e62 重构阶段进度

更新时间：2026-07-27
工程目录：`MSPM0G3507_Project/MSPM0G3507_FreeRTOS`

## 总体结论

核心源码重构和 Filter 目录迁移已经完成，最新 IMU 持续 DMA 输出改动已经实现。已执行 Keil Clean/Rebuild：普通目标和 FactoryTest 目标均成功，且已移除 Objects/FactoryTest 中残留的 BLE/JDY-23、app_state_snapshot 和 app_imu_console 旧目标文件。当前准备提交本次目录整理与工程配置变更。

## 分阶段状态

| 阶段 | 状态 | 结果 |
|---|---|---|
| 阶段 0：工作区冻结与基线记录 | 部分完成 | 基线提交为 `ca6c2ff`；当前 `task_menu.c`、`project_config.h` 及文档仍未提交，因此尚未冻结 |
| 阶段 1：删除 BLE/JDY-23 | 源码完成 | `app_ble_service`、`bsp_ble_uart`、`jdy23` 源码和 Keil 登记已移除；旧编译产物已清理 |
| 阶段 2：精简 IMU 命令 | 代码完成，待最终提交 | `imu` 启动 UART0 DMA 持续输出，每 200 ms 一帧，每帧 10 个 CSV 字段；其他非空菜单命令停止输出 |
| 阶段 3：处理 `app_state_snapshot` | 已完成 | 状态快照已合并到应用共享上下文，独立源文件和旧目标文件已移除 |
| 阶段 4：合并配置文件 | 已完成 | `filter_param_defaults.h`、`filter_tuning.h`、`key_config.h` 已集中到 `Config/project_config.h` |
| 阶段 5：迁移 Key 目录 | 已完成 | 通用按键库位于 `BSP/Input`，BSP 适配位于 `BSP/Peripherals`，`Lib/Key` 已移除 |
| 阶段 6：全量编译与文档收敛 | 已完成 | 已补齐阶段文档和 IMU 使用文档；已执行 Clean/Rebuild，普通目标和 FactoryTest 目标均为 0 Error/0 Warning |

## 本次待提交变更

```text
Application/Algorithm/Filter/        Filter 算法目录迁移
Docs/                                文档路径同步
keil/empty_LP_MSPM0G3507_nortos_keil.uvprojx  工程路径和异常条目清理
```

未执行 git reset、git clean、强制覆盖、烧录或远端操作。

## IMU DMA 输出定义

命令：

```text
imu
```

输出字段顺序：

```text
accel_x_g, accel_y_g, accel_z_g,
gyro_x_dps, gyro_y_dps, gyro_z_dps,
roll_deg, pitch_deg, yaw_deg, temperature_degC
```

周期由以下配置控制：

```c
#define PRJ_IMU_STREAM_PERIOD_MS (200U)
```

发送接口为 UART0 TX DMA。DMA 忙时当前帧丢弃，不阻塞菜单任务，也不排队积压帧。发送其他非空菜单命令时停止持续输出。

## 已核验的目录与工程登记

- `app_ble_service.c/.h`、`bsp_ble_uart.c/.h`、`jdy23.c/.h`：源码不存在。
- `app_state_snapshot.c/.h`：源码不存在。
- `app_imu_console.c/.h`：源码不存在。
- `Lib/Key`：不存在。
- `Config/filter_param_defaults.h`、`Config/filter_tuning.h`、`Config/key_config.h`：不存在。
- Keil 工程未登记上述旧模块。
- `Application/Algorithm/Filter/` 目录迁移已完成，Keil 普通目标和 FactoryTest 目标均已同步登记。
- `BSP/Devices/` 空目录已移除，未影响编译。

## 构建验证

已执行：

```text
UV4 -c empty_LP_MSPM0G3507_nortos_keil.uvprojx
UV4 -b empty_LP_MSPM0G3507_nortos_keil.uvprojx -t empty_LP_MSPM0G3507_nortos_keil
UV4 -b empty_LP_MSPM0G3507_nortos_keil.uvprojx -t empty_LP_MSPM0G3507_drv8870_factory_test
```

普通目标：

```text
0 Error(s), 0 Warning(s)
Code=75088, RO-data=12280, RW-data=48, ZI-data=27360
```

FactoryTest 目标：

```text
0 Error(s), 0 Warning(s)
Code=106624, RO-data=13768, RW-data=48, ZI-data=27744
```

最终构建日志：

```text
keil/rebuild_codex_final_normal.log
keil/rebuild_codex_final_factory.log
```

最终日志中没有 `app_ble_service.c`、`bsp_ble_uart.c`、`jdy23.c`、`app_state_snapshot.c` 或 `app_imu_console.c` 的编译记录。

## 收尾动作

1. 已完成 Keil `Clean Targets` 和两个目标的 `Rebuild`。
2. 已确认最终构建日志不包含 BLE/JDY-23、`app_state_snapshot.c` 或 `app_imu_console.c` 的编译记录。
3. 已执行 `git diff --check`，无空白错误。
4. Filter 目录迁移、Keil 登记同步和文档路径收敛已完成；本次提交后完成阶段 0 冻结。
5. 后续如需调整滤波器实现或目录层级，应另行规划,避免与本次目录整理混合。
