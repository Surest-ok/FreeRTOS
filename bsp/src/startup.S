/*
 * AArch64 startup for QEMU virt with GICv3 (SMP)
 *
 * Supports:
 *   -machine virt,gic-version=3           → starts at EL1
 *   -machine virt,gic-version=3,secure=off → starts at EL1 (single NS state)
 *   -machine virt,gic-version=3,virtualization=on → starts at EL2, drops to EL1
 *
 * Multi-core:
 *   Core 0 (MPIDR affinity == 0): full init → main()
 *   Core 1+ (linear core id > 0): WFE mailbox wait → secondary_entry[]
 *
 * Per-core exception stacks: 8KB each. stack_top is the low address of the
 * reserved stack area; SP_EL1 is set to the high address of each core's slot.
 */

#define STACK_SIZE 8192

.macro GET_LINEAR_CORE_ID Xreg, Xtmp
    mrs     \Xtmp, MPIDR_EL1
    and     \Xreg, \Xtmp, #0xFF       /* Aff0 */
    ubfx    \Xtmp, \Xtmp, #8, #8      /* Aff1 */
    add     \Xreg, \Xreg, \Xtmp, lsl #4
.endm

.section .text.entrypoint
.global _start

_start:
    mrs     x5, MPIDR_EL1
    /* Check Aff2[23:16] | Aff1[15:8] | Aff0[7:0] — all must be 0 for primary.
     * cortex-a53 uses 3-level affinity; QEMU virt with 32 cores uses Aff1
     * to distinguish clusters. Using only Aff0 misses cluster IDs. */
    and     x6, x5, #0x00FFFFFF
    cbnz    x6, park_secondary

    /* === Primary Core (Core 0) === */

    /* Detect current EL */
    mrs     x10, CurrentEL
    lsr     x10, x10, #2
    and     x10, x10, #3

    cmp     x10, #2
    b.eq    el2_setup
    b       el1_setup

el2_setup:
#if defined(CONFIG_RUN_AT_EL2)
    /* Native EL2 Execution: FreeRTOS executes entirely in EL2 */
    mrs     x0, S3_4_C12_C9_5         /* ICC_SRE_EL2 */
    orr     x0, x0, #0xF              /* SRE, DFB, DIB, Enable */
    msr     S3_4_C12_C9_5, x0
    isb

    /* Route physical IRQ/FIQ/SError to EL2: HCR_EL2.RW=1, IMO=1, FMO=1, AMO=1 */
    ldr     x0, =0x80000038
    msr     hcr_el2, x0
    isb

    ldr     x0, =0x33FF
    msr     cnthctl_el2, x0
    msr     cntvoff_el2, xzr
    isb

    msr     cptr_el2, xzr             /* No FP/SIMD traps */
    isb

    ldr     x6, =0x30C50838
    msr     SCTLR_EL2, x6
    isb

    msr     spsel, #1
    bl      setup_stack_el1
    b       common_init
#else
    /* Drop to EL1 Execution: configure EL2 and drop cleanly */
    mrs     x0, S3_4_C12_C9_5         /* ICC_SRE_EL2 */
    orr     x0, x0, #0xF              /* SRE + DFB + DIB + Enable (allows EL1 ICC access) */
    msr     S3_4_C12_C9_5, x0
    isb

    ldr     x0, =0x33FF               /* Allow EL1 timer access without trap */
    msr     cnthctl_el2, x0
    msr     cntvoff_el2, xzr
    isb

    msr     cptr_el2, xzr             /* Disable FP/SIMD traps at EL2 */
    isb

    mov     x0, #(1 << 31)            /* HCR_EL2: RW=1 (EL1 is AArch64) */
    msr     hcr_el2, x0
    isb

    /* Configure EL1 state before dropping */
    mrs     x6, CPACR_EL1
    orr     x6, x6, #0x00F00000       /* Enable FP/SIMD */
    msr     CPACR_EL1, x6

    ldr     x6, =0x30C50838
    msr     SCTLR_EL1, x6
    isb

    /* Drop to EL1 */
    ldr     x0, =el1_entry
    msr     elr_el2, x0
    mov     x0, #0x3C5                /* SPSR_EL2: EL1h, DAIF all masked */
    msr     spsr_el2, x0
    eret                              /* → el1_entry at EL1 */
#endif

el1_entry:
    /* Now at EL1 — set up per-core stack */
    bl      setup_stack_el1
    b       common_init

el1_setup:
    msr     spsel, #1
    bl      setup_stack_el1

    mrs     x6, CPACR_EL1
    orr     x6, x6, #0x00F00000
    msr     CPACR_EL1, x6

    ldr     x6, =0x30C50838
    msr     SCTLR_EL1, x6
    isb

common_init:
    /* Clear BSS (only primary core does this).
     * Aligns to 16B, uses STP for speed, then STR for tail. */
    ldr     x0, =__bss_start
    ldr     x1, =__bss_end
    /* Align to 16 bytes: clear first 8 if odd-aligned */
    tbz     x0, #3, 2f
    str     xzr, [x0], #8
    /* Fast loop: STP clears 16 bytes/iteration */
2:  sub     x2, x1, x0
    cmp     x2, #16
    b.lt    3f
    stp     xzr, xzr, [x0], #16
    b       2b
    /* Tail: remaining < 16 bytes, clear 8 at a time */
3:  cmp     x0, x1
    b.ge    bss_done
    str     xzr, [x0], #8
    b       3b
bss_done:

    /* Enable IRQ */
    msr     daifclr, #0x2
    bl      main

/* === Secondary Cores === */

park_secondary:
    /* Secondary cores may also start at EL2. Handle the drop. */
    mrs     x10, CurrentEL
    lsr     x10, x10, #2
    and     x10, x10, #3
    cmp     x10, #2
    b.eq    el2_setup_secondary

wait_for_mailbox:
    /* WFE sleep loop — Core0 writes this core's mailbox, then SEV wakes us. */
    GET_LINEAR_CORE_ID x6, x9
    ldr     x7, =secondary_entry
    add     x7, x7, x6, lsl #3
1:  ldr     x8, [x7]
    cbnz    x8, 1f
    wfe
    b       1b
1:  dmb     sy

    /* Got entry point — set up per-core stack */
    bl      setup_stack_el1

    mrs     x10, CurrentEL
    lsr     x10, x10, #2
    and     x10, x10, #3
    cmp     x10, #2
    b.eq    2f

    /* Configure EL1 */
    mrs     x6, CPACR_EL1
    orr     x6, x6, #0x00F00000
    msr     CPACR_EL1, x6
    ldr     x6, =0x30C50838
    msr     SCTLR_EL1, x6
    isb
    b       3f

2:  /* Configure EL2 */
    msr     cptr_el2, xzr
    ldr     x6, =0x30C50838
    msr     SCTLR_EL2, x6
    isb
3:

    /* Enable IRQ */
    msr     daifclr, #0x2

    /* Jump to secondary core entry (e.g., vPortSecondaryCoreEntry) */
    br      x8

el2_setup_secondary:
#if defined(CONFIG_RUN_AT_EL2)
    mrs     x0, S3_4_C12_C9_5
    orr     x0, x0, #0xF
    msr     S3_4_C12_C9_5, x0
    isb

    ldr     x0, =0x80000038           /* Route IRQ to EL2 */
    msr     hcr_el2, x0
    isb

    ldr     x0, =0x33FF
    msr     cnthctl_el2, x0
    msr     cntvoff_el2, xzr
    isb

    msr     cptr_el2, xzr
    isb

    ldr     x6, =0x30C50838
    msr     SCTLR_EL2, x6
    isb

    b       wait_for_mailbox
#else
    /* EL2 → EL1 drop for secondary cores */
    mrs     x0, S3_4_C12_C9_5         /* ICC_SRE_EL2 */
    orr     x0, x0, #0xF
    msr     S3_4_C12_C9_5, x0
    isb

    ldr     x0, =0x33FF
    msr     cnthctl_el2, x0
    msr     cntvoff_el2, xzr
    isb

    msr     cptr_el2, xzr
    isb

    mov     x0, #(1 << 31)            /* HCR_EL2.RW = 1 */
    msr     hcr_el2, x0
    isb

    mrs     x6, CPACR_EL1
    orr     x6, x6, #0x00F00000
    msr     CPACR_EL1, x6

    ldr     x6, =0x30C50838
    msr     SCTLR_EL1, x6
    isb

    /* Drop to EL1 — return to wait_for_mailbox */
    ldr     x0, =wait_for_mailbox
    msr     elr_el2, x0
    mov     x0, #0x3C5                /* SPSR_EL2: EL1h, DAIF all masked */
    msr     spsr_el2, x0
    eret
#endif

/* === Stack Setup Helper === */
/* Sets SP_EL1 = stack_top + ((linear_core_id + 1) * STACK_SIZE) */
setup_stack_el1:
    GET_LINEAR_CORE_ID x6, x9
    ldr     x20, =stack_top
    mov     x7, #STACK_SIZE
    add     x6, x6, #1
    madd    x20, x6, x7, x20          /* x20 = stack_top + (core_id + 1) * STACK_SIZE */
    msr     spsel, #1
    mov     sp, x20
    ret

/* stack_top and secondary_entry are defined in linker.ld,
 * placed above all BSS data to avoid overlap with FreeRTOS globals. */
