#ifndef BSP_TIMER_H
#define BSP_TIMER_H

#include <stdint.h>
#include <stdbool.h>

void bsp_timer_init(void);
uint32_t bsp_get_us(void);
uint32_t bsp_get_ms(void);
void bsp_delay_us(uint32_t us);
void bsp_delay_ms(uint32_t ms);

#endif
