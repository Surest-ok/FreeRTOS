/*
 * AArch64 startup for QEMU virt with GICv3
 *
 * Supports:
 *   -machine virt,gic-version=3           → starts at EL1
 *   -machine virt,gic-version=3,secure=off → starts at EL1 (single NS state)
 *   -machine virt,gic-version=3,virtualization=on → starts at EL2
 *
 * At EL2: enable ICC_SRE_EL2 for GICv3 sysregs, then stay at EL2.
 * At EL1: proceed with normal init.
 */

.section .text.entrypoint
.global _start

_start:
    mrs x5, MPIDR_EL1
    and x6, x5, #0xFF
    cbnz x6, park_secondary

    /* Detect current EL */
    mrs x10, CurrentEL
    lsr x10, x10, #2
    and x10, x10, #3

    cmp x10, #2
    b.eq el2_setup
    b el1_setup

el2_setup:
    /* At EL2: enable GICv3 system register access */
    mrs x0, S3_4_C12_C9_5         /* ICC_SRE_EL2 */
    orr x0, x0, #0x7
    msr S3_4_C12_C9_5, x0
    isb

    /* Stack at EL2 */
    msr spsel, #1
    ldr x20, =stack_top
    add x20, x20, #4096
    mov sp, x20

    mrs x6, CPACR_EL1
    orr x6, x6, #0x00F00000
    msr CPACR_EL1, x6

    ldr x6, =0x30C50838
    msr SCTLR_EL1, x6
    isb
    b common_init

el1_setup:
    msr spsel, #1
    ldr x20, =stack_top
    add x20, x20, #4096
    mov sp, x20

    mrs x6, CPACR_EL1
    orr x6, x6, #0x00F00000
    msr CPACR_EL1, x6

    ldr x6, =0x30C50838
    msr SCTLR_EL1, x6
    isb

common_init:
    /* Clear BSS */
    ldr x0, =__bss_start
    ldr x1, =__bss_end
    mov x2, #0
clear_bss:
    cmp x0, x1
    b.ge bss_done
    str x2, [x0], #8
    b clear_bss
bss_done:

    /* Enable IRQ (clear DAIF.I bit) */
    msr daifclr, #0x2
    bl main

park_secondary:
    wfe
    b park_secondary

.section .bss.stack
.global stack_top
stack_top:
    .space 4096
