/*
 * AArch64 startup for QEMU virt with GICv3
 *
 * Supports:
 *   -machine virt,gic-version=3           → starts at EL1
 *   -machine virt,gic-version=3,secure=off → starts at EL1 (single NS state)
 *   -machine virt,gic-version=3,virtualization=on → starts at EL2, drops to EL1
 *
 * When starting at EL2:
 *   1. Enable ICC_SRE_EL2 (GICv3 system register access)
 *   2. Set HCR_EL2.RW=1 (AArch64 at EL1)
 *   3. Configure SCTLR_EL1, CPACR_EL1
 *   4. eret to EL1 — then everything uses the standard EL1 path
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
    /* Enable GICv3 system register access at EL2 */
    mrs x0, S3_4_C12_C9_5         /* ICC_SRE_EL2 */
    orr x0, x0, #0x7
    msr S3_4_C12_C9_5, x0
    isb

    /*
     * HCR_EL2: RW=1 (EL1 is AArch64), no IRQ/FIQ routing.
     * Physical interrupts will be handled at EL1 after the drop.
     */
    mov x0, #(1 << 31)            /* RW bit */
    msr hcr_el2, x0
    isb

    /* Configure EL1 state before dropping */
    mrs x6, CPACR_EL1
    orr x6, x6, #0x00F00000
    msr CPACR_EL1, x6

    ldr x6, =0x30C50838
    msr SCTLR_EL1, x6
    isb

    /* Drop to EL1 */
    ldr x0, =el1_entry
    msr elr_el2, x0
    mov x0, #0x3C5                /* SPSR_EL2: EL1h, DAIF all masked */
    msr spsr_el2, x0
    eret                          /* → el1_entry at EL1 */

el1_entry:
    /* Now at EL1 — set up stack */
    msr spsel, #1
    ldr x20, =stack_top
    add x20, x20, #8192
    mov sp, x20
    b common_init

el1_setup:
    msr spsel, #1
    ldr x20, =stack_top
    add x20, x20, #8192
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
    .space 8192
