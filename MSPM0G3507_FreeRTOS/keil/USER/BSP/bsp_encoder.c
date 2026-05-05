#include "bsp_encoder.h"
#include "ti_msp_dl_config.h"
#define PulseToTime 0.00002
#define ReductionRatio 20
#define PPR 13
#define CapturePeriod 13
//extern volatile uint64_t tick;//定时器计数
extern uint64_t GetUs();
EncoderInfo LEFT_FRONT = {
	ENCODER_LEFT_FRONT,
	CAPTURE_LEFT_FRONT_INST,
	ENCODER_PORT,
	ENCODER_LEFT_FRONT_B_PIN
};
EncoderInfo LEFT_BACK = {
	ENCODER_LEFT_BACK,
	CAPTURE_LEFT_BACK_INST,
	ENCODER_PORT,
	ENCODER_LEFT_BACK_B_PIN
};
EncoderInfo RIGHT_FRONT = {
	ENCODER_RIGHT_FRONT,
	CAPTURE_RIGHT_FRONT_INST,
	ENCODER_PORT,
	ENCODER_RIGHT_FRONT_B_PIN
};
EncoderInfo RIGHT_BACK = {
	ENCODER_RIGHT_BACK,
	CAPTURE_RIGHT_BACK_INST,
	ENCODER_PORT,
	ENCODER_RIGHT_BACK_B_PIN
};
/* 4个电机的累计脉冲计数（A相双边沿判向，等效二倍频） */
static volatile int8_t sign[ENCODER_COUNT] = {1};//方向
static volatile int8_t sign_cpy[ENCODER_COUNT] = {1};//方向
static volatile float Omega[ENCODER_COUNT] = {0};//角速度
static volatile int32_t EncoderCount[ENCODER_COUNT]= {0};//编码器值 2倍频
/* 可选调试量：最近一次捕获到的脉宽/周期（TIMCLK tick） */
volatile uint64_t loadcnt =0;
static const uint32_t capture_period[ENCODER_COUNT] = {
	CAPTURE_LEFT_FRONT_INST_LOAD_VALUE + 1U,
	CAPTURE_LEFT_BACK_INST_LOAD_VALUE + 1U,
	CAPTURE_RIGHT_FRONT_INST_LOAD_VALUE + 1U,
	CAPTURE_RIGHT_BACK_INST_LOAD_VALUE + 1U,
};
//static volatile uint64_t last_tick[ENCODER_COUNT]={0};
//static volatile uint32_t gLastPeriod[ENCODER_COUNT] = {0};
//volatile static uint32_t pwmPeriod;
//__attribute__((unused)) volatile static uint32_t pwmDuty;
static volatile uint32_t pin ;
float Encoder_GetOmegaById(EncoderId id)
{
	if ((uint32_t)id >= ENCODER_COUNT) {
		return 0.0f;
	}
	return Omega[id];
}
void CAPTURE_INST_IRQHandler(EncoderInfo info)
{
	
//////////////////
	switch (DL_TimerG_getPendingInterrupt(info.INST)) {	
			
	  case DL_TIMERG_IIDX_CC0_UP://一个脉冲结束 A相下降沿			
				if (DL_GPIO_readPins(info.port,info.pin) & info.pin){sign[info.id]=1;}
				else{sign[info.id]=-1;	}	//判断正反转				
       	EncoderCount[info.id] += sign[info.id];//更新编码器 2倍频
        break;								
  case DL_TIMERG_IIDX_CC1_UP://经过一个脉冲周期 A相上升沿			
				if (DL_GPIO_readPins(info.port,info.pin) & info.pin){sign[info.id]=-1;}
				else{sign[info.id]=1;}//判断正反转
       			EncoderCount[info.id] += sign[info.id];//更新编码器 2倍频
				break;
		case DL_TIMERG_IIDX_LOAD:
		default:break;
		}			
	}
	 

void Encoder_Init(void)
{
  NVIC_ClearPendingIRQ(CAPTURE_LEFT_FRONT_INST_INT_IRQN);
	NVIC_EnableIRQ(CAPTURE_LEFT_FRONT_INST_INT_IRQN);
	
	NVIC_ClearPendingIRQ(CAPTURE_LEFT_BACK_INST_INT_IRQN);
	NVIC_EnableIRQ(CAPTURE_LEFT_BACK_INST_INT_IRQN);
	
	NVIC_ClearPendingIRQ(CAPTURE_RIGHT_FRONT_INST_INT_IRQN);
	NVIC_EnableIRQ(CAPTURE_RIGHT_FRONT_INST_INT_IRQN);
	
	NVIC_ClearPendingIRQ(CAPTURE_RIGHT_BACK_INST_INT_IRQN);
	NVIC_EnableIRQ(CAPTURE_RIGHT_BACK_INST_INT_IRQN);
    for(int i=0; i<ENCODER_COUNT; i++)
    {
    EncoderCount[i] = 0;
		Omega[i] = 0.0f;
		sign[i] = 1;
		sign_cpy[i] = 1;

    }
}



void Encoder_ClearCount(void)
{
	__disable_irq();
	for (int i = 0; i < ENCODER_COUNT; i++) {
		EncoderCount[i] = 0;
	}
	__enable_irq();
}

void Encoder_GetAllCount(int32_t count[ENCODER_COUNT])
{
	if (count == 0) {
		return;
	}

	__disable_irq();
	for (int i = 0; i < ENCODER_COUNT; i++) {
		count[i] = EncoderCount[i];
	}
	__enable_irq();
}

////电机编码器脉冲计数
void TIMER_0_INST_IRQHandler(void)
{
//	static int key_time = 0;//按键扫描
//	
//	//编码器速度计算
	if( DL_TimerG_getPendingInterrupt(TIMER_0_INST) == DL_TIMER_IIDX_LOAD )
	{		

		loadcnt++;
	//	printf("%d\r\n", Encoder_GetCountById(ENCODER_LEFT_FRONT));
	//	printf("%f\r\n", Encoder_GetOmegaById(ENCODER_LEFT_FRONT));

//		encoderA_cnt = Get_Encoder_countA;//两个电机安装相反，所以编码器值也要相反
//		encoderB_cnt = -Get_Encoder_countB;
//		Get_Encoder_countA = 0;//编码器计数值清零
//		Get_Encoder_countB = 0;
//		
//		//如果当前是速度环
//		if( motor_mode == MOTOR_SPEED )
//		{
//			PWMA = Velocity_A(motor_speed, encoderA_cnt);//指定两轮速度
//			PWMB = Velocity_B(motor_speed, encoderB_cnt);
//			Set_PWM(PWMA,PWMB);
//		}
//		
//		//按键扫描
//		if( key_time++ >= 3 )
//		{
//			key_time = 0;
//			flex_button_scan();
//		}
	}
//	if(DL_TimerG_getTimerCount
}

uint64_t GetUs()
{
	static uint64_t us;
	us = DL_TimerG_getTimerCount(TIMER_0_INST) +
			(loadcnt * (TIMER_0_INST_LOAD_VALUE + 1U));
	return us;
}



void CAPTURE_LEFT_FRONT_INST_IRQHandler(void)
{
	CAPTURE_INST_IRQHandler(LEFT_FRONT);  
}

void CAPTURE_LEFT_BACK_INST_IRQHandler(void)
{
	CAPTURE_INST_IRQHandler(LEFT_BACK);
}

void CAPTURE_RIGHT_FRONT_INST_IRQHandler(void)
{
	CAPTURE_INST_IRQHandler(RIGHT_FRONT);
}

void CAPTURE_RIGHT_BACK_INST_IRQHandler(void)
{
	CAPTURE_INST_IRQHandler(RIGHT_BACK);
}