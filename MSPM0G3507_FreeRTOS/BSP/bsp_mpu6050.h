/*
 * 立创开发板软硬件资料与相关扩展板软硬件资料官网全部开源
 * 开发板官网：www.lckfb.com
 * 技术支持常驻论坛，任何技术问题欢迎随时交流学习
 * 立创论坛：https://oshwhub.com/forum
 * 关注bilibili账号：【立创开发板】，掌握我们的最新动态！
 * 不靠卖板赚钱，以培养中国工程师为己任
 */
#ifndef _BSP_MPU6050_H_
#define _BSP_MPU6050_H_

#include "ti_msp_dl_config.h"

#define SDA_OUT()   {                                                  \
                        DL_GPIO_initDigitalOutput(IIC_SDA_IOMUX);      \
                        DL_GPIO_setPins(IIC_PORT, IIC_SDA_PIN);        \
                        DL_GPIO_enableOutput(IIC_PORT, IIC_SDA_PIN);   \
                    }
#define SDA_IN()    { DL_GPIO_initDigitalInput(IIC_SDA_IOMUX); }

#define SDA_GET()   ( ( ( DL_GPIO_readPins(IIC_PORT,IIC_SDA_PIN) & IIC_SDA_PIN ) > 0 ) ? 1 : 0 )
#define SDA(x)      ( (x) ? (DL_GPIO_setPins(IIC_PORT,IIC_SDA_PIN)) : (DL_GPIO_clearPins(IIC_PORT,IIC_SDA_PIN)) )
#define SCL(x)      ( (x) ? (DL_GPIO_setPins(IIC_PORT,IIC_SCL_PIN)) : (DL_GPIO_clearPins(IIC_PORT,IIC_SCL_PIN)) )

#define MPU6050_RA_SMPLRT_DIV       0x19
#define MPU6050_RA_CONFIG           0x1A
#define MPU6050_RA_GYRO_CONFIG      0x1B
#define MPU6050_RA_ACCEL_CONFIG     0x1C
#define MPU_INT_EN_REG              0X38
#define MPU_USER_CTRL_REG           0X6A
#define MPU_FIFO_EN_REG             0X23
#define MPU_PWR_MGMT2_REG           0X6C
#define MPU_GYRO_CFG_REG            0X1B
#define MPU_ACCEL_CFG_REG           0X1C
#define MPU_CFG_REG                 0X1A
#define MPU_SAMPLE_RATE_REG         0X19
#define MPU_INTBP_CFG_REG           0X37

#define MPU6050_RA_PWR_MGMT_1       0x6B
#define MPU6050_RA_PWR_MGMT_2       0x6C

#define MPU6050_WHO_AM_I            0x75
#define MPU6050_SMPLRT_DIV          0
#define MPU6050_DLPF_CFG            0
#define MPU6050_GYRO_OUT            0x43
#define MPU6050_ACC_OUT             0x3B

#define MPU6050_RA_TEMP_OUT_H       0x41
#define MPU6050_RA_TEMP_OUT_L       0x42

#define MPU_ACCEL_XOUTH_REG         0X3B
#define MPU_ACCEL_XOUTL_REG         0X3C
#define MPU_ACCEL_YOUTH_REG         0X3D
#define MPU_ACCEL_YOUTL_REG         0X3E
#define MPU_ACCEL_ZOUTH_REG         0X3F
#define MPU_ACCEL_ZOUTL_REG         0X40

#define MPU_TEMP_OUTH_REG           0X41
#define MPU_TEMP_OUTL_REG           0X42

#define MPU_GYRO_XOUTH_REG          0X43
#define MPU_GYRO_XOUTL_REG          0X44
#define MPU_GYRO_YOUTH_REG          0X45
#define MPU_GYRO_YOUTL_REG          0X46
#define MPU_GYRO_ZOUTH_REG          0X47
#define MPU_GYRO_ZOUTL_REG          0X48

char MPU6050_WriteReg(uint8_t addr,uint8_t regaddr,uint8_t num,uint8_t *regdata);
char MPU6050_ReadData(uint8_t addr, uint8_t regaddr,uint8_t num,uint8_t* Read);

char MPU6050_Init(void);
void MPU6050ReadGyro(short *gyroData);
void MPU6050ReadAcc(short *accData);
float MPU6050_GetTemp(void);
uint8_t MPU6050ReadID(void);
#endif