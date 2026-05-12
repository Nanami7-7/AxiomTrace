/*
 * 立创开发板软硬件资料与相关扩展板软硬件资料官网全部开源
 * 开发板官网：www.lckfb.com
 * 技术支持常驻论坛，任何技术问题欢迎随时交流学习
 * 立创论坛：https://oshwhub.com/forum
 * 关注bilibili账号：【立创开发板】，掌握我们的最新动态！
 * 不靠卖板赚钱，以培养中国工程师为己任
 */
#include "bsp_mpu6050.h"
#include "board.h"
#include <stdio.h>

#define CPU_FREQ_MHZ   80
#define delay_us(us)   delay_cycles((us) * CPU_FREQ_MHZ)
#define delay_ms(ms)   delay_cycles((ms) * 1000 * CPU_FREQ_MHZ)

void IIC_Start(void)
{
    SDA_OUT();
    SCL(1);
    SDA(0);

    SDA(1);
    delay_us(5);
    SDA(0);
    delay_us(5);

    SCL(0);
}

void IIC_Stop(void)
{
    SDA_OUT();
    SCL(0);
    SDA(0);

    SCL(1);
    delay_us(5);
    SDA(1);
    delay_us(5);
}

void IIC_Send_Ack(unsigned char ack)
{
    SDA_OUT();
    SCL(0);
    SDA(0);
    delay_us(5);
    if(!ack) SDA(0);
    else         SDA(1);
    SCL(1);
    delay_us(5);
    SCL(0);
    SDA(1);
}

unsigned char I2C_WaitAck(void)
{
    char ack = 0;
    unsigned char ack_flag = 10;
    SCL(0);
    SDA(1);
    SDA_IN();

    SCL(1);
    while( (SDA_GET()==1) && ( ack_flag ) )
    {
        ack_flag--;
        delay_us(5);
    }

    if( ack_flag <= 0 )
    {
        IIC_Stop();
        return 1;
    }
    else
    {
        SCL(0);
        SDA_OUT();
    }
    return ack;
}

void Send_Byte(uint8_t dat)
{
    int i = 0;
    SDA_OUT();
    SCL(0);

    for( i = 0; i < 8; i++ )
    {
        SDA( (dat & 0x80) >> 7 );
        delay_us(1);
        SCL(1);
        delay_us(5);
        SCL(0);
        delay_us(5);
        dat<<=1;
    }
}

unsigned char Read_Byte(void)
{
    unsigned char i,receive=0;
    SDA_IN();
    for(i=0;i<8;i++ )
    {
        SCL(0);
        delay_us(5);
        SCL(1);
        delay_us(5);
        receive<<=1;
        if( SDA_GET() )
        {
            receive|=1;
        }
        delay_us(5);
    }
    SCL(0);
    return receive;
}

char MPU6050_WriteReg(uint8_t addr,uint8_t regaddr,uint8_t num,uint8_t *regdata)
{
    uint16_t i = 0;
    IIC_Start();
    Send_Byte((addr<<1)|0);
    if( I2C_WaitAck() == 1 ) {IIC_Stop();return 1;}
    Send_Byte(regaddr);
    if( I2C_WaitAck() == 1 ) {IIC_Stop();return 2;}

    for(i=0;i<num;i++)
    {
        Send_Byte(regdata[i]);
        if( I2C_WaitAck() == 1 ) {IIC_Stop();return (3+i);}
    }
    IIC_Stop();
    return 0;
}

char MPU6050_ReadData(uint8_t addr, uint8_t regaddr,uint8_t num,uint8_t* Read)
{
    uint8_t i;
    IIC_Start();
    Send_Byte((addr<<1)|0);
    if( I2C_WaitAck() == 1 ) {IIC_Stop();return 1;}
    Send_Byte(regaddr);
    if( I2C_WaitAck() == 1 ) {IIC_Stop();return 2;}

    IIC_Start();
    Send_Byte((addr<<1)|1);
    if( I2C_WaitAck() == 1 ) {IIC_Stop();return 3;}

    for(i=0;i<(num-1);i++){
        Read[i]=Read_Byte();
        IIC_Send_Ack(0);
    }
    Read[i]=Read_Byte();
    IIC_Send_Ack(1);
    IIC_Stop();
    return 0;
}

uint8_t MPU_Set_Gyro_Fsr(uint8_t fsr)
{
    return MPU6050_WriteReg(0x68,MPU_GYRO_CFG_REG,1,(uint8_t*)(fsr<<3));
}

uint8_t MPU_Set_Accel_Fsr(uint8_t fsr)
{
    return MPU6050_WriteReg(0x68,MPU_ACCEL_CFG_REG,1,(uint8_t*)(fsr<<3));
}

uint8_t MPU_Set_LPF(uint16_t lpf)
{
    uint8_t data=0;

    if(lpf>=188)data=1;
    else if(lpf>=98)data=2;
    else if(lpf>=42)data=3;
    else if(lpf>=20)data=4;
    else if(lpf>=10)data=5;
    else data=6;
    return data=MPU6050_WriteReg(0x68,MPU_CFG_REG,1,&data);
}

uint8_t MPU_Set_Rate(uint16_t rate)
{
    uint8_t data;
    if(rate>1000)rate=1000;
    if(rate<4)rate=4;
    data=1000/rate-1;
    data=MPU6050_WriteReg(0x68,MPU_SAMPLE_RATE_REG,1,&data);
     return MPU_Set_LPF(rate/2);
}

void MPU6050ReadGyro(short *gyroData)
{
    uint8_t buf[6];
    uint8_t reg = 0;
    reg = MPU6050_ReadData(0x68,MPU6050_GYRO_OUT,6,buf);
    if( reg == 0 )
    {
        gyroData[0] = (buf[0] << 8) | buf[1];
        gyroData[1] = (buf[2] << 8) | buf[3];
        gyroData[2] = (buf[4] << 8) | buf[5];
    }
}

void MPU6050ReadAcc(short *accData)
{
    uint8_t buf[6];
    uint8_t reg = 0;
    reg = MPU6050_ReadData(0x68, MPU6050_ACC_OUT, 6, buf);
    if( reg == 0)
    {
        accData[0] = (buf[0] << 8) | buf[1];
        accData[1] = (buf[2] << 8) | buf[3];
        accData[2] = (buf[4] << 8) | buf[5];
    }
}

float MPU6050_GetTemp(void)
{
    short temp3;
    uint8_t buf[2];
    float Temperature = 0;
    MPU6050_ReadData(0x68,MPU6050_RA_TEMP_OUT_H,2,buf);
    temp3= (buf[0] << 8) | buf[1];
    Temperature=((double) temp3/340.0)+36.53;
    return Temperature;
}

uint8_t MPU6050ReadID(void)
{
    unsigned char Re[2] = {0};
    printf("mpu=%d\r\n",MPU6050_ReadData(0x68,0X75,1,Re));

    if (Re[0] != 0x68)
    {
        printf("Not found MPU6050 module");
        return 1;
     }
    else
    {
        printf("MPU6050 ID = %x\r\n",Re[0]);
        return 0;
    }
}

char MPU6050_Init(void)
{
    SDA_OUT();
    delay_ms(10);
    MPU6050_WriteReg(0x68,MPU6050_RA_PWR_MGMT_1, 1,(uint8_t*)(0x80));
    delay_ms(100);
    MPU6050_WriteReg(0x68,MPU6050_RA_PWR_MGMT_1,1, (uint8_t*)(0x00));

    MPU_Set_Gyro_Fsr(3);
    MPU_Set_Accel_Fsr(0);
    MPU_Set_Rate(50);

    MPU6050_WriteReg(0x68,MPU_INT_EN_REG , 1,(uint8_t*)0x00);
    MPU6050_WriteReg(0x68,MPU_USER_CTRL_REG,1,(uint8_t*)0x00);
    MPU6050_WriteReg(0x68,MPU_FIFO_EN_REG,1,(uint8_t*)0x00);
    MPU6050_WriteReg(0x68,MPU_INTBP_CFG_REG,1,(uint8_t*)0X80);

    if( MPU6050ReadID() == 0 )
    {
        MPU6050_WriteReg(0x68,MPU6050_RA_PWR_MGMT_1, 1,(uint8_t*)0x01);
        MPU6050_WriteReg(0x68,MPU_PWR_MGMT2_REG, 1,(uint8_t*)0x00);
        MPU_Set_Rate(50);
        return 1;
    }
    return 0;
}