#ifndef	__BOARD_H__
#define __BOARD_H__

#include "ti_msp_dl_config.h"
#include "string.h"
#include "math.h"
#include "bsp_encoder.h"

#define MOTOR_STOP    0
#define MOTOR_SPEED   1
#define MOTOR_DIR  		2
#define MOTOR_TRACK  	3


void board_init(void);

void delay_ms(uint32_t count);
void delay_us(uint32_t count);
void delay_1us(uint32_t __us);
void delay_1ms(uint32_t ms);

void uart0_send_char(char ch);
void uart0_send_string(char* str);

void board_test_print_encoder(bsp_encoder_id_t id);



#endif
