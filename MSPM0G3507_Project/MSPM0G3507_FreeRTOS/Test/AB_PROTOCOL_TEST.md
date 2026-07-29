# AB 板通信最小测试

## 1. 连接关系

- Board A UART0：接电脑串口工具，只看调试打印。
- Board A UART1：连接 Board B UART1。
- Board B UART2：连接 BLE 模块。
- Board B BLE 名称：`ONB`。
- 两块板必须共地，UART1 使用交叉连接：`TX -> RX`、`RX -> TX`。

## 2. 上位机发送方式

BLE 连接后，使用 **HEX/RAW 二进制发送模式**，不要发送 ASCII 文本。
每一帧都必须带最后的 `00` 分隔字节。

### 当前版本已取消 HEARTBEAT

Board B 不再主动向 Board A 发送 HEARTBEAT，Host 也不需要先发送 HEARTBEAT。
旧版本 HEARTBEAT 帧即使收到，也只做兼容解析，不会转发、不建立租约、不改变控制状态。

### QUERY_STATUS（无需 HEARTBEAT）

发送：

```text
08 01 01 01 20 02 01 02 01 01 03 03 80 00
```

其中：

- `01`：Host 地址；
- `20`：Board B 网关地址；
- `01 01`：QUERY 类别、QUERY_STATUS 操作码；
- 最后一个 `00`：COBS 帧结束分隔符。

Board B 收到后会重新组帧，把地址改为 `Board B -> Board A`，再从 UART1 发送给 Board A。
当前版本直接发送 QUERY/COMMAND，不需要先发送 HEARTBEAT。

## 3. Board A 预期打印

Board A UART0 应看到类似信息：

```text
[AB TEST] src=0x20 dst=0x10 class=0x02 opcode=0x01 seq=1 len=0
```

实际协议分发器还会继续处理该帧，并通过 Board B 返回 ACK/STATUS。

## 4. 测试代码位置

- `Test/test_ab_protocol.h`：最小测试接口。
- `Test/test_ab_protocol.c`：收到完整帧后打印字段和前 16 字节 payload。
- `Application/app_protocol_a.c`：在正式分发前调用测试打印函数。

测试函数不修改协议状态机，不参与电机控制，也不在中断中打印。

## 5. 断开安全行为

取消 HEARTBEAT 后，Board A 不会因为 200ms 没有协议帧自动进入 `LINK_LOST`。
如果 Board B 能检测到 BLE 物理断开并调用 `gateway_router_on_ble_disconnected()`，仍会执行：

1. `STOP_ALL`；
2. 等待约 40ms；
3. 必要时发送 `DISABLE`。

如果 Board B 掉电、复位或无法检测到 BLE 断开，则不会有 HEARTBEAT 超时兜底；这是取消 HEARTBEAT/watchdog 后必须接受的安全取舍。