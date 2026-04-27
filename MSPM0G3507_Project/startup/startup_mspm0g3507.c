/**
 * @file    startup_mspm0g3507.c
 * @brief   MSPM0G3507 startup code and interrupt vector table
 * @note    Compatible with FreeRTOS (PendSV + SysTick used by scheduler)
 */

#include <stdint.h>

/* ---------- Linker symbols ---------- */
extern uint32_t _estack;
extern uint32_t _sidata;
extern uint32_t _sdata;
extern uint32_t _edata;
extern uint32_t _sbss;
extern uint32_t _ebss;

/* ---------- System entry points ---------- */
extern int  main(void);
extern void SystemInit(void);

/* ---------- FreeRTOS hooks (implemented in app layer) ---------- */
extern void vApplicationIdleHook(void);
extern void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName);
extern void vApplicationMallocFailedHook(void);

/* ---------- Default interrupt handler ---------- */
void Default_Handler(void)
{
    __asm volatile ("bkpt #0");
    while (1);
}

/* ---------- Cortex-M0+ core exception handlers ---------- */
void Reset_Handler(void);
void NMI_Handler(void)              __attribute__((weak, alias("Default_Handler")));
void HardFault_Handler(void)        __attribute__((weak, alias("Default_Handler")));
void SVC_Handler(void)              __attribute__((weak, alias("Default_Handler")));
void PendSV_Handler(void)           __attribute__((weak, alias("Default_Handler")));
void SysTick_Handler(void)          __attribute__((weak, alias("Default_Handler")));

/* ---------- MSPM0G3507 peripheral interrupt handlers ---------- */
void GPIOA_IRQHandler(void)         __attribute__((weak, alias("Default_Handler")));
void GPIOB_IRQHandler(void)         __attribute__((weak, alias("Default_Handler")));
void UART0_IRQHandler(void)         __attribute__((weak, alias("Default_Handler")));
void UART1_IRQHandler(void)         __attribute__((weak, alias("Default_Handler")));
void SPI0_IRQHandler(void)          __attribute__((weak, alias("Default_Handler")));
void SPI1_IRQHandler(void)          __attribute__((weak, alias("Default_Handler")));
void I2C0_IRQHandler(void)          __attribute__((weak, alias("Default_Handler")));
void I2C1_IRQHandler(void)          __attribute__((weak, alias("Default_Handler")));
void ADC0_IRQHandler(void)          __attribute__((weak, alias("Default_Handler")));
void TIMA0_IRQHandler(void)         __attribute__((weak, alias("Default_Handler")));
void TIMA1_IRQHandler(void)         __attribute__((weak, alias("Default_Handler")));
void TIMG0_IRQHandler(void)         __attribute__((weak, alias("Default_Handler")));
void TIMG1_IRQHandler(void)         __attribute__((weak, alias("Default_Handler")));
void DAC0_IRQHandler(void)          __attribute__((weak, alias("Default_Handler")));
void DMA_IRQHandler(void)           __attribute__((weak, alias("Default_Handler")));

/* ---------- Interrupt vector table ---------- */
__attribute__((section(".isr_vector")))
const uint32_t g_pfnVectors[] = {
    (uint32_t)&_estack,          /*  -16: Initial stack pointer      */
    (uint32_t)Reset_Handler,     /*  -15: Reset                      */
    (uint32_t)NMI_Handler,       /*  -14: Non-maskable interrupt      */
    (uint32_t)HardFault_Handler, /*  -13: Hard fault                  */
    0,                           /*  -12: Reserved                    */
    0,                           /*  -11: Reserved                    */
    0,                           /*  -10: Reserved                    */
    0,                           /*   -9: Reserved                    */
    0,                           /*   -8: Reserved                    */
    0,                           /*   -7: Reserved                    */
    0,                           /*   -6: Reserved                    */
    (uint32_t)SVC_Handler,       /*   -5: SVCall                      */
    0,                           /*   -4: Reserved                    */
    0,                           /*   -3: Reserved                    */
    (uint32_t)PendSV_Handler,    /*   -2: PendSV (FreeRTOS context switch) */
    (uint32_t)SysTick_Handler,   /*   -1: SysTick (FreeRTOS tick)     */

    /* MSPM0G3507 peripheral interrupts */
    (uint32_t)GPIOA_IRQHandler,  /*   0: GPIO Port A                  */
    (uint32_t)GPIOB_IRQHandler,  /*   1: GPIO Port B                  */
    (uint32_t)UART0_IRQHandler,  /*   2: UART0                        */
    (uint32_t)UART1_IRQHandler,  /*   3: UART1                        */
    (uint32_t)SPI0_IRQHandler,   /*   4: SPI0                         */
    (uint32_t)SPI1_IRQHandler,   /*   5: SPI1                         */
    (uint32_t)I2C0_IRQHandler,   /*   6: I2C0                         */
    (uint32_t)I2C1_IRQHandler,   /*   7: I2C1                         */
    (uint32_t)ADC0_IRQHandler,   /*   8: ADC0                         */
    (uint32_t)TIMA0_IRQHandler,  /*   9: Timer A0                     */
    (uint32_t)TIMA1_IRQHandler,  /*  10: Timer A1                     */
    (uint32_t)TIMG0_IRQHandler,  /*  11: Timer G0                     */
    (uint32_t)TIMG1_IRQHandler,  /*  12: Timer G1                     */
    (uint32_t)DAC0_IRQHandler,   /*  13: DAC0                         */
    (uint32_t)DMA_IRQHandler,    /*  14: DMA                          */
};

/* ---------- Reset handler ---------- */
void Reset_Handler(void)
{
    uint32_t *p_src, *p_dst;

    /* Copy .data section from Flash to SRAM */
    p_src = &_sidata;
    p_dst = &_sdata;
    while (p_dst < &_edata) {
        *p_dst++ = *p_src++;
    }

    /* Zero-fill .bss section */
    p_dst = &_sbss;
    while (p_dst < &_ebss) {
        *p_dst++ = 0;
    }

    /* Call CMSIS system initialization */
    SystemInit();

    /* Call application entry point */
    main();

    /* Should never reach here */
    while (1);
}
