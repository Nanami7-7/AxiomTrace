_Min_Stack_Size = 0x1000;
_Min_Heap_Size = 0;
MEMORY
{
    FLASH      (rx)  : ORIGIN = 0x00000000, LENGTH = 0x00020000
    SRAM       (rwx) : ORIGIN = 0x20200000, LENGTH = 0x00008000
    BCR_CONFIG (r)   : ORIGIN = 0x41C00000, LENGTH = 0x000000FF
    BSL_CONFIG (r)   : ORIGIN = 0x41C00100, LENGTH = 0x00000080
}
__StackTop = ORIGIN(SRAM) + LENGTH(SRAM);
__StackLimit = __StackTop - _Min_Stack_Size;
SECTIONS
{
    .intvecs ORIGIN(FLASH) : ALIGN(4)
    {
        KEEP(*(.intvecs))
    } > FLASH
    .text : ALIGN(4)
    {
        *(.text.Reset_Handler)
        *(.text*)
        KEEP(*(.init))
        KEEP(*(.fini))
        . = ALIGN(4);
    } > FLASH
    .rodata : ALIGN(4)
    {
        *(.rodata*)
        . = ALIGN(4);
    } > FLASH
    .ARM.exidx : ALIGN(4)
    {
        *(.ARM.exidx*)
    } > FLASH
    .preinit_array : ALIGN(4)
    {
        PROVIDE_HIDDEN(__preinit_array_start = .);
        KEEP(*(.preinit_array*))
        PROVIDE_HIDDEN(__preinit_array_end = .);
    } > FLASH
    .init_array : ALIGN(4)
    {
        PROVIDE_HIDDEN(__init_array_start = .);
        KEEP(*(SORT(.init_array.*)))
        KEEP(*(.init_array*))
        PROVIDE_HIDDEN(__init_array_end = .);
    } > FLASH
    .fini_array : ALIGN(4)
    {
        PROVIDE_HIDDEN(__fini_array_start = .);
        KEEP(*(SORT(.fini_array.*)))
        KEEP(*(.fini_array*))
        PROVIDE_HIDDEN(__fini_array_end = .);
    } > FLASH
    . = ALIGN(4);
    _sidata = LOADADDR(.data);
    .data : ALIGN(4)
    {
        _sdata = .;
        *(.data*)
        . = ALIGN(4);
        _edata = .;
    } > SRAM AT > FLASH
    .ramfunc : ALIGN(4)
    {
        __ramfunc_start__ = .;
        *(.ramfunc*)
        . = ALIGN(4);
        __ramfunc_end__ = .;
    } > SRAM AT > FLASH
    .bss (NOLOAD) : ALIGN(4)
    {
        _sbss = .;
        __bss_start__ = .;
        *(.bss*)
        *(COMMON)
        . = ALIGN(4);
        _ebss = .;
        __bss_end__ = .;
    } > SRAM
    .noinit (NOLOAD) : ALIGN(4)
    {
        *(.noinit*)
        . = ALIGN(4);
    } > SRAM
    .heap (NOLOAD) : ALIGN(8)
    {
        __HeapBase = .;
        . += _Min_Heap_Size;
        __HeapLimit = .;
    } > SRAM
    .stack __StackLimit (NOLOAD) : ALIGN(8)
    {
        . = ALIGN(8);
        __StackLimit = .;
        . += _Min_Stack_Size;
        . = ALIGN(8);
        __StackTop = .;
    } > SRAM
    .BCRConfig ORIGIN(BCR_CONFIG) :
    {
        KEEP(*(.BCRConfig))
    } > BCR_CONFIG
    .BSLConfig ORIGIN(BSL_CONFIG) :
    {
        KEEP(*(.BSLConfig))
    } > BSL_CONFIG
    /DISCARD/ :
    {
        *(.note.GNU-stack)
        *(.comment)
    }
}
PROVIDE(end = _ebss);
PROVIDE(_end = _ebss);
ASSERT(__StackLimit >= _ebss, "SRAM overflow: application data overlaps stack");
ASSERT(SIZEOF(.intvecs) <= 0xC0, "Interrupt vector table exceeds MSPM0G3507 size");
