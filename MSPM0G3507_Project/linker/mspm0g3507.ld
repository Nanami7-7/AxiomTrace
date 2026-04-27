/* =============================================================================
 * MSPM0G3507 Linker Script
 * ARM Cortex-M0+ | 128KB Flash | 32KB SRAM
 * ============================================================================= */

/* ---------- Memory regions ---------- */
MEMORY
{
    FLASH (rx)  : ORIGIN = 0x00000000, LENGTH = 128K
    SRAM  (rwx) : ORIGIN = 0x20000000, LENGTH = 32K
}

/* ---------- Stack and heap sizes ---------- */
_stack_size = 1024;   /* Main stack size (used before FreeRTOS scheduler start) */
_heap_size  = 0;      /* Heap managed by FreeRTOS (heap_4), not C library */

/* ---------- Entry point ---------- */
ENTRY(Reset_Handler)

/* ---------- Sections ---------- */
SECTIONS
{
    /* --- ISR Vector (must be at the very beginning of Flash) --- */
    .isr_vector :
    {
        . = ALIGN(4);
        KEEP(*(.isr_vector))
        . = ALIGN(4);
    } > FLASH

    /* --- Code --- */
    .text :
    {
        . = ALIGN(4);
        *(.text)
        *(.text*)
        *(.glue_7)
        *(.glue_7t)
        KEEP(*(.init))
        KEEP(*(.fini))
        . = ALIGN(4);
    } > FLASH

    /* --- Read-only data --- */
    .rodata :
    {
        . = ALIGN(4);
        *(.rodata)
        *(.rodata*)
        . = ALIGN(4);
    } > FLASH

    /* --- ARM sections --- */
    .ARM.extab :
    {
        *(.ARM.extab*)
    } > FLASH

    .ARM.exidx :
    {
        __exidx_start = .;
        *(.ARM.exidx*)
        __exidx_end = .;
    } > FLASH

    /* --- Init/fini arrays --- */
    .preinit_array :
    {
        PROVIDE_HIDDEN(__preinit_array_start = .);
        KEEP(*(.preinit_array*))
        PROVIDE_HIDDEN(__preinit_array_end = .);
    } > FLASH

    .init_array :
    {
        PROVIDE_HIDDEN(__init_array_start = .);
        KEEP(*(SORT(.init_array.*)))
        KEEP(*(.init_array*))
        PROVIDE_HIDDEN(__init_array_end = .);
    } > FLASH

    .fini_array :
    {
        PROVIDE_HIDDEN(__fini_array_start = .);
        KEEP(*(SORT(.fini_array.*)))
        KEEP(*(.fini_array*))
        PROVIDE_HIDDEN(__fini_array_end = .);
    } > FLASH

    /* --- SIDATA: Load address of .data (used by startup copy) --- */
    _sidata = LOADADDR(.data);

    /* --- Initialized data (stored in Flash, copied to SRAM at startup) --- */
    .data :
    {
        . = ALIGN(4);
        _sdata = .;
        *(.data)
        *(.data*)
        . = ALIGN(4);
        _edata = .;
    } > SRAM AT > FLASH

    /* --- Zero-initialized data --- */
    .bss :
    {
        . = ALIGN(4);
        _sbss = .;
        __bss_start__ = _sbss;
        *(.bss)
        *(.bss*)
        *(COMMON)
        . = ALIGN(4);
        _ebss = .;
        __bss_end__ = _ebss;
    } > SRAM

    /* --- FreeRTOS heap (placed after .bss) --- */
    .freertos_heap (NOINIT) :
    {
        . = ALIGN(8);
        __freertos_heap_start__ = .;
        . = . + 16K;   /* configTOTAL_HEAP_SIZE must match FreeRTOSConfig.h */
        __freertos_heap_end__ = .;
        . = ALIGN(8);
    } > SRAM

    /* --- User heap --- */
    ._user_heap (NOINIT) :
    {
        . = ALIGN(8);
        PROVIDE(end = .);
        PROVIDE(_end = .);
        . = . + _heap_size;
        . = ALIGN(8);
    } > SRAM

    /* --- Main stack (descending from top of SRAM) --- */
    ._stack :
    {
        . = ALIGN(8);
        _sstack = .;
        . = . + _stack_size;
        . = ALIGN(8);
        _estack = .;
    } > SRAM

    /* --- Remove unwanted sections --- */
    /DISCARD/ :
    {
        *(.ARM.attributes)
    }
}
