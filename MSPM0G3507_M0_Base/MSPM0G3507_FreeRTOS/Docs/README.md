# MSPM0G3507 M0 Base Project

这是从 `MSPM0G3507_Project_temp/MSPM0G3507_Project` 派生的通用 MSPM0G3507 + FreeRTOS 起始工程。

## 保留内容

- MSPM0G3507 器件、时钟和启动配置
- SysConfig 配置入口：`Config/empty.syscfg`
- FreeRTOS、OSAL、CMSIS 和 TI DriverLib 构建依赖
- UART0 调试输出；UART1～UART3 保留为通用串口资源
- 软件 I2C OLED：SCL=PA0，SDA=PA1
- PA13 单按键输入（低电平有效）
- Keil 工程和最小应用入口

## 已移除内容

本基础工程不包含电机驱动、编码器、LSM6DSR 陀螺仪/IMU、控制算法、BLE/JDY23、VOFA、红外巡线和相关测试代码。后续项目可在此目录复制后按需添加。

## OLED 菜单操作

当前菜单只有一列 Mode：

- PA13 短按：切换到下一个 Mode
- PA13 长按：进入当前 Mode
- Mode 内再次长按：返回 Mode 列表

菜单实现位于：

- `Application/app_mode_menu.c`
- `Application/app_mode_menu.h`

新增或修改业务时，主要修改 `app_mode_menu.c` 底部的 `mode_status()`、`mode_test()`、`mode_custom()`，或者在 `s_modes[]` 中增加一个模式项。Mode 函数每次被调用时负责自己的业务处理和 OLED 绘制；菜单任务已经统一完成 `OLED_NewFrame()` 和 `OLED_ShowFrame()`，Mode 函数中不要再次调用这两个帧接口。

新增 Mode 的最小步骤：

1. 添加一个 `static void mode_xxx(void);` 声明。
2. 在 `s_modes[]` 添加 `{ "XXX", mode_xxx }`。
3. 实现 `static void mode_xxx(void)`，直接填写业务代码和显示代码。

模式名建议不超过 21 个 ASCII 字符，以适配当前 128x64 OLED 的一行显示宽度。

## 按键和显示移植点

- 按键扫描周期、消抖和长按时间：`Config/key_config.h`
- PA13 引脚宏：由 `Config/ti_msp_dl_config.h` 生成；默认回退定义位于 `Config/key_config.h`
- OLED 软件 I2C 引脚：由 SysConfig 生成 `IIC_SCL_*`/`IIC_SDA_*` 宏，OLED 端口层不硬编码引脚
- 按键底层适配：`BSP/Peripherals/bsp_key.c/.h`
- 通用按键状态机：`Lib/Key/key.c/.h`

## 打开工程

使用 Keil 打开：

`MSPM0G3507_FreeRTOS/keil/empty_LP_MSPM0G3507_nortos_keil.uvprojx`
