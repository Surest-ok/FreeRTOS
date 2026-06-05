/*
 * Exception Vector Table for AArch64 EL1
 * GICv3 system register interface (ICC_IAR1_EL1 / ICC_EOIR1_EL1)
 *
 * Vector table layout: 16 entries, 2KB aligned, each entry 128 bytes.
 * We use SP_EL1, so interrupts arrive at EL1h group (offset 0x200).
 */

.section ".text.vectors", "ax", %progbits
.align 11

.macro ventry label
    .align 7
    b \label
.endm

.global vector_table

vector_table:
    /* Current EL with SP_EL0 */
    ventry  sp0_stub           /* +0x000 Sync */
    ventry  sp0_stub           /* +0x080 IRQ */
    ventry  sp0_stub           /* +0x100 FIQ */
    ventry  sp0_stub           /* +0x180 SError */

    /* Current EL with SP_ELx (SP_EL1) — main handlers */
    ventry  el1h_sync          /* +0x200 Sync */
    ventry  el1h_irq           /* +0x280 IRQ ← GICv3 */
    ventry  el1h_fiq           /* +0x300 FIQ */
    ventry  el1h_serror        /* +0x380 SError */

    /* Lower EL AArch64 — unused */
    ventry  lower_stub
    ventry  lower_stub
    ventry  lower_stub
    ventry  lower_stub

    /* Lower EL AArch32 — unused */
    ventry  lower_stub
    ventry  lower_stub
    ventry  lower_stub
    ventry  lower_stub

/* ============================================================
 * Stubs
 * ============================================================ */
sp0_stub:
lower_stub:
    b panic_hang

/* ============================================================
 * EL1h Synchronous
 * ============================================================ */
el1h_sync:
    mrs x0, esr_el1
    mrs x1, elr_el1
    b panic_hang

/* ============================================================
 * EL1h IRQ Handler — GICv3 system register interface
 * ============================================================ */
el1h_irq:
    /* Save all general-purpose registers */
    stp x29, x30, [sp, #-16]!
    stp x27, x28, [sp, #-16]!
    stp x25, x26, [sp, #-16]!
    stp x23, x24, [sp, #-16]!
    stp x21, x22, [sp, #-16]!
    stp x19, x20, [sp, #-16]!
    stp x17, x18, [sp, #-16]!
    stp x15, x16, [sp, #-16]!
    stp x13, x14, [sp, #-16]!
    stp x11, x12, [sp, #-16]!
    stp x9, x10, [sp, #-16]!
    stp x7, x8, [sp, #-16]!
    stp x5, x6, [sp, #-16]!
    stp x3, x4, [sp, #-16]!
    stp x1, x2, [sp, #-16]!
    stp x0, x1, [sp, #-16]!

    /* Acknowledge interrupt: read ICC_IAR1_EL1 */
    mrs x0, S3_0_C12_C12_0

    /* Save IAR value in callee-saved register (x19 is already on stack) */
    mov x19, x0

    /* Call C dispatcher */
    bl interrupt_dispatch

    /* End of interrupt: write ICC_EOIR1_EL1 with original IAR value */
    mov x0, x19
    msr S3_0_C12_C12_1, x0

    /* Restore registers */
    ldp x0, x1, [sp], #16
    ldp x1, x2, [sp], #16
    ldp x3, x4, [sp], #16
    ldp x5, x6, [sp], #16
    ldp x7, x8, [sp], #16
    ldp x9, x10, [sp], #16
    ldp x11, x12, [sp], #16
    ldp x13, x14, [sp], #16
    ldp x15, x16, [sp], #16
    ldp x17, x18, [sp], #16
    ldp x19, x20, [sp], #16
    ldp x21, x22, [sp], #16
    ldp x23, x24, [sp], #16
    ldp x25, x26, [sp], #16
    ldp x27, x28, [sp], #16
    ldp x29, x30, [sp], #16
    eret

/* ============================================================
 * EL1h/EL2 FIQ Handler — Group 0 interrupts (timer via FMO)
 * At EL2 with HCR_EL2.FMO=1, Group 0 interrupts arrive as FIQ.
 * Use ICC_IAR0_EL1 for Group 0 acknowledge.
 * ============================================================ */
el1h_fiq:

/* ============================================================
 * SError — hang
 * ============================================================ */
el1h_serror:
    b panic_hang

/* ============================================================
 * Panic — infinite WFE loop
 * ============================================================ */
panic_hang:
    wfe
    b panic_hang
