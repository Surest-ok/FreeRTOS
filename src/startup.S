/*
 * AArch64 startup code for QEMU virt platform
 * Handles EL drop from EL2/EL3 to EL1, secondary core parking
 */

.section .text.entrypoint
.global _start

_start:
    /* Save current MPIDR for boot core identification */
    mrs x5, MPIDR_EL1

    /* We're already at EL1 (QEMU virt) */
    /* Check if this is a secondary core */
    and x6, x5, #0xFF   /* Get Aff0 (core ID) */
    cbz x6, secondary_core_done

    /* Secondary core: park in WFE loop */
    mov x8, #0
    b park_secondary

secondary_core_done:

    /* Primary core: set up stack pointer */
    /* Using stack from linker script, but initialize SP_EL1 */
    msr spsel, #1      /* Use SP_EL1 instead of SP_EL0 */
    ldr x20, =stack_top   /* Load stack base address */
    add x20, x20, #4096   /* Stack grows down, SP = base + size */
    mov sp, x20         /* Set stack pointer */

    /* Enable FP/SIMD for EL1 */
    mrs x6, CPACR_EL1
    orr x6, x6, #0x00F00000 /* CPACR_EL1: Enable CP10 and CP11 for EL1 */
    msr CPACR_EL1, x6

    /* Configure SCTLR_EL1: MMU OFF, caches off, no alignment trap
     * Set reserved bits: 0x30C50838 (bits that must be 1 per ARM spec)
     * We do NOT set bit 0 (MMU) or bit 2 (D-cache) since we have
     * no page tables configured yet.
     */
    ldr x6, =0x30C50838
    msr SCTLR_EL1, x6
    isb

    /* Clear BSS section */
    ldr x0, =__bss_start
    ldr x1, =__bss_end
    mov x2, #0
    b clear_bss

clear_bss:
    cmp x0, x1
    b.ge bss_cleared
    str x2, [x0], #8
    b clear_bss

bss_cleared:
    /* Initialize timer frequency (optional) */
    /* For now, assume CNTFRQ_EL0 is already set by QEMU */

    /* Jump to C main */
    bl main

park_secondary:
    /* Secondary cores wait for primary to wake them */
    wfe
    /* When woken, fall through to the C entry point */
    b secondary_cpu_entry

/* Stack definition (must be last in the file) */
.section .bss.stack
.global stack_top
stack_top:
    .space 4096    /* 4KB stack */