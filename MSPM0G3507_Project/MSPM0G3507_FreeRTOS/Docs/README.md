# 固件文档中心

当前工程：MSPM0G3507 FreeRTOS
当前重构基线：e62 固件整理

## 快速入口

| 文档 | 用途 |
|---|---|
| [IMU串口输出使用说明.md](IMU串口输出使用说明.md) | `imu` 命令、UART0 DMA 持续输出和数据字段定义 |
| [通信协议_v1.md](通信协议_v1.md) | UART0 菜单命令、VOFA+ 协议和响应约定 |
| [外设分类开发参考手册.md](外设分类开发参考手册.md) | SysConfig、GPIO、UART、PWM、电机、编码器、ADC、SPI、IMU 的配置与 API |
| [外设API快速参考_v0.1.0.md](外设API快速参考_v0.1.0.md) | 按外设查找配置、核心 API、参数和示例 |
| [公共接口参考_v0.1.0.md](公共接口参考_v0.1.0.md) | Application、BSP、HAL、OSAL 公共接口 |
| [架构与重构报告_v0.1.0.md](架构与重构报告_v0.1.0.md) | 模块边界、数据流、依赖和重构约束 |
| [DRV8870综合技术审查与安全重构方案.md](DRV8870综合技术审查与安全重构方案.md) | DRV8870 安全、PWM、功率门控和测试边界 |
| [电机驱动双后端分层与切换指南.md](电机驱动双后端分层与切换指南.md) | DRV8870/TB6612 双后端兼容和切换 |
| [Plans/e62_refactor_progress.md](Plans/e62_refactor_progress.md) | 本轮 e62 重构阶段状态、验证结果和剩余事项 |

## 当前目录约定

- 硬件生成配置：`Config/empty.syscfg` 和生成的 `ti_msp_dl_config.[ch]`
- 工程参数：`Config/project_config.h`
- 应用任务：`Application/Task/`
- 应用算法：`Application/Algorithm/`
- 输入设备：`BSP/Input/`；按键适配：`BSP/Peripherals/bsp_key.[ch]`
- 外设驱动：`BSP/Peripherals/`
- IMU 滤波实现：当前仍位于 `Application/Algorithm/Filter/`，由 Keil 工程统一登记
- RTOS、OSAL 和通用库：`Lib/`

## 维护规则

1. 引脚、外设实例、DMA 请求和中断优先在 `Config/empty.syscfg` 修改，重新生成后核对 `ti_msp_dl_config.[ch]`。
2. 周期、阈值、比例、功能开关等工程参数统一放在 `Config/project_config.h`。
3. UART0 DMA 发送必须考虑忙状态；控制路径不得因日志发送阻塞。
4. FactoryTest 代码和命令不得进入生产运行路径，使用 `PRJ_DRV8870_FACTORY_TEST_ENABLE` 隔离。
5. 删除模块后应执行 Keil Clean/Rebuild，确认旧目标文件不再参与构建；不要用 `git clean` 代替工程清理。
6. 对外协议、公共 API、构建结果和阶段计划需要同步更新文档。
