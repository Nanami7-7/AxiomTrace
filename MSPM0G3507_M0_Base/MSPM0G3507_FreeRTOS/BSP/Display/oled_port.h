/**
 * @file    oled_port.h
 * @brief   OLED 软件 I2C 端口层。
 * @details 提供本模块的基础功能实现。
 */
#ifndef OLED_PORT_H
#define OLED_PORT_H

#include <stdbool.h>
#include <stdint.h>

/* 7-bit I2C address. 0x3C is the common SSD1306 module address. */
#ifndef OLED_I2C_ADDRESS
#define OLED_I2C_ADDRESS (0x3CU)
#endif

/**
 * @brief   OLED 软件 I2C 端口层。
 * @note   使用时请遵循接口约束。
 */
void OLED_Port_Init(void);

/**
 * @brief   OLED 软件 I2C 端口层。
 * @param  data 参数说明。
 * @param  length 参数说明。
 * @return 返回处理结果。
 */
bool OLED_Port_Write(const uint8_t *data, uint16_t length);

/**
 * @brief   OLED 软件 I2C 端口层。
 * @param  addr7 参数说明。
 */
void OLED_Port_SetAddress(uint8_t addr7);

#endif /* OLED_PORT_H */