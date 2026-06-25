ENTRY(_start)
SECTIONS
{
    . = 0x40000000;
    .text.entrypoint : { KEEP(*(.text.entrypoint)) *(.text*) *(.rodata*) }
    .data : { *(.data*) }
    .bss : {
        __bss_start = .;
        *(.bss*) *(COMMON)
        . = ALIGN(16);
        ucHeap = .;
        . = . + (256 * 1024);
        __bss_end = .;
    }
    .stack (NOLOAD) : { . = ALIGN(16); stack_top = .; . = . + (8192 * 32); }
    .mailbox (NOLOAD) : { . = ALIGN(8); secondary_entry = .; . = . + (8 * 32); }
    _end = .;
}
