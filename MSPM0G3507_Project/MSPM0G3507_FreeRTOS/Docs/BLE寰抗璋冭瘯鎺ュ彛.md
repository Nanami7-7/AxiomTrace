# Board A BLE 循迹调试接口

## 1. 用途

该接口用于在没有移植完整蓝牙调参协议前，通过 UART1 透明串口查看循迹、IMU、速度环和串口状态，并修改少量循迹运行参数。

设计约束：

- UART1 为 `9600 8N1`；
- `PRJ_UART1_BLE_DEBUG_ENABLE=1` 时，UART1 BLE 调试与 AB 板协议互斥；
- 2 ms 控制任务只复制快照，字符串格式化和 UART 发送在低优先级任务执行；
- 不增加 Board A 通信看门狗；
- 不提供启动电机的 BLE 命令；
- 参数修改仅保存在 RAM，复位后恢复编译时默认值。

相关文件：

```text
Application/app_uart1_ble_debug.c
Application/app_uart1_ble_debug.h
Config/project_config.h
```

## 2. 最简单验证

BLE 模块工作在透明串口模式。手机或上位机连接 BLE 后，每条命令末尾发送回车或换行。

建议依次发送：

```text
HELP
STATUS
LOG OFF
VIEW CORE
LOG ONCE
VIEW ALL
PERIOD 500
LOG ON
```

正常启动时首先收到：

```text
#READY,ble_debug=2,baud=9600,period=500,view=ALL,map=L:D/R:A,use=HELP
```

如果一直收到 `#WAIT,no_snapshot`，说明控制任务尚未向调试模块更新快照，应检查控制任务是否已经创建和运行。

## 3. 命令

命令不区分大小写，首尾空格会被忽略；一条命令最长 95 个可见字符。

| 命令 | 作用 |
|---|---|
| `HELP` 或 `?` | 显示简短帮助。 |
| `STATUS` | 查看日志开关、周期、视图、快照、UART 和命令统计。 |
| `LOG ON` | 开启周期日志。 |
| `LOG OFF` | 关闭周期日志，命令应答仍保留。 |
| `LOG ONCE` | 按当前视图立即输出一页。 |
| `PERIOD 500` | 设置周期，范围 `300..5000 ms`。9600 波特率建议不低于 500 ms。 |
| `VIEW CORE` | 只看基本循迹、红外、轮速和输出。 |
| `VIEW MODEL` | 只看曲率、S 型速度规划、角速度前馈/反馈。 |
| `VIEW IMU` | 只看 IMU 和角速度闭环。 |
| `VIEW MOTOR` | 只看左右轮目标、实测、PID、输出、电流和母线电压。 |
| `VIEW SYS` | 只看快照延迟、UART 和命令错误统计。 |
| `VIEW ALL` | 每周期只发一页，按 CORE→MODEL→IMU→MOTOR→SYS 循环。 |
| `PARAM` | 查看当前循迹运行参数。 |
| `CFG` | 查看编译期模型、几何和限幅参数。 |
| `DEFAULT` | 循迹停止时恢复运行参数默认值。 |
| `SET SPEED 30` | 设置基础速度。 |
| `SET FWD 70` | 设置前进输出限制。 |
| `SET TURNMIN 8` | 设置最小转弯修正。 |
| `SET TURNMID 25` | 设置中等转弯修正。 |
| `SET TURNMAX 75` | 设置大转弯修正。 |
| `SET TURN90 85` | 设置直角弯修正。 |

`SET` 和 `DEFAULT` 在循迹运行时返回：

```text
#ERR,STOP_TRACK_FIRST
```

参数必须满足：

```text
0 <= 参数 <= PRJ_PLANNER_MAX_RPM
TURNMIN <= TURNMID <= TURNMAX <= TURN90
```

违反范围或顺序时返回：

```text
#ERR,BAD_PARAM_OR_ORDER
```

## 4. 日志页面

所有周期日志以 `LT,<页>,` 开头：

| 页 | 前缀 | 主要用途 |
|---|---|---|
| CORE | `LT,C` | 首选总览，确认红外输入、循迹状态、目标轮速、实测轮速和电机输出。 |
| MODEL | `LT,M` | 检查数学模型、曲率限速、S 型规划、陀螺仪反馈是否工作。 |
| IMU | `LT,I` | 检查原始 IMU、数据年龄、角速度参考和反馈。 |
| MOTOR | `LT,D` | 检查左右轮映射、速度误差、PID 修正、输出饱和、电流和母线电压。 |
| SYS | `LT,S` | 检查控制快照是否卡住、UART 是否溢出、命令和发送是否异常。 |

共同字段：

| 字段 | 含义 |
|---|---|
| `n` | 日志序号，用于发现漏行或任务停止。 |
| `t` | 控制快照时间，单位 ms。 |
| `lag` | 当前发送时刻与快照的差，单位 ms。持续大于 100 ms 表示快照陈旧。 |
| `why` | 当前最高优先级诊断结论。 |

### 4.1 CORE 字段

```text
LT,C,n=...,t=...,lag=...,run=...,why=...,st=...,bm=...,ir=...,e=...,lost=...,base=...,turn=...,tgt=L/R,rpm=L/R,out=L/R,en=L/R
```

- `run`：循迹是否运行；
- `st`：循迹状态机状态；
- `bm`：红外黑线位掩码；
- `ir`：按通道顺序展开的 0/1 状态；
- `e`：加权循迹误差；
- `lost`：连续丢线周期；
- `base`：基础目标转速；
- `turn`：差速转弯修正；
- `tgt`：循迹计算的左/右目标 RPM；
- `rpm`：左 D 电机、右 A 电机实测 RPM；
- `out`：左/右最终电机命令；
- `en`：左右电机使能状态。

### 4.2 MODEL 字段

- `on/valid`：模型是否启用、当前模型结果是否有效；
- `e/em`：离散误差和换算后的横向偏差（m）；
- `kr/k`：原始曲率和限幅后曲率（1/m）；
- `req/plan/acc`：请求基础速度、S 型规划速度和当前加速度；
- `yr/ym/ye`：目标角速度、实测角速度、角速度误差；
- `ff/fb`：几何前馈和陀螺仪反馈产生的差速修正；
- `turn`：最终差速修正。

### 4.3 IMU 字段

- `rawV`：共享状态中的原始 IMU 数据是否有效；
- `age`：IMU 数据年龄，单位 ms；
- `yaw/gz`：航向角和 Z 轴角速度；
- `ax/ay/az/an`：三轴加速度和模长；
- `iv/rateV`：模型 IMU 数据和角速度反馈是否有效；
- `yr/ym/ye/fb`：角速度参考、实测、误差和反馈修正。

### 4.4 MOTOR 字段

- `map=L:D/R:A`：固定提醒左轮对应 D/M4，右轮对应 A/M1；
- `cmd`：循迹模块给出的左右轮目标；
- `set`：实际写入速度控制器的目标；
- `rpm/err`：实测转速和速度误差；
- `pid`：速度 PID 修正；
- `out`：最终电机输出；
- `cur`：左右轮电流；
- `vbus`：母线电压（mV）；
- `sat`：输出是否达到命令上限的 95%。

### 4.5 SYS 字段

- `rx/ovf`：UART1 已收字节数和 RX 环形缓冲溢出数；
- `irq/ign`：UART1 中断数和未识别中断数；
- `cmd/cerr`：已处理命令数和命令错误数；
- `bad`：非可见字符计数；
- `lineovf`：命令行过长次数；
- `txerr`：UART 发送错误次数；
- `trunc`：日志格式化超过发送缓冲区的次数。

## 5. `why` 快速诊断

`why` 只显示当前优先级最高的问题，判断顺序如下：

| 值 | 含义 | 首先检查 |
|---|---|---|
| `TRACK_OFF` | 循迹未启动。 | 按键/菜单是否进入循迹模式。 |
| `SNAP_OLD` | 控制快照超过 100 ms 未更新。 | 控制任务是否阻塞、崩溃或调度异常。 |
| `IR_LOST` | 红外全白或已经进入丢线计数。 | 四/五路红外值、引脚、电平极性、传感器高度。 |
| `MODEL_BAD` | 模型开启但计算结果无效。 | `VIEW MODEL` 的偏差、曲率和几何参数。 |
| `IMU_NONE` | 模型需要 IMU，但尚无有效数据。 | IMU 初始化、SPI、任务和时间戳。 |
| `IMU_STALE` | IMU 数据年龄超过允许值。 | IMU 任务频率、SPI 超时、任务优先级。 |
| `MOTOR_OFF` | 左或右电机未使能。 | 电机电源、使能状态和模式切换。 |
| `OUT_SAT` | 左或右输出达到 95% 命令上限。 | 目标是否过高、负载、电池、电机方向和 PID。 |
| `OK` | 当前未发现上述明显问题。 | 再按现象选择 MODEL、IMU 或 MOTOR 页面。 |

## 6. 推荐排查流程

### 6.1 车不走

```text
VIEW CORE
LOG ONCE
VIEW MOTOR
LOG ONCE
```

依次看 `run`、`why`、`en`、`tgt`、`set`、`rpm`、`out` 和 `vbus`。

### 6.2 容易丢线或转弯慢

```text
VIEW CORE
PERIOD 500
LOG ON
```

先确认 `ir/bm/e/lost` 是否正确变化，再切到：

```text
VIEW MODEL
```

检查 `k`、`plan`、`yr`、`ym`、`ff` 和 `fb`。如果 `k` 已很大但 `turn` 仍小，检查转向限幅；如果 `turn` 足够但实测轮速跟不上，切到 MOTOR 页检查输出饱和和速度误差。

### 6.3 怀疑 IMU 反馈方向错误

```text
VIEW IMU
LOG ON
```

手动向左、向右转动车体，观察 `gz/ym` 的符号是否与 `yr` 定义一致。符号错误时优先核对 `LINE_TRACK_MODEL_GYRO_SIGN`，不要先盲调 PID。

### 6.4 怀疑日志本身异常

```text
VIEW SYS
LOG ONCE
STATUS
```

- `lag` 增长：控制快照停止更新；
- `ovf` 增长：BLE 下行太快或任务长期无法处理 RX；
- `trunc` 增长：日志字段异常放大或发送缓冲区不足；
- `txerr` 增长：UART 配置、接线或底层发送超时异常。

## 7. 注意事项

1. 9600 波特率理论有效载荷约 960 字节/秒。默认 500 ms 且 ALL 每次只发一页，避免持续拥塞。
2. `PERIOD 300` 适合短时观察单页；长期记录建议使用 500 ms 或更慢。
3. `CFG` 显示的是编译期模型参数，当前 `SET` 不修改这些宏。
4. 若需要恢复 AB 板通信，将 `PRJ_UART1_BLE_DEBUG_ENABLE` 改为 0 后重新编译。
5. 该调试接口没有心跳和看门狗，也不会因 BLE 断开自动改变电机状态。
