#include "bsp_pid.h"

extern uint64_t GetUs();
//float Velcity_Kp=1.1,  Velcity_Ki=0.5,  Velcity_Kd; //相关速度PID参数
float Velcity_Kp=4.15,  Velcity_Ki=0.9,  Velcity_Kd; //相关速度PID参数

PID pid_lf;
PID pid_lb;
PID pid_rf;
PID pid_rb;


/***************************************************************************
函数功能：电机的PID闭环控制
入口参数：左右电机的编码器值
返回值  ：电机的PWM
***************************************************************************/
int Velocity_A(int TargetVelocity, int CurrentVelocity)
{  
    int Bias;  //定义相关变量
		static int ControlVelocityA, Last_biasA; //静态变量，函数调用结束后其值依然存在
		
		Bias=TargetVelocity-CurrentVelocity; //求速度偏差
		
		ControlVelocityA+=Velcity_Ki*(Bias-Last_biasA)+Velcity_Kp*Bias;  //增量式PI控制器
                                                                   //Velcity_Kp*(Bias-Last_bias) 作用为限制加速度
	                                                                 //Velcity_Ki*Bias             速度控制值由Bias不断积分得到 偏差越大加速度越大
		Last_biasA=Bias;	
	    if(ControlVelocityA>9999) ControlVelocityA=9999;
	    else if(ControlVelocityA<-9999) ControlVelocityA=-9999;
		return ControlVelocityA; //返回速度控制值
}

/***************************************************************************
函数功能：电机的PID闭环控制
入口参数：左右电机的编码器值
返回值  ：电机的PWM
***************************************************************************/
int Velocity_B(int TargetVelocity, int CurrentVelocity)
{  
    int Bias;  //定义相关变量
		static int ControlVelocityB, Last_biasB; //静态变量，函数调用结束后其值依然存在
		
		Bias=TargetVelocity-CurrentVelocity; //求速度偏差
		
		ControlVelocityB+=Velcity_Ki*(Bias-Last_biasB)+Velcity_Kp*Bias;  //增量式PI控制器
                                                                   //Velcity_Kp*(Bias-Last_bias) 作用为限制加速度
	                                                                 //Velcity_Ki*Bias             速度控制值由Bias不断积分得到 偏差越大加速度越大
		Last_biasB=Bias;	
	    if(ControlVelocityB>9999) ControlVelocityB=9999;
	    else if(ControlVelocityB<-9999) ControlVelocityB=-9999;
		return ControlVelocityB; //返回速度控制值
}

void PID_Init(PID *pid,float kp,float ki,float kd,float min,float max)
{
pid->Kp = kp;
pid->Ki = ki;
pid->Kd = kd;
pid->SP	= 0;
pid->LastTime = 0;
pid->LastError = 0;
pid->LastIntegral = 0;
pid->OutputMin = min;
pid->OutputMax = max;
}

float PID_Compute(PID *pid,float current,float target)
{
pid->SP = target;
float error = target - current;
uint64_t now = GetUs();
float deltaTime = (now - pid->LastTime) / 1000000.0f; // 将时间差转换为秒
float error_integral = 0;
float error_derivative = 0;
if(pid->LastTime != 0) {
float error_integral = pid->LastIntegral + (error + pid->LastError) * deltaTime * 0.5f;
float error_derivative = (error - pid->LastError) / deltaTime;
}
//计算
float Op = pid->Kp * error;
float Oi = pid->Ki * error_integral;
float Od = pid->Kd * error_derivative;
float Output = Op + Oi + Od;
//更新
pid->LastTime = now;
pid->LastIntegral = error_integral;
pid->LastError = error;


if(Output > pid->OutputMax) {
Output = pid->OutputMax;
}
if(Output < pid->OutputMin) {
Output = pid->OutputMin;
}
if(pid->LastIntegral > pid->OutputMax) {
pid->LastIntegral = pid->OutputMax;
}
if(pid->LastIntegral < pid->OutputMin) {
pid->LastIntegral = pid->OutputMin;
}

return Output;

}