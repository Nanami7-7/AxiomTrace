# 故障排除 / Troubleshooting Guide

## 目录 / Table of Contents

1. [电机不转 / Motor Not Running](#1-电机不转--motor-not-running)
2. [RPM 读数为 0 / RPM Reads Zero](#2-rpm-读数为-0--rpm-reads-zero)
3. [PID 震荡 / PID Oscillation](#3-pid-震荡--pid-oscillation)
4. [VOFA+ 无法连接 / VOFA+ Connection Issues](#4-vofa-无法连接--vofa-connection-issues)
5. [串口乱码 / Serial Garbled Output](#5-串口乱码--serial-garbled-output)
6. [NaN/Inf 警告 / NaN/Inf Warning](#6-naninf-警告--naninf-warning)
7. [系统卡死 / System Hang](#7-系统卡死--system-hang)

---

## 1. 电机不转 / Motor Not Running

### 检查清单 / Checklist

| # | 检查项 | 正常状态 | 异常处理 |
|---|---|---|---|
| 1 | 电机电源 | VM = 6~12V | 检查外部电源 |
| 2 | STBY 引脚 | 高电平 (VCC) | 接 VCC |
| 3 | PWM 输出 | 示波器可见 PWM 波形 | 检查 SysConfig Timer 配置 |
| 4 | IN1/IN2 接线 | 正确对应 | 参考 hardware_setup.md |
| 5 | motor_enabled | true | 菜单按 'd' 或 VOFA 发 Run |
| 6 | PID 输出 | 非 0 | 检查 setpoint 和 Kp/Ki |

### 诊断步骤 / Diagnostic Steps

1. **串口显示 `Motor X running` 但电机不转**：
   - 用万用表测量 PWM 引脚是否有 PWM 输出
   - 检查 IN1/IN2 是否有正确的高低电平
   - 检查 TB6612FNG 的 VM 电源是否正常

2. **PID 输出为 0**：
   - 确认 setpoint 不为 0（VOFA 发送 `Target=200`）
   - 确认 Kp 和 Ki 不为 0

---

## 2. RPM 读数为 0 / RPM Reads Zero

### 可能原因 / Possible Causes

| 原因 | 症状 | 解决 |
|---|---|---|
| 编码器未接线 | 所有电机 RPM = 0 | 检查编码器接线 |
| A/B 相接反 | RPM 符号相反或为 0 | 交换 A/B 相 |
| 编码器电源 | 无信号输出 | 检查 VCC/GND |
| 电机实际未转 | RPM = 0 是正确值 | 先确认电机在转 |
| 低于检测下限 | 低速时 RPM = 0 | M 法下限 ~23 RPM |

### 诊断 / Diagnosis

```
1. 手动转动电机，观察串口是否有 RPM 变化
2. 如果手动转动有 RPM → 电机驱动问题
3. 如果手动转动无 RPM → 编码器接线问题
```

---

## 3. PID 震荡 / PID Oscillation

### 症状 / Symptoms

- 实际 RPM 在目标值附近大幅波动
- 电机发出嗡嗡声
- VOFA+ 图表显示 RPM 波形震荡

### 解决方法 / Solutions

| 步骤 | 操作 |
|---|---|
| 1 | 减小 Kp（如从 0.8 减到 0.4） |
| 2 | 如果有积分震荡，减小 Ki |
| 3 | 如果有高频抖动，减小 Kd 或启用微分滤波 |
| 4 | 参考 [PID 参数整定指南](pid_controller.md#10-参数整定指南--parameter-tuning-guide) |

---

## 4. VOFA+ 无法连接 / VOFA+ Connection Issues

### 检查项 / Checklist

| # | 检查项 | 正确值 |
|---|---|---|
| 1 | 串口号 | 设备管理器中确认 COM 口 |
| 2 | 波特率 | 115200 |
| 3 | 帧格式 | FireWater |
| 4 | 系统状态 | RUNNING（菜单按 'd'） |
| 5 | 终端类型 | Serial（非 TCP） |

### 常见问题 / Common Issues

- **有数据但波形不对**：确认帧格式为 FireWater，不是 JustFloat
- **命令发送无响应**：确认系统在 RUNNING 状态
- **数据间断**：检查 USB 线缆质量，避免 USB Hub

---

## 5. 串口乱码 / Serial Garbled Output

### 原因与解决 / Causes & Solutions

| 原因 | 解决 |
|---|---|
| 波特率不匹配 | 设置为 115200 |
| 数据位/停止位错误 | 设置为 8N1 |
| 终端编码问题 | 使用 UTF-8 编码 |
| USB 转串口驱动 | 更新 CH340/CP2102 驱动 |

---

## 6. NaN/Inf 警告 / NaN/Inf Warning

### 症状 / Symptoms

串口输出：
```
Motor 0 NaN/Inf! set=200 fb=0
```

### 原因 / Causes

1. 编码器故障导致异常脉冲计数
2. 极端 PID 参数导致浮点溢出
3. 编码器接线松动

### 处理 / Resolution

系统会自动重置 PID 并继续运行。如果频繁出现：
1. 检查编码器接线
2. 降低 Kp/Ki 参数
3. 检查编码器供电是否稳定

---

## 7. 系统卡死 / System Hang

### 症状 / Symptoms

- LED 停止闪烁
- 串口无输出
- 按键无响应

### 可能原因 / Possible Causes

| 原因 | 说明 |
|---|---|
| 栈溢出 | 任务栈太小（检查 FreeRTOSConfig.h 栈溢出检测） |
| 死循环 | 代码逻辑错误 |
| 中断风暴 | 外设配置错误导致中断不断触发 |

### 诊断 / Diagnosis

1. 启用 FreeRTOS 栈溢出检测：`#define configCHECK_FOR_STACK_OVERFLOW 2`
2. 使用 Keil 调试器暂停执行，查看调用栈
3. 检查 HardFault 处理程序

---