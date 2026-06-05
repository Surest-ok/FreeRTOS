/*
 * Linker script for FreeRTOS SMP on QEMU virt AArch64
 * Target: RAM base = 0x40000000, EL1, single stack
 */

ENTRY(_start)

SECTIONS
{
    /* Load the entry point at the very beginning of RAM */
    . = 0x40000000;

    /*
     * .text.entrypoint section must be the first in the file
     * This contains the assembly startup code that QEMU jumps to
     */
    .text.entrypoint :
    {
        KEEP(*(.text.entrypoint))
        *(.text*)
        *(.rodata*)
    }

    /* Data section */
    .data :
    {
        *(.data*)
    }

    /* BSS section (zero-initialized data) */
    .bss :
    {
        __bss_start = .;
        *(.bss*)
        *(COMMON)
        __bss_end = .;
    }

    /* End of image */
    _end = .;
}