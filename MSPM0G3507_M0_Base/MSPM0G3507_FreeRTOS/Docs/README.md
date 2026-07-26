# MSPM0G3507 M0 Base Project

这是从 `MSPM0G3507_Project_temp/MSPM0G3507_Project` 派生的通用 MSPM0G3507 + FreeRTOS 起始工程。

## 保留内容

- MSPM0G3507 器件、时钟和启动配置
- SysConfig 配置入口：`Config/empty.syscfg`
- FreeRTOS、OSAL、CMSIS 和 TI DriverLib 构建依赖
- Keil 工程和最小应用入口

## 已移除内容

本基础工程不包含电机驱动、编码器、LSM6DSR 陀螺仪/IMU、控制算法、BLE/JDY23、VOFA 和相关测试代码。后续项目可在此目录复制后按需添加。

## 打开工程

使用 Keil 打开：

`MSPM0G3507_FreeRTOS/keil/empty_LP_MSPM0G3507_nortos_keil.uvprojx`
