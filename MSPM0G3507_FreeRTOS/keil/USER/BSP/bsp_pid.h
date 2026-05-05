#ifndef	__BSP_PID_H__
#define __BSP_PID_H__

#include "ti_msp_dl_config.h"


int Velocity_A(int TargetVelocity, int CurrentVelocity);
int Velocity_B(int TargetVelocity, int CurrentVelocity);
extern float Velcity_Kp,Velcity_Ki,Velcity_Kd;


typedef struct{
float Kp;
float Ki;
float Kd;
float SP;//Setpoiunt
float OutputMin;
float OutputMax;
uint64_t LastTime;
float LastError;
float LastIntegral;
}PID;
extern PID pid_lf, pid_lb, pid_rf, pid_rb;
void PID_Init(PID *pid,float kp,float ki,float kd,float min,float max);
float PID_Compute(PID *pid,float current,float target);
#endif