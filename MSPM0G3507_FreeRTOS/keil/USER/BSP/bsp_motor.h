#ifndef	__BSP_MOTOR_H__
#define __BSP_MOTOR_H__

#include "board.h"
#define ABS(a)      (a>0 ? a:(-a))

extern int32_t Get_Encoder_countA,Get_Encoder_countB,encoderA_cnt,encoderB_cnt;
//extern volatile uint64_t tick;//定时器计数
void motor_init(void);
void Set_PWM(int pwma,int pwmb);
void motor_stop();
extern uint64_t GetUs();
#endif
