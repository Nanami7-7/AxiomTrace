# MSPM0G3507 A/B 双板控制工程

本仓库包含两个 MSPM0G3507 工程和一个 Windows 串口配置工具：

| 目录 | 作用 |
|---|---|
| `MSPM0G3507_Project/` | **Board A**：电机、编码器、IMU、ADC、循迹与控制算法 |
| `MSPM0G3507_M0_Base/` | **Board B**：OLED、按键、BLE、红外及 A/B 板通信网关 |
| `tools/mspm0_configurator/` | Board A UART0 串口配置、PID 调参与实时曲线工具 |

## 直接下载 Windows 工具

下载并运行：

[**mspm0-configurator.exe**](tools/release/mspm0-configurator.exe)

- 无需安装 Python；首次启动会稍慢几秒。
- Board A 调试串口：**UART0，PA10 TX / PA11 RX，115200，8-N-1**。
- 先选择串口并连接，再使用界面按钮或“串口终端”发送命令。
- 首次测试必须架空车轮，并准备可随时断开的电机电源。

> 配置工具连接的是 **Board A UART0 文本调试口**。Board B 蓝牙到 Board A 使用的是 A/B 二进制业务帧，不要把下面的文本命令直接当作 BLE 业务帧发送。

# Board A UART0 串口指令大全

## 基本规则

- 命令不区分大小写，例如 `Kp=0.8` 与 `kp=0.8` 等价。
- 一行只发一条命令，以 `CR`、`LF` 或 `CRLF` 结束。
- 电机编号为 `0~3`；未指定电机时使用最近一次 `Motor=n` 选中的电机。
- `Target` 只修改目标速度，**不会自动启动电机**；启动需再发 `Run`。
- 当前目标速度会限制在 `-800~800 RPM`，PID 参数会限制在 `-100~100`。
- `StopAll` 是最直接的全部停止命令；位置/角度运动可用 `abort` 紧急终止。

## 1. 查询与数据流

| 指令 | 示例 | 作用 |
|---|---|---|
| `Info?` 或 `Info` | `Info?` | 查询固件版本、协议版本、板卡、驱动、波特率等信息 |
| `Config?` 或 `Config` | `Config?` | 查询编码器、减速比、轮径、轮距、控制周期等配置 |
| `Status?` 或 `Status` | `Status?` | 查询当前选中电机状态 |
| `Status=n` | `Status=2` | 查询指定电机状态，`n=0~3`，并将其设为当前电机 |
| `Stream=1` 或 `Stream=on` | `Stream=1` | 开启 FireWater 实时遥测，不会启动电机 |
| `Stream=0` 或 `Stream=off` | `Stream=0` | 关闭实时遥测 |
| `Menu` | `Menu` | 在串口重新打印固件菜单/当前参数 |

## 2. 电机选择、速度环与启停

推荐的最小调速流程：

```text
Motor=0
Kp=0.8
Ki=0.3
Kd=0
Target=200
Run
Status?
```

| 指令 | 示例 | 作用 |
|---|---|---|
| `Motor=n` | `Motor=0` | 选择后续命令作用的电机，`n=0~3` |
| `Kp=x` | `Kp=0.8` | 设置当前电机速度环比例参数 |
| `Ki=x` | `Ki=0.3` | 设置当前电机速度环积分参数；当前实现按“每秒”积分 |
| `Kd=x` | `Kd=0` | 设置当前电机速度环微分参数 |
| `Target=x` | `Target=200` | 设置目标速度，单位 RPM，范围会限幅到 `-800~800` |
| `Run` | `Run` | 使用当前目标启动当前电机 |
| `Run=n` | `Run=2` | 选择并启动指定电机 |
| `Stop` | `Stop` | 停止当前电机 |
| `Stop=n` | `Stop=2` | 停止指定电机 |
| `StopAll` | `StopAll` | 立即停止全部电机，同时可中断 Sweep |

## 3. 前馈参数

| 指令 | 示例 | 作用 |
|---|---|---|
| `FFk=x` | `FFk=1.25` | 设置当前电机前馈斜率 |
| `FFb=x` | `FFb=18` | 设置当前电机前馈截距 |
| `FFe=1` | `FFe=1` | 开启前馈，重新启动电机后生效 |
| `FFe=0` | `FFe=0` | 关闭前馈，重新启动电机后生效 |
| `FFKp=x` | `FFKp=0.5` | 设置前馈模式下的比例修正参数 |
| `FFKi=x` | `FFKi=0.1` | 设置前馈模式下的积分修正参数 |
| `FFKd=x` | `FFKd=0` | 设置前馈模式下的微分修正参数 |

## 4. 位置环与角度环

| 指令 | 示例 | 作用 |
|---|---|---|
| `mode=speed` | `mode=speed` | 切换为速度控制模式 |
| `mode=position` | `mode=position` | 切换为位置控制模式 |
| `mode=angle` | `mode=angle` | 切换为角度控制模式 |
| `pos=脉冲,巡航RPM` | `pos=1000,200` | 位置运动；巡航速度必须大于 0 且不超过 500 RPM |
| `angle=角度,巡航RPM` | `angle=90,120` | 基于 IMU yaw 的角度运动；巡航速度必须大于 0 且不超过 500 RPM |
| `abort` | `abort` | 终止位置/角度规划并停止全部电机 |

`pos` 与 `angle` 会在需要时自动切换到对应模式。正式运行前仍建议先单独调好速度环。

## 5. 辨识、自动整定与扫频

> **危险：以下命令会让电机自动运动。必须架空车轮、使用限流电源，并先确认 `StopAll` 可用。**

| 指令 | 示例 | 作用 |
|---|---|---|
| `Step` | `Step` | 使用默认 PWM=300 执行阶跃辨识，并尝试更新 `FFk` |
| `Step=pwm` | `Step=250` | 使用指定 PWM 做阶跃辨识；有效值为 `1~500` |
| `Auto` | `Auto` | 阶跃辨识后按默认 5 Hz 带宽计算并应用 PI 参数 |
| `Auto=带宽Hz` | `Auto=3` | 使用指定闭环带宽自动整定；大于 0.5 Hz 才采用该值 |
| `Sweep` | `Sweep` | 执行约 40 秒自动扫频标定，并尝试更新前馈参数 |

阶跃或扫频过程中可以发送 `Stop`、`Stop=n` 或 `StopAll` 中断。

## 6. 常用复制粘贴示例

### 电机 0 运行 200 RPM

```text
Motor=0
Target=200
Run
```

### 查看全部 4 个电机状态

```text
Status=0
Status=1
Status=2
Status=3
```

### 安全停止并关闭遥测

```text
StopAll
Stream=0
```

### 位置控制示例

```text
Motor=0
mode=position
pos=1000,150
```

### 角度控制示例

```text
mode=angle
angle=90,120
```

# Board B UART0 BLE 调试命令

Board B 调试串口为 **UART0，PA10 TX / PA11 RX，9600，8-N-1**。它只负责把 AT 调试命令转发到 UART2 的 DX-BT311 蓝牙模块。

| 指令 | 作用 |
|---|---|
| `ble help` 或 `help` | 打印 BLE CLI 帮助 |
| `ble AT+ROLE` | 查询蓝牙角色 |
| `ble AT+ROLE0` | 切换为从机模式并自动重启 |
| `ble AT+NAMEONB` | 在从机模式设置名称为 ONB |
| `ble AT+LADDR` | 查询蓝牙 MAC 地址 |
| `ble AT+DISC` | 主机已连接时先断开连接 |
| `ble AT+RESET` | 重启蓝牙模块 |
| `AT+...` | 也可以省略 `ble ` 前缀，直接转发任意 AT 命令 |

已进入蓝牙透传连接时，普通 AT 命令可能无响应。此时先发送 `ble AT+DISC`，等待约 1 秒后再执行角色、名称等配置命令。

## 开发与构建

详细使用、源码运行、测试和打包方法见：

- [串口控制命令完整参考](串口控制命令参考.md)
- [外设 API 快速参考](外设API快速参考_v0.1.0.md)
- [MSPM0 Configurator 使用说明](tools/mspm0_configurator/README.md)
- [Board A GUI 配置软件说明](MSPM0G3507_Project/MSPM0G3507_FreeRTOS/Docs/GUI配置软件使用说明_v0.1.0.md)
- [Board A UART0 通信协议](MSPM0G3507_Project/MSPM0G3507_FreeRTOS/Docs/通信协议_v1.md)
