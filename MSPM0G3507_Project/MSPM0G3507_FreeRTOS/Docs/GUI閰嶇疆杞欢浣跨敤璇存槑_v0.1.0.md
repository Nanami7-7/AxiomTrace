# GUI 配置软件使用说明 v0.1.0

工具路径：`tools/mspm0_configurator/`
适用固件：`fw=0.1.0`、`proto=1`。

## 1. 功能与边界

MSPM0 Configurator 是基于 PySide6 的中英双语串口工具，用于：

- 查询固件、协议、机械配置和电机状态；
- 设置当前电机的 Kp/Ki/Kd、目标 RPM 和前馈参数；
- 发送 Run、Stop、StopAll、位置和角度控制命令；
- 接收 11 通道 FireWater 遥测并绘制四路实际 RPM 与四路目标 RPM；
- 导出完整 11 通道 CSV；
- 通过原始终端发送协议命令；
- 把 GUI 参数保存为本机 JSON 文件。

**0.1 不会把参数写入 MCU Flash。**“保存配置”只保存 PC 文件，固件重启后仍使用编译期默认值或本次上电期间收到的命令值。

## 2. 安装

要求 Windows、Python 3.10 或更高版本。

```powershell
cd D:\msp_project\temp2\MSPM0G3507_Project\tools\mspm0_configurator
python -m venv .venv
.venv\Scripts\python.exe -m pip install --upgrade pip
.venv\Scripts\python.exe -m pip install -e .
```

启动：

```powershell
.venv\Scripts\python.exe -m mspm0_configurator
```

也可在激活虚拟环境后运行：

```powershell
mspm0-configurator
```

## 3. 连接设备

1. 烧录生产固件；
2. 用串口工具确认 UART0 为 PA10 TX、PA11 RX、115200 8-N-1；
3. 打开 GUI，选择中文或 English；
4. 单击“刷新”，选择对应 COM 口；
5. 波特率保持 115200；
6. 单击“连接”。

连接成功后软件自动发送：

```text
Info?
Config?
Status=当前电机
```

设备页应出现 `@INFO`、`@CONFIG` 和 `@STATUS`。若 `fw`不是`0.1.0`或`proto`不是`1`，不要直接运行电机，应先核对固件。

## 4. 电机设置与运行

电机下拉框映射：

| GUI | 物理电机 | 编码器位置 |
|---|---|---|
| A | M1 | RB |
| B | M2 | RF |
| C | M3 | LF |
| D | M4 | LB |

推荐步骤：

1. 悬空四轮并把电源设置为限流；
2. 选择一个电机；
3. 输入 Kp/Ki/Kd 和目标 RPM；
4. 单击“应用”，只下发参数，不使能电机；
5. 单击“运行”，确认对话框后才发送 `Run=n`；
6. 观察实际 RPM、目标 RPM 和机械方向；
7. 单击“停止”停止当前电机，或单击红色“全部急停”停止四路。

断开串口或关闭窗口时，GUI 会先排队发送：

```text
StopAll
Stream=0
```

但串口、MCU或供电异常时软件命令不构成最终安全保障，必须保留硬件电源开关。

## 5. 前馈、位置与角度

### 前馈

选择电机，填写 `k`、`b` 并选择是否使能，然后单击“应用”。GUI依次发送`Motor=n`、`FFk=x`、`FFb=x`、`FFe=0/1`。

### 位置

在“运动控制”页选择“位置”，填写编码器脉冲目标与巡航 RPM，单击“启动”。这会发送：

```text
mode=position
pos=<pulses>,<cruise_rpm>
```

### 角度

选择“角度”，填写车体旋转角度和巡航 RPM，单击“启动”：

```text
mode=angle
angle=<degrees>,<cruise_rpm>
```

“中止”发送`abort`。位置/角度依赖轮距、轮径、编码器计数和方向配置，未完成实车标定前只能做悬空或低速验证。

## 6. 遥测与绘图

单击“开始遥测”发送`Stream=1`。该命令只进入遥测循环，不使能电机。

图中显示：

- `rpm_a..rpm_d`：四路实测 RPM；
- `target_a..target_d`：四路目标 RPM。

CSV 还包含：

- `selected_ff`：当前选中电机的前馈量；
- `selected_pid_correction`：PID修正量；
- `selected_output`：最终有符号驱动命令。

“暂停”只暂停曲线刷新，后台仍接收并保存数据；“清空”清除内存数据并重置时间；“时间窗”可设为3～300秒；“导出CSV”使用UTF-8 BOM，Excel可直接打开。

## 7. JSON 配置

“保存配置”写入以下 PC 端字段：

```json
{
  "language": "zh",
  "baud": 115200,
  "motor": 0,
  "kp": 0.8,
  "ki": 0.3,
  "kd": 0.0,
  "target_rpm": 0.0,
  "ff_k": 0.0,
  "ff_b": 0.0,
  "ff_enabled": false,
  "plot_window_s": 15
}
```

“加载配置”只更新GUI控件，不会自动发送到电机；仍需单击“应用”。无效电机编号、非法语言、非有限浮点数和错误时间窗会被拒绝。

## 8. 常见问题

### 能连接但没有曲线

- 单击“开始遥测”；
- 确认收到`@ACK,cmd=stream,state=1`；
- 查看终端是否存在11个逗号分隔浮点值；
- 检查是否有大量调试日志与遥测交织；
- 确认固件协议版本为1。

### 端口列表为空

- 安装对应USB串口驱动；
- 关闭占用端口的 Keil、VOFA+ 或其他串口软件；
- 单击“刷新”；
- 确认已安装 `pyserial`。

### 电机方向或反馈相反

不要在GUI中用负PID参数掩盖安装错误。先停止并断电，核对 `Config/project_config.h` 中对应电机和编码器方向宏，重新构建和分轮验证。

### 点击断开后仍担心电机状态

GUI会尝试发送安全命令，但不能保证在USB掉线、MCU卡死或串口阻塞时送达。优先使用板载硬件总电源开关切断电机电源。
