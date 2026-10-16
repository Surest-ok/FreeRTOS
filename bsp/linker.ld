ENTRY(_start)
SECTIONS
{
    . = 0x40000000;
    .text.entrypoint : { KEEP(*(.text.entrypoint)) *(.text*) *(.rodata*) }
    .cherry_shell_sym : {
        . = ALIGN(8);
        __cherry_shell_sym_start = .;
        KEEP(*(SORT(.cherry_shell_sym*)))
        __cherry_shell_sym_end = .;
    }
    .ksymtab : {
        . = ALIGN(8);
        __ksymtab_start = .;
        KEEP(*(SORT(__ksymtab*)))
        __ksymtab_end = .;
    }
    .data : { *(.data*) }
    .bss : {
        __bss_start = .;
        *(.bss*) *(COMMON)
        . = ALIGN(16);
        ucHeap = .;
        . = . + (64 * 1024 * 1024);
        __bss_end = .;
    }
    .stack (NOLOAD) : { . = ALIGN(16); stack_top = .; . = . + (8192 * 32); }
    .mailbox (NOLOAD) : { . = ALIGN(8); secondary_entry = .; . = . + (8 * 32); }
    _end = .;
}
