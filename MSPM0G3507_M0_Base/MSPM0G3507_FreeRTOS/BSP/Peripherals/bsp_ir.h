/**
 * @file    bsp_ir.h
 * @brief   WHEELTEC-C07A-LF04 四路红外巡线模块 BSP 接口 (M0_Base 版)。
 * @details
 * 4 路红外传感器直连 MSPM0G3507 GPIO, 不依赖 hal_gpio 抽象层。
 * GPIO 初始化在 BSP 层完成, 不修改 SysConfig 生成的 ti_msp_dl_config.h/c。
 *
 * 引脚分配：
 *   优先使用 SysConfig 生成的 IR_A17、IR_B18、IR_B19、IR_A16；
 *   当前工程对应 CH1=PA17、CH2=PB18、CH3=PB19、CH4=PA16。
 *
 * 电平逻辑:
 *   检测到黑线时输出低电平, 白色区域输出高电平。
 *   BSP_IR_LINE_ACTIVE_LOW=1 时内部自动翻转, 对外统一 1=黑线 0=白色。
 */
#ifndef BSP_IR_H
#define BSP_IR_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "ti_msp_dl_config.h"

/* ======================== 用户可配置宏 ======================== */

/** CH1 引脚：使用 SysConfig 的 IR_A17 */
#ifndef BSP_IR_CH1_IOMUX
#define BSP_IR_CH1_IOMUX          (IR_A17_IOMUX)
#endif
#ifndef BSP_IR_CH1_PORT
#define BSP_IR_CH1_PORT           (IR_A17_PORT)
#endif
#ifndef BSP_IR_CH1_PIN
#define BSP_IR_CH1_PIN            (IR_A17_PIN)
#endif

/** CH2 引脚：使用 SysConfig 的 IR_B18 */
#ifndef BSP_IR_CH2_IOMUX
#define BSP_IR_CH2_IOMUX          (IR_B18_IOMUX)
#endif
#ifndef BSP_IR_CH2_PORT
#define BSP_IR_CH2_PORT           (IR_B18_PORT)
#endif
#ifndef BSP_IR_CH2_PIN
#define BSP_IR_CH2_PIN            (IR_B18_PIN)
#endif

/** CH3 引脚：使用 SysConfig 的 IR_B19 */
#ifndef BSP_IR_CH3_IOMUX
#define BSP_IR_CH3_IOMUX          (IR_B19_IOMUX)
#endif
#ifndef BSP_IR_CH3_PORT
#define BSP_IR_CH3_PORT           (IR_B19_PORT)
#endif
#ifndef BSP_IR_CH3_PIN
#define BSP_IR_CH3_PIN            (IR_B19_PIN)
#endif

/** CH4 引脚：使用 SysConfig 的 IR_A16 */
#ifndef BSP_IR_CH4_IOMUX
#define BSP_IR_CH4_IOMUX          (IR_A16_IOMUX)
#endif
#ifndef BSP_IR_CH4_PORT
#define BSP_IR_CH4_PORT           (IR_A16_PORT)
#endif
#ifndef BSP_IR_CH4_PIN
#define BSP_IR_CH4_PIN            (IR_A16_PIN)
#endif

/**
 * @brief 黑线有效电平选择
 * @note  1 = 低电平表示黑线 (WHEELTEC-C07A-LF04 默认, 需翻转)
 *        0 = 高电平表示黑线
 */
#ifndef BSP_IR_LINE_ACTIVE_LOW
#define BSP_IR_LINE_ACTIVE_LOW    (1U)
#endif

/** 红外通道总数 */
#define BSP_IR_CHANNEL_COUNT      (4U)

/* ======================== 函数接口 ======================== */

/**
 * @brief 初始化 4 路红外巡线 GPIO (上拉输入)。
 * @note  DL_GPIO_initDigitalInputFeatures 已通过 IOMUX_PINCM_INENA_ENABLE
 *        使能输入, 无需额外调用 DL_GPIO_enableInput。
 */
void BSP_IR_Init(void);

/**
 * @brief 读取 4 路红外传感器状态。
 * @param state 输出缓冲区, 至少 4 字节。state[0]=CH1 ... state[3]=CH4。
 *              1=检测到黑线, 0=白色区域。
 */
void BSP_IR_Read(uint8_t state[4]);

/**
 * @brief 读取单通道红外状态。
 * @param channel 通道号 0-3 (对应 CH1-CH4)。
 * @return 1=黑线, 0=白色。
 */
uint8_t BSP_IR_GetChannel(uint8_t channel);

#ifdef __cplusplus
}
#endif

#endif /* BSP_IR_H */
