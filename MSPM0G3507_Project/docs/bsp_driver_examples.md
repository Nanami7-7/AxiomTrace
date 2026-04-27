# BSP 外设驱动使用案例

本文档提供 6 个 BSP 外设驱动的完整使用案例。所有驱动默认关闭，使用前需在 `bsp_config.h` 中将对应的 `BSP_USE_xxx` 宏设为 `1`。

---

## 1. 电机驱动 (bsp_motor)

**适用硬件**: TB6612 / L298N H桥 + 直流电机

### 使能方式

```c
/* bsp_config.h */
#define BSP_USE_MOTOR    1
```

### 案例：小车前进、转弯、后退、刹车

```c
#include "bsp_motor.h"

void motor_example(void)
{
    /* 初始化由 bsp_board_init() 自动完成 */

    /* 前进：两电机正转，速度 60% */
    bsp_motor_run(BSP_MOTOR_A, BSP_MOTOR_DIR_FORWARD, 60);
    bsp_motor_run(BSP_MOTOR_B, BSP_MOTOR_DIR_FORWARD, 60);

    osal_delay_ms(2000);  /* 前进 2 秒 */

    /* 左转：右轮正转，左轮反转 */
    bsp_motor_run(BSP_MOTOR_A, BSP_MOTOR_DIR_BACKWARD, 40);
    bsp_motor_run(BSP_MOTOR_B, BSP_MOTOR_DIR_FORWARD, 40);

    osal_delay_ms(500);   /* 转弯 0.5 秒 */

    /* 加速直线前进 */
    bsp_motor_run(BSP_MOTOR_A, BSP_MOTOR_DIR_FORWARD, 80);
    bsp_motor_run(BSP_MOTOR_B, BSP_MOTOR_DIR_FORWARD, 80);

    osal_delay_ms(1000);

    /* 刹车（急停） */
    bsp_motor_brake(BSP_MOTOR_A);
    bsp_motor_brake(BSP_MOTOR_B);

    /* 自由停止（滑行） */
    bsp_motor_stop(BSP_MOTOR_A);
    bsp_motor_stop(BSP_MOTOR_B);
}
```

---

## 2. 舵机驱动 (bsp_servo)

**适用硬件**: SG90 / MG996R 等标准舵机

### 使能方式

```c
/* bsp_config.h */
#define BSP_USE_SERVO    1
```

### 案例：舵机扫描 + 精确角度控制

```c
#include "bsp_servo.h"

void servo_example(void)
{
    /* 初始化由 bsp_board_init() 自动完成，默认归中位(90°) */

    /* 转到 0°（最左） */
    bsp_servo_set_angle(BSP_SERVO_0, 0);
    osal_delay_ms(500);

    /* 缓慢扫描 0° → 180° */
    for (uint32_t angle = 0; angle <= 180; angle += 5) {
        bsp_servo_set_angle(BSP_SERVO_0, angle);
        osal_delay_ms(20);
    }

    /* 精确脉宽控制：1.0 ms 脉宽 */
    bsp_servo_set_pulse_width(BSP_SERVO_0, 1000);

    /* 双舵机配合（如云台） */
    bsp_servo_set_angle(BSP_SERVO_0, 90);   /* 水平居中 */
    bsp_servo_set_angle(BSP_SERVO_1, 45);   /* 俯仰 45° */
}
```

---

## 3. MPU6050 六轴传感器 (bsp_mpu6050)

**适用硬件**: MPU6050 模块，I2C 接口

### 使能方式

```c
/* bsp_config.h */
#define BSP_USE_MPU6050    1
```

### 案例：读取加速度、陀螺仪和温度，转换为物理单位

```c
#include "bsp_mpu6050.h"

void mpu6050_example(void)
{
    bsp_mpu6050_axis_t accel, gyro;
    float temp;

    /* 读取原始加速度数据 */
    if (bsp_mpu6050_read_accel(&accel) == HAL_OK) {
        /* 转换为 g 单位：raw / sensitivity */
        float sensitivity = bsp_mpu6050_get_accel_sensitivity();
        float ax_g = accel.x / sensitivity;
        float ay_g = accel.y / sensitivity;
        float az_g = accel.z / sensitivity;
    }

    /* 读取原始陀螺仪数据 */
    if (bsp_mpu6050_read_gyro(&gyro) == HAL_OK) {
        /* 转换为 °/s 单位：raw / sensitivity */
        float sensitivity = bsp_mpu6050_get_gyro_sensitivity();
        float gx_dps = gyro.x / sensitivity;
        float gy_dps = gyro.y / sensitivity;
        float gz_dps = gyro.z / sensitivity;
    }

    /* 读取温度 */
    if (bsp_mpu6050_read_temp(&temp) == HAL_OK) {
        /* temp 即为摄氏温度 */
    }
}

/* 案例：修改量程 */
void mpu6050_change_range_example(void)
{
    /* 加速度改为 ±8g 量程 */
    bsp_mpu6050_set_accel_range(BSP_MPU6050_ACCEL_8G);

    /* 陀螺仪改为 ±1000 °/s 量程 */
    bsp_mpu6050_set_gyro_range(BSP_MPU6050_GYRO_1000);
}

/* 案例：周期性读取任务 */
void mpu6050_task(void *param)
{
    (void)param;
    bsp_mpu6050_axis_t accel;

    for (;;) {
        if (bsp_mpu6050_read_accel(&accel) == HAL_OK) {
            /* 处理加速度数据 ... */
        }
        osal_delay_ms(10);  /* 100 Hz 采样率 */
    }
}
```

---

## 4. OLED 显示屏 (bsp_oled)

**适用硬件**: SSD1306 驱动的 0.96" OLED，I2C 接口

### 使能方式

```c
/* bsp_config.h */
#define BSP_USE_OLED    1
```

### 案例：显示欢迎画面 + 实时数据

```c
#include "bsp_oled.h"

void oled_example(void)
{
    /* 初始化由 bsp_board_init() 自动完成 */

    /* 清屏 */
    bsp_oled_clear();

    /* 显示标题（小字体 6x8） */
    bsp_oled_write_string(0, 0, "MSPM0G3507 Demo", BSP_OLED_FONT_SMALL, 1);

    /* 显示分隔线 */
    bsp_oled_draw_hline(0, 8, 128, 1);

    /* 显示数据标签 */
    bsp_oled_write_string(0, 1, "Temp:", BSP_OLED_FONT_SMALL, 1);
    bsp_oled_write_string(0, 2, "Dist:", BSP_OLED_FONT_SMALL, 1);
    bsp_oled_write_string(0, 3, "Line:", BSP_OLED_FONT_SMALL, 1);

    /* 更新数值（就地覆盖旧值） */
    bsp_oled_write_string(48, 1, "25.3C", BSP_OLED_FONT_SMALL, 1);
    bsp_oled_write_string(48, 2, "150cm", BSP_OLED_FONT_SMALL, 1);
    bsp_oled_write_string(48, 3, "0101",  BSP_OLED_FONT_SMALL, 1);

    /* 画一个矩形框 */
    bsp_oled_fill_rect(80, 16, 48, 32, 1);   /* 白色填充 */
    bsp_oled_fill_rect(84, 20, 40, 24, 0);   /* 黑色内框（镂空效果） */

    /* 刷新显示到屏幕 */
    bsp_oled_update_display();

    /* 注意：所有绘图操作只修改帧缓冲区，必须调用 bsp_oled_update_display() 才会显示 */
}

/* 案例：动态刷新任务 */
void oled_update_task(void *param)
{
    (void)param;
    char buf[16];

    for (;;) {
        bsp_oled_clear();
        bsp_oled_write_string(0, 0, "Sensor Data", BSP_OLED_FONT_SMALL, 1);

        /* 假设有全局变量存储传感器数据 */
        snprintf(buf, sizeof(buf), "AX:%d", g_accel_x);
        bsp_oled_write_string(0, 1, buf, BSP_OLED_FONT_SMALL, 1);

        snprintf(buf, sizeof(buf), "D:%dcm", g_distance_cm);
        bsp_oled_write_string(0, 2, buf, BSP_OLED_FONT_SMALL, 1);

        bsp_oled_update_display();
        osal_delay_ms(100);  /* 10 Hz 刷新 */
    }
}
```

---

## 5. 循迹模块 (bsp_tracer)

**适用硬件**: 4路/5路红外循迹传感器模块

### 使能方式

```c
/* bsp_config.h */
#define BSP_USE_TRACER    1
```

### 案例：黑线检测 + 差速纠偏

```c
#include "bsp_tracer.h"
#include "bsp_motor.h"

void tracer_example(void)
{
    /* 读取所有通道 */
    uint8_t line = bsp_tracer_read_all();
    /* line = 0b0100 表示 CH2 检测到黑线 */

    /* 读取单个通道 */
    if (bsp_tracer_read_channel(BSP_TRACER_CH2)) {
        /* 中间传感器检测到黑线，直行 */
    }
}

/* 案例：循迹 PID 控制任务 */
void line_follow_task(void *param)
{
    (void)param;

    for (;;) {
        uint8_t line = bsp_tracer_read_all();

        switch (line) {
        case 0b0010:  /* 偏左 → 右转修正 */
            bsp_motor_run(BSP_MOTOR_A, BSP_MOTOR_DIR_FORWARD, 50);
            bsp_motor_run(BSP_MOTOR_B, BSP_MOTOR_DIR_FORWARD, 30);
            break;
        case 0b0100:  /* 居中 → 直行 */
            bsp_motor_run(BSP_MOTOR_A, BSP_MOTOR_DIR_FORWARD, 50);
            bsp_motor_run(BSP_MOTOR_B, BSP_MOTOR_DIR_FORWARD, 50);
            break;
        case 0b1000:  /* 偏右 → 左转修正 */
            bsp_motor_run(BSP_MOTOR_A, BSP_MOTOR_DIR_FORWARD, 30);
            bsp_motor_run(BSP_MOTOR_B, BSP_MOTOR_DIR_FORWARD, 50);
            break;
        case 0b0000:  /* 脱线 → 原地旋转寻找 */
            bsp_motor_run(BSP_MOTOR_A, BSP_MOTOR_DIR_FORWARD, 20);
            bsp_motor_run(BSP_MOTOR_B, BSP_MOTOR_DIR_BACKWARD, 20);
            break;
        default:
            break;
        }

        osal_delay_ms(10);
    }
}
```

---

## 6. 超声波测距 HC-SR04 (bsp_hcsr04)

**适用硬件**: HC-SR04 超声波模块

### 使能方式

```c
/* bsp_config.h */
#define BSP_USE_HCSR04    1
```

### 案例 1：阻塞式测距

```c
#include "bsp_hcsr04.h"

void hcsr04_blocking_example(void)
{
    uint32_t dist_cm;

    /* 阻塞式测距，最多等待 30ms */
    if (bsp_hcsr04_get_distance_cm(&dist_cm, 30) == HAL_OK) {
        /* dist_cm 为测量的厘米距离 */
        if (dist_cm < 10) {
            /* 距障碍物太近，执行避障 */
        }
    } else {
        /* 超时，无回波 */
    }
}
```

### 案例 2：异步回调式测距

```c
#include "bsp_hcsr04.h"

static volatile uint32_t g_latest_distance_mm = 0;

void ultrasonic_callback(uint32_t distance_mm)
{
    g_latest_distance_mm = distance_mm;
}

void hcsr04_async_example(void)
{
    /* 注册异步回调 */
    bsp_hcsr04_register_callback(ultrasonic_callback);

    /* 触发一次测量（非阻塞） */
    bsp_hcsr04_start_measure();

    /* 回调会在 Echo 下降沿中断中自动调用 */
    /* 之后可随时读取 g_latest_distance_mm */
}

/* 案例：周期测距任务 */
void hcsr04_task(void *param)
{
    (void)param;
    uint32_t dist_mm;

    for (;;) {
        if (bsp_hcsr04_get_distance(&dist_mm, 30) == HAL_OK) {
            /* 处理距离数据 */
        }
        osal_delay_ms(100);  /* 10 Hz 测距频率 */
    }
}
```

---

## 综合案例：智能小车

```c
/**
 * 综合应用：循迹避障智能小车
 * 功能：OLED显示状态 + 循迹行驶 + 超声波避障 + MPU6050姿态监测
 */

#include "bsp_motor.h"
#include "bsp_tracer.h"
#include "bsp_hcsr04.h"
#include "bsp_mpu6050.h"
#include "bsp_oled.h"
#include "bsp_led.h"
#include "osal_task.h"

/* bsp_config.h 中需设置:
 * #define BSP_USE_MOTOR    1
 * #define BSP_USE_TRACER   1
 * #define BSP_USE_HCSR04   1
 * #define BSP_USE_MPU6050  1
 * #define BSP_USE_OLED     1
 */

static void smart_car_task(void *param)
{
    (void)param;
    uint32_t dist_cm;
    bsp_mpu6050_axis_t accel;
    char buf[16];

    for (;;) {
        /* 1. 超声波避障检测 */
        if (bsp_hcsr04_get_distance_cm(&dist_cm, 30) == HAL_OK && dist_cm < 15) {
            /* 障碍物太近 → 后退转弯 */
            bsp_motor_run(BSP_MOTOR_A, BSP_MOTOR_DIR_BACKWARD, 30);
            bsp_motor_run(BSP_MOTOR_B, BSP_MOTOR_DIR_FORWARD, 30);
            osal_delay_ms(300);
            continue;
        }

        /* 2. 循迹行驶 */
        uint8_t line = bsp_tracer_read_all();
        if (line & 0b0100) {
            /* 居中直行 */
            bsp_motor_run(BSP_MOTOR_A, BSP_MOTOR_DIR_FORWARD, 50);
            bsp_motor_run(BSP_MOTOR_B, BSP_MOTOR_DIR_FORWARD, 50);
        } else if (line & 0b0010) {
            /* 偏左修正 */
            bsp_motor_run(BSP_MOTOR_A, BSP_MOTOR_DIR_FORWARD, 40);
            bsp_motor_run(BSP_MOTOR_B, BSP_MOTOR_DIR_FORWARD, 55);
        } else if (line & 0b1000) {
            /* 偏右修正 */
            bsp_motor_run(BSP_MOTOR_A, BSP_MOTOR_DIR_FORWARD, 55);
            bsp_motor_run(BSP_MOTOR_B, BSP_MOTOR_DIR_FORWARD, 40);
        }

        /* 3. 读取 MPU6050 姿态 */
        bsp_mpu6050_read_accel(&accel);

        /* 4. OLED 更新显示 */
        bsp_oled_clear();
        bsp_oled_write_string(0, 0, "SmartCar v1.0", BSP_OLED_FONT_SMALL, 1);
        snprintf(buf, sizeof(buf), "D:%3lucm", dist_cm);
        bsp_oled_write_string(0, 1, buf, BSP_OLED_FONT_SMALL, 1);
        snprintf(buf, sizeof(buf), "L:0x%02X", line);
        bsp_oled_write_string(0, 2, buf, BSP_OLED_FONT_SMALL, 1);
        snprintf(buf, sizeof(buf), "A:%5d", accel.x);
        bsp_oled_write_string(0, 3, buf, BSP_OLED_FONT_SMALL, 1);
        bsp_oled_update_display();

        osal_delay_ms(50);  /* 20 Hz 控制循环 */
    }
}

/* 在 app_task_create_all() 中创建任务:
 * osal_task_config_t car_cfg = {
 *     .name       = "SmartCar",
 *     .function   = smart_car_task,
 *     .param      = NULL,
 *     .stack_size = 256,
 *     .priority   = OSAL_TASK_PRIORITY_NORMAL,
 * };
 * osal_task_create(&car_cfg, &car_handle);
 */
```
