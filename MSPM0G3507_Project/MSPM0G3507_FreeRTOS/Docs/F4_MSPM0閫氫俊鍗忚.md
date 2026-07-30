# STM32F4 上位机与 MSPM0 下位机通信协议

## 1. 角色与物理连接

- STM32F4：上位机，使用 USART2。
- MSPM0G3507：下位机，使用 UART1。
- MSPM0 UART1：`PB6=TX`、`PB7=RX`，UART0 继续保留给调试。
- 连接：`F4 PD5(TX) -> MSPM0 PB7(RX)`，`F4 PD6(RX) <- MSPM0 PB6(TX)`，两块板必须共地。
- 串口参数：`921600，8 数据位，无校验，1 停止位（8N1）`。

当前 MSPM0 代码通过 `project_config.h` 在 UART1 初始化后重新设置 921600 分频，不修改 SysConfig 生成文件。

## 2. 固定帧格式

所有多字节整数均为小端序。完整帧如下：

| 字段 | 长度 | 说明 |
|---|---:|---|
| SOF | 2 | `0xA5 0x5A` |
| Version | 1 | 当前为 `0x01` |
| MessageID | 1 | 消息类型 |
| PayloadLen | 2 | Payload 字节数，最大 256 |
| Sequence | 2 | 发送端独立递增序号，允许回绕 |
| Timestamp | 4 | 发送端时间戳，单位 us |
| Payload | N | 业务数据 |
| CRC16 | 2 | CRC-16/CCITT-FALSE，小端序 |

- 固定头（含 SOF）为 12 字节。
- 完整帧长度为 `14 + PayloadLen`，最大 270 字节。
- CRC 覆盖 `Version` 到 `Payload`，不覆盖 SOF，也不覆盖 CRC 字段。
- CRC 参数：多项式 `0x1021`、初值 `0xFFFF`、非反射、无异或输出。
- 接收端遇到错误长度、错误版本或 CRC 错误时，只增加统计，不执行命令。
- 解析器支持 SOF 重同步和单帧接收超时。

## 3. 业务消息

### 3.1 `0x10 MSG_CHASSIS_CMD`（F4 -> MSPM0）

Payload 长度至少 2 字节：

| 偏移 | 类型 | 说明 |
|---:|---|---|
| 0 | u8 | command |
| 1 | u8 | reason，当前版本记录但不解释 |

command：

- `0`：STOP，调用统一电机停车接口，清除普通协议故障。
- `1`：START，只解除通信侧的普通停止/链路故障状态，不凭空生成速度目标；没有已有控制目标时不会自动驱动电机。
- `2`：ESTOP，停车并关闭电机功率，故障锁存到复位。

### 3.2 `0x11 MSG_CHASSIS_STATUS`（MSPM0 -> F4）

Payload 固定 5 字节：

```text
byte 0     state                 u8
byte 1..2  fault_code            u16 LE
byte 3..4  last_command_seq      u16 LE
```

state：`0=IDLE`、`1=RUNNING`、`2=FAULT`。

fault_code：`0=NONE`、`1=LINK_TIMEOUT`、`2=ESTOP`。

### 3.3 `0x12 MSG_CHASSIS_IMU`（MSPM0 -> F4）

Payload 固定 13 字节：

```text
byte 0..3    ax_mm_s2    i32 LE
byte 4..7    ay_mm_s2    i32 LE
byte 8..11   az_mm_s2    i32 LE
byte 12      quality     u8
```

加速度换算：`accel_g * 9806.65 = mm/s²`。当前 `quality=0xFF` 表示共享 IMU 时间戳有效，`0` 表示无有效采样标志。

### 3.4 `0x13 MSG_CHASSIS_HEARTBEAT`（双向）

Payload 长度为 0。MSPM0 默认每 50 ms 发送一次心跳。收到合法 F4 帧都会刷新链路计时。

## 4. 周期与安全策略

默认配置位于：

```text
MSPM0G3507_FreeRTOS/Config/project_config.h
```

```c
PRJ_F4_PROTOCOL_ENABLE        // 1：启用 F4 固定帧；0：保留旧 COBS 协议
PRJ_F4_LINK_TIMEOUT_MS        // 默认 250 ms，合法帧超时后停车
PRJ_F4_STATUS_PERIOD_MS       // 默认 100 ms
PRJ_F4_HEARTBEAT_PERIOD_MS    // 默认 50 ms
PRJ_F4_IMU_PERIOD_MS          // 默认 50 ms
```

链路超时只在已经收到过至少一帧合法 F4 帧后生效，避免上电时无上位机导致重复打印或误动作。超时后只触发一次停车；重新发送 START 可清除普通链路故障，但不能解除 ESTOP 锁存。

## 5. 示例帧

下面示例只说明字段布局；CRC 需要按实际 Sequence 和 Timestamp 重新计算。

### F4 发送 STOP

```text
A5 5A 01 10 02 00 01 00 tt tt tt tt 00 00 crc_lo crc_hi
```

其中 Payload 为 `00 00`，Sequence 为 `0x0001`。

### F4 发送 START

```text
A5 5A 01 10 02 00 02 00 tt tt tt tt 01 00 crc_lo crc_hi
```

### MSPM0 发送心跳

```text
A5 5A 01 13 00 00 ss ss tt tt tt tt crc_lo crc_hi
```

## 6. 代码接入点

- UART1 收字节：`Protocol/Transport/proto_uart1_a.c`，ISR 只写环形缓冲。
- 协议任务：`Application/app_protocol_a.c`，宏开启时不再让旧 COBS parser 消费 UART1。
- F4 业务：`Application/app_f4_protocol.c/.h`。
- 旧 COBS 文件保留，关闭 `PRJ_F4_PROTOCOL_ENABLE` 即可恢复旧路径。
- 不在中断中解析、不使用动态内存、不修改测速、PID、角度环、位置环和按键逻辑。

## 7. 联调顺序

1. 先确认 F4 和 MSPM0 共地，TX/RX 交叉连接。
2. 两端都设置 921600 8N1。
3. F4 周期发送 `0x13` 心跳，观察 MSPM0 是否回发心跳、状态和 IMU 帧。
4. 检查 CRC、Sequence 和 Timestamp。
5. 发送 STOP，确认所有电机目标清零。
6. 空载发送 ESTOP，确认功率关闭；复位 MSPM0 后再恢复。
7. 暂时不要用 START 作为速度控制命令；第一阶段协议没有定义目标速度、位置或角度参数。

后续若需要远程速度/位置/角度控制，应新增独立 MessageID 和明确的单位、范围、超时及急停语义，不要复用 `0x10` 的启停字段。