/**
 * @file    bsp_ir.h
 * @brief   四/五路数字红外巡线传感器 BSP 接口。
 *
 * 说明：
 * 1. 红外引脚由 Config/empty.syscfg 中名为 IR 的 GPIO 组生成。
 * 2. GPIO 组内通道名保持 CH1、CH2、CH3、CH4、CH5 不变，后续只需在 SysConfig 中修改引脚。
 * 3. 默认低电平表示检测到黑线，对外统一为 1=黑线、0=白色。
 * 4. 本模块只读取传感器，不连接电机、不启动看门狗。
 * 5. 通道数量由 BSP_IR_CHANNEL_COUNT 切换（4 或 5），五路板需 SysConfig 生成 IR_CH5_*。
 */
#ifndef BSP_IR_H
#define BSP_IR_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "ti_msp_dl_config.h"

/*
 * 红外输入通道数量（设备切换宏）。
 * 切换红外板只改这一处：4=四路红外板，5=五路红外板。
 * 选 5 时要求 SysConfig 已生成 IR_CH5_* 引脚定义（见 ti_msp_dl_config.h）。
 * 默认 5：当前工程已更换为五路红外板（中间三路跨度 48.9mm，总宽 96.5mm）。
 * 也可在 Keil 预处理选项中用 -DBSP_IR_CHANNEL_COUNT=4 强制回退到四路板。
 */
#ifndef BSP_IR_CHANNEL_COUNT
#define BSP_IR_CHANNEL_COUNT    (4U)
#endif

#if (BSP_IR_CHANNEL_COUNT != 4U) && (BSP_IR_CHANNEL_COUNT != 5U)
#error "BSP_IR_CHANNEL_COUNT 只支持 4 或 5"
#endif

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

/* 第五路仅在五路模式下使用；SysConfig 已生成 IR_CH5_* 定义。 */
#if (BSP_IR_CHANNEL_COUNT >= 5U)
#ifndef BSP_IR_CH5_PORT
#ifdef IR_CH5_PORT
#define BSP_IR_CH5_PORT         (IR_CH5_PORT)
#else
#define BSP_IR_CH5_PORT         (IR_PORT)
#endif
#endif
#ifndef BSP_IR_CH5_PIN
#define BSP_IR_CH5_PIN          (IR_CH5_PIN)
#endif
#ifndef BSP_IR_CH5_IOMUX
#define BSP_IR_CH5_IOMUX        (IR_CH5_IOMUX)
#endif
#if !defined(IR_CH5_PIN)
#error "BSP_IR_CHANNEL_COUNT=5 需要在 SysConfig 中生成 IR_CH5 引脚定义"
#endif
#endif /* BSP_IR_CHANNEL_COUNT >= 5U */

/* 1=低电平表示黑线；0=高电平表示黑线。 */
#ifndef BSP_IR_LINE_ACTIVE_LOW
#define BSP_IR_LINE_ACTIVE_LOW (1U)
#endif

/** 初始化红外输入（四路或五路，由 BSP_IR_CHANNEL_COUNT 决定）。 */
void BSP_IR_Init(void);

/**
 * 读取红外状态。
 * state[0]~state[BSP_IR_CHANNEL_COUNT-1] 对应 CH1~CHn，1=黑线、0=白色。
 */
void BSP_IR_Read(uint8_t state[BSP_IR_CHANNEL_COUNT]);

/** 读取单路红外状态，channel 范围为 0~BSP_IR_CHANNEL_COUNT-1。 */
uint8_t BSP_IR_GetChannel(uint8_t channel);

#ifdef __cplusplus
}
#endif

#endif /* BSP_IR_H */