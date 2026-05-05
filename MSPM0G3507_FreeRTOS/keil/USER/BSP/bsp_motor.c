#include "bsp_motor.h"
#include "stdio.h"
//#include "bsp_pid.h"
//#include "bsp_key.h"
#include "bsp_encoder.h"
#include "ti_msp_dl_config.h"
//uint32_t gpio_interrup;
//extern uint32_t PWMA,PWMB;
//extern int32_t motor_speed;
//extern int32_t motor_mode;//0默认停止  1速度环  2位置环  3循迹
volatile bool flag;
volatile int32_t EncoderCount[ENCODER_COUNT];
void motor_init(void)
{
  //定时器中断
	NVIC_ClearPendingIRQ(TIMER_0_INST_INT_IRQN);
	NVIC_EnableIRQ(TIMER_0_INST_INT_IRQN);

	/* 编码器中断由Encoder_Init统一按4路配置开启 */
	Encoder_Init();
	printf("Motor initialized successfully\r\n");
}

////左右电机速度控制，范围0-999
void Set_PWM(int pwma,int pwmb)
{
	if(pwma>0) 
	{
		DL_GPIO_setPins(MOTOR_AIN2_PORT,MOTOR_AIN2_PIN);
		DL_GPIO_clearPins(MOTOR_AIN1_PORT,MOTOR_AIN1_PIN);	
		DL_TimerG_setCaptureCompareValue(PWM_MOTOR_INST,ABS(pwma),GPIO_PWM_MOTOR_C0_IDX);

	}
	else 
	{
		DL_GPIO_setPins(MOTOR_AIN1_PORT,MOTOR_AIN1_PIN);
		DL_GPIO_clearPins(MOTOR_AIN2_PORT,MOTOR_AIN2_PIN);
		DL_TimerG_setCaptureCompareValue(PWM_MOTOR_INST,ABS(pwma),GPIO_PWM_MOTOR_C0_IDX);
	}
	if(pwmb>0)
	{
		DL_GPIO_setPins(MOTOR_BIN1_PORT,MOTOR_BIN1_PIN);
		DL_GPIO_clearPins(MOTOR_BIN2_PORT,MOTOR_BIN2_PIN);
		DL_TimerG_setCaptureCompareValue(PWM_MOTOR_INST,ABS(pwmb),GPIO_PWM_MOTOR_C1_IDX);
	}
  else
	{
		DL_GPIO_setPins(MOTOR_BIN2_PORT,MOTOR_BIN2_PIN);
		DL_GPIO_clearPins(MOTOR_BIN1_PORT,MOTOR_BIN1_PIN);
		DL_TimerG_setCaptureCompareValue(PWM_MOTOR_INST,ABS(pwmb),GPIO_PWM_MOTOR_C1_IDX);
	}		

}

//void motor_stop()
//{
//		DL_GPIO_setPins(MOTOR_AIN1_A02_PORT,MOTOR_AIN1_A02_PIN);
//		DL_GPIO_setPins(MOTOR_AIN2_B24_PORT,MOTOR_AIN2_B24_PIN);
//		DL_TimerG_setCaptureCompareValue(PWM_MOTOR_INST,0,GPIO_PWM_MOTOR_C0_IDX);

//		DL_GPIO_setPins(MOTOR_BIN2_B19_PORT,MOTOR_BIN2_B19_PIN);
//		DL_GPIO_setPins(MOTOR_BIN1_B20_PORT,MOTOR_BIN1_B20_PIN);
//		DL_TimerG_setCaptureCompareValue(PWM_MOTOR_INST,0,GPIO_PWM_MOTOR_C1_IDX);

//}
///*******************************************************
//函数功能：外部中断模拟编码器信号
//入口函数：无
//返回  值：无
//***********************************************************/
//void GROUP1_IRQHandler(void)
//{
//	//获取中断信号
//	gpio_interrup = DL_GPIO_getEnabledInterruptStatus(GPIOB,ENCODER_E1A_PIN|ENCODER_E1B_PIN|ENCODER_E2A_PIN|ENCODER_E2B_PIN);
//	//encoderA
//	if((gpio_interrup & ENCODER_E1A_PIN)==ENCODER_E1A_PIN)
//	{
//		if(!DL_GPIO_readPins(GPIOB,ENCODER_E1B_PIN))
//		{
//			Get_Encoder_countA--;
//		}
//		else
//		{
//			Get_Encoder_countA++;
//		}
//	}
//	else if((gpio_interrup & ENCODER_E1B_PIN)==ENCODER_E1B_PIN)
//	{
//		if(!DL_GPIO_readPins(GPIOB,ENCODER_E1A_PIN))
//		{
//			Get_Encoder_countA++;
//		}
//		else
//		{
//			Get_Encoder_countA--;
//		}
//	}
//	//encoderB
//	if((gpio_interrup & ENCODER_E2A_PIN)==ENCODER_E2A_PIN)
//	{
//		if(!DL_GPIO_readPins(GPIOB,ENCODER_E2B_PIN))
//		{
//			Get_Encoder_countB--;
//		}
//		else
//		{
//			Get_Encoder_countB++;
//		}
//	}
//	else if((gpio_interrup & ENCODER_E2B_PIN)==ENCODER_E2B_PIN)
//	{
//		if(!DL_GPIO_readPins(GPIOB,ENCODER_E2A_PIN))
//		{
//			Get_Encoder_countB++;
//		}
//		else
//		{
//			Get_Encoder_countB--;
//		}
//	}
//	DL_GPIO_clearInterruptStatus(GPIOB,ENCODER_E1A_PIN|ENCODER_E1B_PIN|ENCODER_E2A_PIN|ENCODER_E2B_PIN);
//}


