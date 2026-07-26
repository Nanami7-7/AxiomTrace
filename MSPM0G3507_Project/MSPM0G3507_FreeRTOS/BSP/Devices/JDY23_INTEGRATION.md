# JDY-23 BLE5.0 接入与联调说明

## 1. 模块边界

```text
Application/Task/task_menu.c
        |
Application/app_ble_service.c
        |
BSP/Devices/jdy23.c
        |
BSP/Peripherals/bsp_ble_uart.c
        |
BSP/Peripherals/hal_uart.c
        |
UART1: PB6(TX) / PB7(RX)
```

- `bsp_ble_uart`：只负责 UART1 字节流、RX 中断环形缓冲和轮询 TX。
- `jdy23`：只负责 AT 请求/响应与透明数据收发，不依赖 DriverLib 和 FreeRTOS。
- `app_ble_service`：绑定 transport、OSAL 时间和项目配置。
- `task_menu`：仅提供 UART0 调试命令，不把 BLE 输入转发给电机/VOFA 命令解析器。

## 2. 硬件连接

```text
MSPM0 PB6 / UART1_TX  -> JDY-23 RX
MSPM0 PB7 / UART1_RX  <- JDY-23 TX
MSPM0 GND             -- JDY-23 GND
JDY-23 VCC            -- 按所购模块底板/商品页标称供电
```

UART 信号应为 3.3 V 逻辑。VCC 允许范围必须以实际购买模块（裸模块或带稳压底板）的资料为准，不可只凭模块名称判断。

## 3. 当前串口配置

- 外设：UART1
- TX：PB6
- RX：PB7
- 波特率：9600
- 格式：8 数据位、无校验、1 停止位、无流控
- RX：FIFO + RX/RX-timeout 中断
- TX：第一版使用轮询发送，不占用 UART1 DMA
- 缓冲区：256 字节；满时丢弃新字节并累计 `rx_overflow`

配置来源：

- `Config/empty.syscfg`
- `Config/ti_msp_dl_config.c`
- `Config/ti_msp_dl_config.h`
- `Config/project_config.h`

## 4. UART0 联调命令

```text
ble help
ble status
ble probe
ble query VER
ble query STAT
ble inspect
ble action DISC CONFIRM
ble at AT+VER
ble atraw AT
ble send hello
ble rx
ble monitor on
ble monitor off
ble flush
```

说明：

- 商品页截图第一条字符不清晰，本工程按实测可用指令固定为 `AT+VER`，不是 `AT-VER`，也不是 `ATIVER`。
- 商品页注明所有 AT 指令必须追加 `\r\n`；`ble query`、`ble inspect`、`ble action` 和 `ble at` 均自动追加。
- `ble probe`：先发送 `AT+VER\r\n`，失败后发送 `AT\r\n`。
- `ble query <key>`：通过强类型指令表执行单个只读查询，同时打印原始响应、提取值和格式匹配状态。指令名大小写不敏感，也接受 `AT+VER` 形式。
- `ble inspect`：依次查询全部已知只读项，不会执行复位、断连或睡眠。
- `ble action <key> CONFIRM`：仅允许 `RST`、`DISC`、`SLEEP`，必须显式输入大写 `CONFIRM`。
- `ble at <cmd>`：原始扩展入口，默认追加 CRLF，适合验证尚未封装或带参数的指令。
- `ble atraw <cmd>`：不追加结束符，只用于不同批次固件兼容测试。
- `ble send <text>`：透明发送文本，不追加换行。
- `ble monitor on`：每 100 ms 从 BLE RX 缓冲区读取并打印。
- AT 指令通常应在模块未处于透明连接时执行；连接后相同字节可能被当作透传数据。

### 4.1 已封装的商品页指令

下表响应均已在当前模块 `JDY-23-V81-02` 上通过 `ble inspect` 实测确认：

| Key | 实际发送 | 类型 | V81-02 实测响应 |
|---|---|---|---|
| `VER` | `AT+VER\r\n` | 查询 | `+VER:JDY-23-V81-02` |
| `RST` | `AT+RST\r\n` | 动作 | 未测试；可能在回复前复位 |
| `DISC` | `AT+DISC\r\n` | 动作 | 未测试；可能在回复前断连 |
| `STAT` | `AT+STAT\r\n` | 查询 | `+STAT:00` |
| `MAC` | `AT+MAC\r\n` | 查询 | `+MAC:4BB41C04360D` |
| `BAUD` | `AT+BAUD\r\n` | 查询 | `+BAUD:4` |
| `SLEEP` | `AT+SLEEP\r\n` | 动作 | 未测试；可能在回复前睡眠 |
| `NAME` | `AT+NAME\r\n` | 查询 | `+NAME:JDY-23` |
| `STARTEN` | `AT+STARTEN\r\n` | 查询 | `+STARTEN:1` |
| `ADVIN` | `AT+ADVIN\r\n` | 查询 | `+ADVIN:1` |
| `HOSTEN` | `AT+HOSTEN\r\n` | 查询 | `+HOSTEN:0` |
| `IBUUID` | `AT+IBUUID\r\n` | 查询 | `+IBUUID:FDA50693A4E24FB1AFCFC6EB07647825` |
| `MAJOR` | `AT+MAJOR\r\n` | 查询 | `+IBMAJOR:000A`，注意回复前缀含 `IB` |
| `MINOR` | `AT+MINOR\r\n` | 查询 | `+IBMINOR:0007`，注意回复前缀含 `IB` |

当前代码已据此将所有查询前缀标记为硬件已验证。`MAJOR` 和 `MINOR` 的请求仍分别是 `AT+MAJOR`、`AT+MINOR`，但回复必须按 `+IBMAJOR:`、`+IBMINOR:` 解析。

当前值的有限解释：

- `BAUD=4` 与当前可正常通信的 9600 bit/s 配置一致，因此可确认该模块/固件上代码 `4` 对应 9600；不要未经验证套用到其他固件批次。
- `ADVIN=1` 与商品页给出的 200 ms 默认广播间隔一致。
- `HOSTEN=0` 与商品页的从机模式默认值一致。
- `IBMAJOR=000A` 是十六进制 `0x000A`，即十进制 10；`IBMINOR=0007` 即十进制 7。
- `STAT=00`、`STARTEN=1` 的业务含义仍应以该批次完整手册为准；驱动当前只做无损读取，不擅自转换成连接/睡眠布尔语义。

### 4.2 输出示例

```text
BLE query MAJOR [AT+MAJOR]: OK (0)
  raw    : +IBMAJOR:000A\r\n
  value  : 000A
  format : PREFIX_MATCHED/VERIFIED
  prefix : +IBMAJOR: (hardware verified)
```

即使后续其他固件批次出现不同格式，原始响应仍然保留；前缀不匹配时输出 `RAW_FALLBACK`，不会丢失诊断信息。
## 5. 最小验收流程

1. 烧录 normal 或 factory target；UART0 仍按原配置连接调试终端。
2. 发送 `ble status`，确认 `initialized=YES`、映射为 PB6/PB7、波特率 9600。
3. 在 JDY-23 未连接手机时发送 `ble probe`。
4. 正常情况下应看到 `OK` 和版本响应；若超时，先检查 TX/RX 是否交叉、共地和模块供电。
5. 使用支持 BLE UART/自定义服务的手机工具扫描并连接 JDY-23。BLE 连接不一定表现为经典蓝牙 PIN 配对。
6. UART0 发送 `ble send hello`，手机端应收到 `hello`。
7. UART0 发送 `ble monitor on`，手机端发送 `world`，UART0 应打印 `[BLE RX 5] world`。
8. 发送 `ble status`，检查 `rx_bytes` 增长且 `rx_overflow=0`。

## 6. 故障判断

| 现象 | 优先检查 |
|---|---|
| `ble probe` 超时且 IRQ=0 | JDY-23 未供电、TX/RX 未交叉、PB6/PB7 配置或模块波特率不一致 |
| IRQ 增长但无有效响应 | 波特率/帧格式错误、线路噪声、模块固件的 AT 结束符不同 |
| `ble atraw` 可用、`ble at` 不可用 | 模块批次使用无结束符 AT 语法 |
| 手机能收到 MCU 数据，MCU 收不到手机数据 | JDY-23 TX 到 PB7 连接、PB7 复用、RX 中断配置 |
| `rx_overflow` 增长 | 上层读取过慢；关闭大量 UART0 打印或增大缓冲区 |
| 连接后 AT 无响应 | 模块已进入透明传输；断开 BLE 后再执行 AT |

## 7. 安全约束

- 当前 BLE RX 不进入电机控制命令解析器，这是有意的安全隔离。
- 后续如需 BLE 控车，应新增独立协议层，至少包含帧头、长度、CRC、命令白名单、超时失联停车和授权机制。
- 不要在未确认所购模块 VCC 范围时直接接入 5 V。
- 商品页或随货手册中的 AT 指令优先级高于网络上其他批次资料。

## 8. 引脚兼容性标记

- 当前默认电机后端是 DRV8870，PB6/PB7 可用于 UART1。
- 历史 TB6612 备用板级配置曾将 PB6/PB7 用作 D 路方向 GPIO。
- 因此启用 TB6612 兼容后端时，UART1 BLE 与 D 路方向控制不能共用这两个引脚；必须使用独立 SysConfig 配置重新分配其中一方，禁止仅靠宏切换后端。
- 商品详情页的 14 条基础 AT 指令已经集中封装在 `BSP/Devices/jdy23.c` 的只读元数据表中；未知或带参数的扩展指令仍可通过 `ble at <command>` / `ble atraw <command>` 验证。

## 9. 软件接口与职责

### 9.1 设备驱动层 `jdy23`

- `jdy23_find_command()`：将 `VER` 或 `AT+VER` 映射为强类型枚举。
- `jdy23_get_command_info()`：读取 AT 文本、命令类型、预期前缀和前缀是否经硬件确认。
- `jdy23_execute_command()`：统一追加 CRLF 并完成一次请求/响应事务。
- `jdy23_extract_response_value()`：优先按已知前缀提取值；不匹配时保留并提取原始首行。
- `jdy23_send_at()`：底层通用 AT 入口，保留给批次差异和带参数命令。

### 9.2 应用服务层 `app_ble_service`

- `app_ble_execute_command()`：隐藏全局 `jdy23_t` 实例，返回 `transfer_status`、`parse_status`、`raw_response`、`value` 和响应格式。
- `app_ble_find_command()` / `app_ble_get_command_info()`：为菜单或后续协议层提供受控查询，不暴露 UART/DriverLib。
- 当前服务不解析 BLE 透传数据为电机命令，继续保持通信链路与运动控制解耦。

### 9.3 当前实测基线与后续回归

当前 `JDY-23-V81-02` 的全部只读查询已经完成一次基线采集，正常回归时执行：

```text
ble inspect
```

验收标准：

- 11 条查询均返回 `OK (0)`；
- 所有查询均显示 `PREFIX_MATCHED/VERIFIED`；
- `MAJOR` 的解析值为 `000A`，不再包含 `+IBMAJOR:`；
- `MINOR` 的解析值为 `0007`，不再包含 `+IBMINOR:`；
- 不出现 `RAW_FALLBACK`、`TIMEOUT` 或 RX overflow。

若未来更换模块或固件后出现 `RAW_FALLBACK`，以 `raw` 字段为准建立新的批次适配，不要直接覆盖 V81-02 已验证基线。
