/**
 * @file    bsp_ir.c
 * @brief   WHEELTEC-C07A-LF04 四路红外巡线模块 BSP 实现 (M0_Base 版)。
 * @details
 * 4 路红外传感器直连 GPIO, 上拉输入。DL_GPIO_initDigitalInputFeatures 已通过
 * IOMUX_PINCM_INENA_ENABLE 使能输入, 无需额外调用 DL_GPIO_enableInput。
 *
 * 电平逻辑: WHEELTEC-C07A-LF04 检测到黑线时输出低电平。
 * BSP_IR_LINE_ACTIVE_LOW=1 时内部翻转, 对外统一 1=黑线 0=白色。
 */
#include "bsp_ir.h"
#include "ti_msp_dl_config.h"

/* ======================== 内部通道表 ======================== */

/** 单通道硬件上下文 */
typedef struct {
    uint32_t iomux;  /**< IOMUX PINCM 宏 */
    GPIO_Regs *port; /**< GPIO 端口寄存器基址 */
    uint32_t pin;    /**< GPIO 引脚位掩码 */
} bsp_ir_channel_t;

/** 4 路红外通道硬件映射表 (顺序: CH1..CH4) */
static const bsp_ir_channel_t s_ir_channels[BSP_IR_CHANNEL_COUNT] = {
    { BSP_IR_CH1_IOMUX, BSP_IR_CH1_PORT, BSP_IR_CH1_PIN },
    { BSP_IR_CH2_IOMUX, BSP_IR_CH2_PORT, BSP_IR_CH2_PIN },
    { BSP_IR_CH3_IOMUX, BSP_IR_CH3_PORT, BSP_IR_CH3_PIN },
    { BSP_IR_CH4_IOMUX, BSP_IR_CH4_PORT, BSP_IR_CH4_PIN },
};

/* ======================== 公共函数实现 ======================== */

/**
 * @brief 初始化函数 BSP_IR_Init，完成对应模块的功能处理。
 * @return 函数执行结果。
 */
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

/**
 * @brief 读取函数 BSP_IR_Read，完成对应模块的功能处理。
 * @param state 函数参数 state。
 * @return 函数执行结果。
 */
void BSP_IR_Read(uint8_t state[4])
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
        /* 黑线=低电平, 翻转后 1=黑线, 0=白色 */
        state[i] = raw ^ 1U;
#else
        state[i] = raw;
#endif
    }
}

/**
 * @brief 获取函数 BSP_IR_GetChannel，完成对应模块的功能处理。
 * @param channel 函数参数 channel。
 * @return 函数执行结果。
 */
uint8_t BSP_IR_GetChannel(uint8_t channel)
{
    uint8_t state[4];

    if (channel >= BSP_IR_CHANNEL_COUNT) {
        return 0U;
    }

    BSP_IR_Read(state);
    return state[channel];
}
