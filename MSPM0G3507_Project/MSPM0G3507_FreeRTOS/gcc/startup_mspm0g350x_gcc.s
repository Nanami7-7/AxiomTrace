.syntax unified
.cpu cortex-m0plus
.thumb
.section .intvecs,"a",%progbits
.align 2
.global __Vectors
.type __Vectors, %object
__Vectors:
    .word __StackTop
    .word Reset_Handler
    .word NMI_Handler
    .word HardFault_Handler
    .word 0
    .word 0
    .word 0
    .word 0
    .word 0
    .word 0
    .word 0
    .word SVC_Handler
    .word 0
    .word 0
    .word PendSV_Handler
    .word SysTick_Handler
    .word GROUP0_IRQHandler
    .word GROUP1_IRQHandler
    .word TIMG8_IRQHandler
    .word UART3_IRQHandler
    .word ADC0_IRQHandler
    .word ADC1_IRQHandler
    .word CANFD0_IRQHandler
    .word DAC0_IRQHandler
    .word 0
    .word SPI0_IRQHandler
    .word SPI1_IRQHandler
    .word 0
    .word 0
    .word UART1_IRQHandler
    .word UART2_IRQHandler
    .word UART0_IRQHandler
    .word TIMG0_IRQHandler
    .word TIMG6_IRQHandler
    .word TIMA0_IRQHandler
    .word TIMA1_IRQHandler
    .word TIMG7_IRQHandler
    .word TIMG12_IRQHandler
    .word 0
    .word 0
    .word I2C0_IRQHandler
    .word I2C1_IRQHandler
    .word 0
    .word 0
    .word AES_IRQHandler
    .word 0
    .word RTC_IRQHandler
    .word DMA_IRQHandler
.global __Vectors_End
.type __Vectors_End, %object
__Vectors_End:
.size __Vectors, . - __Vectors

.section .text.Reset_Handler,"ax",%progbits
.thumb_func
.global Reset_Handler
.type Reset_Handler, %function
Reset_Handler:
    ldr r0, =_sidata
    ldr r1, =_sdata
    ldr r2, =_edata
1:
    cmp r1, r2
    bcs 2f
    ldr r3, [r0]
    str r3, [r1]
    adds r0, r0, #4
    adds r1, r1, #4
    b 1b
2:
    ldr r1, =_sbss
    ldr r2, =_ebss
    movs r3, #0
3:
    cmp r1, r2
    bcs 4f
    str r3, [r1]
    adds r1, r1, #4
    b 3b
4:
    ldr r0, =main
    blx r0
5:
    b 5b
.size Reset_Handler, . - Reset_Handler

.section .text.Default_Handler,"ax",%progbits
.thumb_func
.global Default_Handler
.type Default_Handler, %function
Default_Handler:
    b .
.macro WEAK_DEFAULT name
    .weak \name
    .set \name, Default_Handler
.endm
WEAK_DEFAULT NMI_Handler
WEAK_DEFAULT HardFault_Handler
WEAK_DEFAULT SVC_Handler
WEAK_DEFAULT PendSV_Handler
WEAK_DEFAULT SysTick_Handler
WEAK_DEFAULT GROUP0_IRQHandler
WEAK_DEFAULT GROUP1_IRQHandler
WEAK_DEFAULT TIMG8_IRQHandler
WEAK_DEFAULT UART3_IRQHandler
WEAK_DEFAULT ADC0_IRQHandler
WEAK_DEFAULT ADC1_IRQHandler
WEAK_DEFAULT CANFD0_IRQHandler
WEAK_DEFAULT DAC0_IRQHandler
WEAK_DEFAULT SPI0_IRQHandler
WEAK_DEFAULT SPI1_IRQHandler
WEAK_DEFAULT UART1_IRQHandler
WEAK_DEFAULT UART2_IRQHandler
WEAK_DEFAULT UART0_IRQHandler
WEAK_DEFAULT TIMG0_IRQHandler
WEAK_DEFAULT TIMG6_IRQHandler
WEAK_DEFAULT TIMA0_IRQHandler
WEAK_DEFAULT TIMA1_IRQHandler
WEAK_DEFAULT TIMG7_IRQHandler
WEAK_DEFAULT TIMG12_IRQHandler
WEAK_DEFAULT I2C0_IRQHandler
WEAK_DEFAULT I2C1_IRQHandler
WEAK_DEFAULT AES_IRQHandler
WEAK_DEFAULT RTC_IRQHandler
WEAK_DEFAULT DMA_IRQHandler
.section .note.GNU-stack,"",%progbits
