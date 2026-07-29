/**
 * @file    bsp_ir.h
 * @brief   四路数字红外巡线传感器 BSP 接口。
 *
 * 说明：
 * 1. 红外引脚由 Config/empty.syscfg 中名为 IR 的 GPIO 组生成。
 * 2. GPIO 组内通道名保持 CH1、CH2、CH3、CH4 不变，后续只需在 SysConfig 中修改引脚。
 * 3. 默认低电平表示检测到黑线，对外统一为 1=黑线、0=白色。
 * 4. 本模块只读取传感器，不连接电机、不启动看门狗。
 */
#ifndef BSP_IR_H
#define BSP_IR_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "ti_msp_dl_config.h"

/* 红外输入通道数量。 */
#define BSP_IR_CHANNEL_COUNT    (4U)

/*
 * SysConfig 在四个通道属于同一端口时通常只生成 IR_PORT；
 * 如果后续改成跨端口，SysConfig 会生成 IR_CHx_PORT，下面的兼容写法也能继续使用。
 */
#ifndef BSP_IR_CH1_PORT
#ifdef IR_CH1_PORT
#define BSP_IR_CH1_PORT         (IR_CH1_PORT)
#else
#define BSP_IR_CH1_PORT         (IR_PORT)
#endif
#endif
#ifndef BSP_IR_CH2_PORT
#ifdef IR_CH2_PORT
#define BSP_IR_CH2_PORT         (IR_CH2_PORT)
#else
#define BSP_IR_CH2_PORT         (IR_PORT)
#endif
#endif
#ifndef BSP_IR_CH3_PORT
#ifdef IR_CH3_PORT
#define BSP_IR_CH3_PORT         (IR_CH3_PORT)
#else
#define BSP_IR_CH3_PORT         (IR_PORT)
#endif
#endif
#ifndef BSP_IR_CH4_PORT
#ifdef IR_CH4_PORT
#define BSP_IR_CH4_PORT         (IR_CH4_PORT)
#else
#define BSP_IR_CH4_PORT         (IR_PORT)
#endif
#endif

/* 以下三个宏由 SysConfig 根据 CH1~CH4 自动生成。 */
#ifndef BSP_IR_CH1_PIN
#define BSP_IR_CH1_PIN          (IR_CH1_PIN)
#endif
#ifndef BSP_IR_CH1_IOMUX
#define BSP_IR_CH1_IOMUX        (IR_CH1_IOMUX)
#endif
#ifndef BSP_IR_CH2_PIN
#define BSP_IR_CH2_PIN          (IR_CH2_PIN)
#endif
#ifndef BSP_IR_CH2_IOMUX
#define BSP_IR_CH2_IOMUX        (IR_CH2_IOMUX)
#endif
#ifndef BSP_IR_CH3_PIN
#define BSP_IR_CH3_PIN          (IR_CH3_PIN)
#endif
#ifndef BSP_IR_CH3_IOMUX
#define BSP_IR_CH3_IOMUX        (IR_CH3_IOMUX)
#endif
#ifndef BSP_IR_CH4_PIN
#define BSP_IR_CH4_PIN          (IR_CH4_PIN)
#endif
#ifndef BSP_IR_CH4_IOMUX
#define BSP_IR_CH4_IOMUX        (IR_CH4_IOMUX)
#endif

/* 1=低电平表示黑线；0=高电平表示黑线。 */
#ifndef BSP_IR_LINE_ACTIVE_LOW
#define BSP_IR_LINE_ACTIVE_LOW (1U)
#endif

/** 初始化四路红外输入。 */
void BSP_IR_Init(void);

/**
 * 读取四路红外状态。
 * state[0]~state[3] 对应 CH1~CH4，1=黑线、0=白色。
 */
void BSP_IR_Read(uint8_t state[BSP_IR_CHANNEL_COUNT]);

/** 读取单路红外状态，channel 范围为 0~3。 */
uint8_t BSP_IR_GetChannel(uint8_t channel);

#ifdef __cplusplus
}
#endif

#endif /* BSP_IR_H */