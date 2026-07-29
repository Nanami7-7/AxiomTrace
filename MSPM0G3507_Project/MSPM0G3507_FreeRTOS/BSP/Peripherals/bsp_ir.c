/**
 * @file    bsp_ir.c
 * @brief   四路数字红外巡线传感器 BSP 实现。
 */
#include "bsp_ir.h"
#include <stddef.h>

/* 单路红外输入的硬件信息。 */
typedef struct {
    uint32_t iomux;
    GPIO_Regs *port;
    uint32_t pin;
} bsp_ir_channel_t;

/* 通道顺序固定为 CH1、CH2、CH3、CH4。 */
static const bsp_ir_channel_t s_ir_channels[BSP_IR_CHANNEL_COUNT] = {
    { BSP_IR_CH1_IOMUX, BSP_IR_CH1_PORT, BSP_IR_CH1_PIN },
    { BSP_IR_CH2_IOMUX, BSP_IR_CH2_PORT, BSP_IR_CH2_PIN },
    { BSP_IR_CH3_IOMUX, BSP_IR_CH3_PORT, BSP_IR_CH3_PIN },
    { BSP_IR_CH4_IOMUX, BSP_IR_CH4_PORT, BSP_IR_CH4_PIN },
};

void BSP_IR_Init(void)
{
    uint8_t i;

    for (i = 0U; i < BSP_IR_CHANNEL_COUNT; i++) {
        DL_GPIO_initDigitalInputFeatures(
            s_ir_channels[i].iomux,
            DL_GPIO_INVERSION_DISABLE,
            DL_GPIO_RESISTOR_PULL_UP,
            DL_GPIO_HYSTERESIS_DISABLE,
            DL_GPIO_WAKEUP_DISABLE);
    }
}

void BSP_IR_Read(uint8_t state[BSP_IR_CHANNEL_COUNT])
{
    uint8_t i;
    uint8_t raw;

    if (state == NULL) {
        return;
    }

    for (i = 0U; i < BSP_IR_CHANNEL_COUNT; i++) {
        raw = (DL_GPIO_readPins(s_ir_channels[i].port,
                                s_ir_channels[i].pin) != 0U) ? 1U : 0U;
#if (BSP_IR_LINE_ACTIVE_LOW != 0U)
        /* 默认模块为低电平有效，翻转为 1=黑线、0=白色。 */
        state[i] = (uint8_t)(raw ^ 1U);
#else
        state[i] = raw;
#endif
    }
}

uint8_t BSP_IR_GetChannel(uint8_t channel)
{
    uint8_t state[BSP_IR_CHANNEL_COUNT];

    if (channel >= BSP_IR_CHANNEL_COUNT) {
        return 0U;
    }

    BSP_IR_Read(state);
    return state[channel];
}