/*
 * Exception Vector Table for AArch64 EL1
 * Phase 2: QEMU virt GICv3 + Generic Timer
 *
 * ARMv8-A vector table layout (2048 bytes, 2KB aligned):
 *   Each exception level has 4 entry types (Sync/IRQ/FIQ/SError)
 *   x 2 SP modes (SP_EL0 / SP_ELx) = 8 groups x 4 entries = 16 entries
 *   Each entry is 128 bytes aligned
 */

.section ".text.vectors", "ax", %progbits
.align 11  /* 2048-byte alignment for VBAR_EL1 */

.global vector_table

/*
 * ventry macro: align to 128 bytes and branch to handler
 * Each vector slot is 0x80 (128) bytes
 */
.macro ventry label
    .align 7
    b \label
.endm

vector_table:
    /* Group 0: Current EL with SP_EL0 (we don't use SP_EL0, just trap) */
    ventry  el1_sp0_stub         /* Synchronous */
    ventry  el1_sp0_stub         /* IRQ */
    ventry  el1_sp0_stub         /* FIQ */
    ventry  el1_sp0_stub         /* SError */

    /* Group 1: Current EL with SP_ELx (SP_EL1) - THIS IS THE MAIN ONE */
    ventry  el1h_sync_handler    /* Synchronous */
    ventry  el1h_irq_handler     /* IRQ ← GICv3 interrupts arrive here */
    ventry  el1h_fiq_stub        /* FIQ */
    ventry  el1h_error_stub      /* SError */

    /* Group 2: Lower EL with AArch64 */
    ventry  lower_el_sync_stub
    ventry  lower_el_irq_stub
    ventry  lower_el_fiq_stub
    ventry  lower_el_error_stub

    /* Group 3: Lower EL with AArch32 */
    ventry  lower_el_sync_stub
    ventry  lower_el_irq_stub
    ventry  lower_el_fiq_stub
    ventry  lower_el_error_stub

/* ============================================================
 * SP_EL0 stubs (we don't use SP_EL0, so these are just traps)
 * ============================================================ */
el1_sp0_stub:
    b panic_exception

/* ============================================================
 * EL1h handlers (SP_EL1) - our main exception handlers
 * ============================================================ */

/* EL1h Synchronous Exception Handler */
el1h_sync_handler:
    mrs x0, esr_el1
    mrs x1, elr_el1
    b panic_exception

/* EL1h IRQ Handler - Main interrupt entry point
 *
 * Uses GICv2 memory-mapped CPU Interface:
 *   GICC_IAR (0x0801000C) to acknowledge interrupt
 *   GICC_EOIR (0x08010010) to signal end of interrupt
 *
 * C ABI: x0-x18 are caller-saved, x19-x30 are callee-saved.
 * We save ALL general registers to use C freely.
 */
el1h_irq_handler:
    /* Save context on stack (SP_EL1, which we set in startup.S) */
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

    /* Read GICC_IAR to acknowledge the interrupt
     * GICC base = 0x08010000, IAR offset = 0x0C
     */
    ldr x1, =0x0801000C
    ldr w0, [x1]

    /* Call C dispatcher: interrupt_dispatch(irq_id) */
    bl interrupt_dispatch

    /* Write GICC_EOIR to signal end of interrupt
     * GICC base = 0x08010000, EOIR offset = 0x10
     * x0 still contains the IRQ ID from the dispatcher return
     */
    ldr x1, =0x08010010
    str w0, [x1]

    /* Restore context */
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

/* EL1h FIQ and SError stubs */
el1h_fiq_stub:
el1h_error_stub:
    b panic_exception

/* Lower EL stubs (we stay at EL1, so these shouldn't fire) */
lower_el_sync_stub:
lower_el_irq_stub:
lower_el_fiq_stub:
lower_el_error_stub:
    b panic_exception

/* ============================================================
 * Panic handler - print exception info and hang
 * ============================================================ */
panic_exception:
    /* At this point we're in exception context with SP_EL1.
     * We can call C functions since the IRQ handler saved
     * context properly, but for panic we just halt.
     */
    wfe
    b panic_exception
