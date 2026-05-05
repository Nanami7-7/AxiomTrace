#ifndef __BSP_ENCODER_H
#define __BSP_ENCODER_H

#include <stdint.h>
#include "ti_msp_dl_config.h"
/* 4个电机编号：左前、左后、右前、右后 */
typedef enum {
	ENCODER_LEFT_FRONT = 0,
	ENCODER_LEFT_BACK,
	ENCODER_RIGHT_FRONT,
	ENCODER_RIGHT_BACK,
	ENCODER_COUNT
} EncoderId;
void EncoderTask();
float Encoder_GetOmegaById(EncoderId id);
typedef struct {
	EncoderId id;//哪个编码器
	GPTIMER_Regs * INST;//哪个定时器
	//DL_TIMER_IIDX IIDX;//哪个中断
	GPIO_Regs* port;//编码器B相port
	uint32_t pin;//编码器B相pin
} EncoderInfo;
extern EncoderInfo LEFT_FRONT;
extern EncoderInfo LEFT_BACK;
extern EncoderInfo RIGHT_FRONT;
extern EncoderInfo RIGHT_BACK;
/*
 * 作用：初始化编码器模块。
 * 输入：无。
 * 输出：无。
 * 调用时机：系统初始化时调用一次。
 */
void Encoder_Init(void);
/*
 * 作用：把4个电机累计脉冲清零。
 * 输入：无。
 * 输出：无。
 */
void Encoder_ClearCount(void);
/*
 * 作用：读取单个电机累计脉冲（不清零）。
 * 输入：id=电机编号。
 * 输出：累计脉冲值。
 */
int32_t Encoder_GetCountById(EncoderId id);
/*
 * 作用：读取4个电机累计脉冲（不清零）。
 * 输入：count=长度为 ENCODER_COUNT 的数组。
 * 输出：count数组被填充。
 */
void Encoder_GetAllCount(int32_t count[ENCODER_COUNT]);
/*
 * 作用：读取并清零4个电机脉冲增量。
 * 输入：delta=长度为 ENCODER_COUNT 的数组。
 * 输出：delta数组被填充，内部计数清零。
 * 调用时机：固定周期测速任务中使用最合适。
 */


#endif