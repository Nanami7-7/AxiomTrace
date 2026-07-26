/**
 * @file    spi_bridge.h
 * @brief   LSM6DSR 硬件 SPI 桥接层接口
 *
 * 将 LSM6DSR 驱动层的 lsm6dsr_io_t 接口桥接到 MSPM0G3507 硬件 SPI。
 *
 * 硬件连接（通过宏定义，方便修改）：
 *   - SPI 实例：SPI1
 *   - SCK:  PB9  (SPI1_SCK)
 *   - MOSI: PA18 (SPI1_PICO)
 *   - MISO: PA16 (SPI1_POCI)
 *   - CS:   PA25 (独立 GPIO 输出，手动控制)
 *
 * SPI 协议（LSM6DSR 4-wire Mode 0）：
 *   - 读操作：CS↓ → [REG|0x80] → 排空RX → [DUMMY]×len → 读取 → CS↑
 *   - 写操作：CS↓ → [REG&0x7F] → 排空RX → [DATA]×len → 排空RX → CS↑
 *
 * 使用方式：
 * @code
 *   #include "spi_bridge.h"
 *
 *   // 初始化
 *   spi_bridge_init();
 *
 *   // 使用（通过 lsm6dsr_io_t 接口）
 *   extern lsm6dsr_io_t lsm6dsr_io_spi;
 *   lsm6dsr_io = lsm6dsr_io_spi;
 * @endcode
 */

#ifndef SPI_BRIDGE_H
#define SPI_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lsm6dsr.h"

/**
 * @brief LSM6DSR SPI I/O 实例
 *
 * 供 bsp_lsm6dsr.c 使用。
 */
extern lsm6dsr_io_t lsm6dsr_io_spi;

/**
 * @brief 初始化 SPI 桥接层
 *
 * 配置 CS 引脚为 GPIO 输出模式。
 * SPI 外设由 SysConfig 配置（ti_msp_dl_config.c）。
 *
 * @note 在 bsp_lsm6dsr_init() 之前调用
 */
void spi_bridge_init(void);

#ifdef __cplusplus
}
#endif

#endif /* SPI_BRIDGE_H */
