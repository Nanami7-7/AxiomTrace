# IMU 串口输出使用说明

## 1. 功能概述

当前固件使用 UART0 作为上位机/调试串口。输入菜单命令 `imu` 后，固件会持续通过 UART0 TX DMA 输出 IMU 快照。

本功能不新增独立 IMU 控制台，也不使用 UART1。UART1 如后续重新启用，应作为独立外部通信模块处理。

## 2. 使用方法

### 开始输出

在 UART0 发送一行：

```text
imu\r\n
```

固件收到命令后立即发送一帧，之后按 200 ms 周期持续发送。

### 停止输出

发送任意其他非空菜单命令即可停止，例如：

```text
help\r\n
```

停止后，命令本身按照普通菜单流程处理。

复位固件也会清除持续输出状态。

## 3. 输出格式

每帧为一行 CSV，固定 10 个字段：

```text
accel_x_g,accel_y_g,accel_z_g,gyro_x_dps,gyro_y_dps,gyro_z_dps,roll_deg,pitch_deg,yaw_deg,temperature_degC
```

示例：

```text
0.01234,-0.00321,0.99876,0.125,-0.230,0.041,1.20,-0.80,12.35,26.50
```

单位：

| 字段 | 单位 |
|---|---|
| `accel_x_g`、`accel_y_g`、`accel_z_g` | g |
| `gyro_x_dps`、`gyro_y_dps`、`gyro_z_dps` | deg/s |
| `roll_deg`、`pitch_deg`、`yaw_deg` | deg |
| `temperature_degC` | °C |

## 4. DMA 行为

实现位置：

```text
Application/Task/task_menu.c
```

核心调用：

```c
bsp_uart_send_dma((const uint8_t *)tx_buf, (uint16_t)len);
```

特性：

- 发送周期：`PRJ_IMU_STREAM_PERIOD_MS`，默认 200 ms；
- 发送缓冲区：`PRJ_IMU_SNAPSHOT_DMA_BUF_SIZE`；
- DMA 忙时丢弃本次快照；
- 不阻塞菜单任务；
- 不缓存多帧，避免控制系统恢复后出现过期数据突发发送；
- 其他任务仍可能使用 UART0，因此观测端应允许普通日志与 IMU 帧交错出现。

## 5. 配置项

配置文件：

```text
Config/project_config.h
```

相关宏：

```c
#define PRJ_IMU_STREAM_PERIOD_MS       (200U)
#define PRJ_IMU_SNAPSHOT_DMA_BUF_SIZE  (192U)
```

如果修改周期，应注意：

1. 周期过短会增加 UART DMA 占用；
2. 周期不能小于菜单任务实际调度能力；
3. 不应在控制闭环高负载或故障诊断期间盲目提高发送频率；
4. 帧长度必须小于 DMA 缓冲区和 UART DMA API 支持的最大长度。

## 6. 上位机解析建议

不要按每一行都必然是 IMU 数据处理，因为启动日志、菜单响应和其他诊断可能共享 UART0。建议：

1. 按行接收；
2. 过滤包含 9 个逗号的数字行；
3. 将 10 个字段转换为浮点数；
4. 检查数值范围和时间间隔；
5. 将异常行保存到原始日志，不直接丢弃。

## 7. 安全和限制

- `imu` 只读 IMU 共享快照，不控制电机；
- DMA 忙时丢帧不代表 IMU 采样停止；
- 输出值的更新时间由 IMU 任务更新频率决定，不等同于 200 ms 采样频率；
- 该输出不能替代硬件故障保护或实时控制通道。
