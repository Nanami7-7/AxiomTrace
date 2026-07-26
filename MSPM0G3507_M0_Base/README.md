# MSPM0G3507 M0 Base Project

这是从 `MSPM0G3507_Project_temp/MSPM0G3507_Project` 复制出的通用 MSPM0G3507 + FreeRTOS 起始工程。

## 保留内容

- MSPM0G3507 器件、启动和时钟配置
- SysConfig 配置入口：`MSPM0G3507_FreeRTOS/Config/empty.syscfg`
- FreeRTOS、OSAL、CMSIS 和 TI DriverLib 构建依赖
- Keil 工程和最小应用入口

## 已移除内容

本工程不包含电机驱动、编码器、LSM6DSR 陀螺仪/IMU、控制算法、BLE/JDY23、VOFA 和相关测试代码。后续具体产品可以从本工程复制，再按需增加自己的外设和应用层。

## 打开工程

使用 Keil 打开：

`MSPM0G3507_FreeRTOS/keil/empty_LP_MSPM0G3507_nortos_keil.uvprojx`
