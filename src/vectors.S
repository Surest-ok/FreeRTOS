/*
 * Exception vector table for AArch64 EL1
 * Each vector is 128 bytes (256-byte alignment)
 * Based on ARMv8-A reference architecture
 */

.section ".text.vectors", "ax", %alloc
.align 11  /* 2048-byte alignment */

.global vectors
.global vector_table

/* Exception vector table */
vector_table:
    /* EL0t Sync */
    ventry  vector_synchronous
    ventry  vector_irq
    ventry  vector_fiq
    ventry  vector_error

    /* EL1t Sync */
    ventry  vector_synchronous
    ventry  vector_irq
    ventry  vector_fiq
    ventry  vector_error

    /* EL2t Sync */
    ventry  vector_synchronous
    ventry  vector_irq
    ventry  vector_fiq
    ventry  vector_error

    /* EL3t Sync */
    ventry  vector_synchronous
    ventry  vector_irq
    ventry  vector_fiq
    ventry  vector_error

    /* EL0h Sync */
    ventry  vector_synchronous
    ventry  vector_irq
    ventry  vector_fiq
    ventry  vector_error

    /* EL1h Sync */
    ventry  vector_synchronous
    ventry  vector_irq
    ventry  vector_fiq
    ventry  vector_error

    /* EL2h Sync */
    ventry  vector_synchronous
    ventry  vector_irq
    ventry  vector_fiq
    ventry  vector_error

    /* EL3h Sync */
    ventry  vector_synchronous
    ventry  vector_irq
    ventry  vector_fiq
    ventry  vector_error

/* Vector handlers - each 128 bytes (256-byte aligned) */

/* Synchronous exception handler */
vector_synchronous:
    /* Save context and call handler */
    mrs x1, esr_el1    /* Get exception syndrome register */
    mov x0, #0x80      /* Sync exception type */
    b exception_dispatch

/* IRQ handler - this is the main interrupt handler */
vector_irq:
    /* Save full context */
    stp x29, x30, [sp, #-16]!   /* Save LR and FP */
    mrs x29, sp_el0             /* Save SP */
    stp x0, x1, [sp, #-16]!     /* Save general registers */
    stp x2, x3, [sp, #-16]!
    stp x4, x5, [sp, #-16]!
    stp x6, x7, [sp, #-16]!
    stp x8, x9, [sp, #-16]!
    stp x10, x11, [sp, #-16]!
    stp x12, x13, [sp, #-16]!
    stp x14, x15, [sp, #-16]!
    stp x16, x17, [sp, #-16]!
    stp x18, x19, [sp, #-16]!
    stp x20, x21, [sp, #-16]!
    stp x22, x23, [sp, #-16]!
    stp x24, x25, [sp, #-16]!
    stp x26, x27, [sp, #-16]!
    stp x28, x29, [sp, #-16]!

    /* Get IRQ ID from GIC IAR */
    mrs x0, ICC_IAR1_EL1

    /* Call interrupt dispatcher */
    mov x1, sp      /* Pass context pointer */
    bl interrupt_dispatch

    /* Acknowledge interrupt */
    mrs x2, ICC_EOIR1_EL1

    /* Restore context */
    ldp x28, x29, [sp], #16
    ldp x26, x27, [sp], #16
    ldp x24, x25, [sp], #16
    ldp x22, x23, [sp], #16
    ldp x20, x21, [sp], #16
    ldp x18, x19, [sp], #16
    ldp x16, x17, [sp], #16
    ldp x14, x15, [sp], #16
    ldp x12, x13, [sp], #16
    ldp x10, x11, [sp], #16
    ldp x8, x9, [sp], #16
    ldp x6, x7, [sp], #16
    ldp x4, x5, [sp], #16
    ldp x2, x3, [sp], #16
    ldp x0, x1, [sp], #16
    ldp x29, x30, [sp], #16
    msr sp_el0, x29

    eret

/* FIQ handler */
vector_fiq:
    /* For now, just handle as error */
    b panic

/* Error handler */
vector_error:
    /* For now, just handle as error */
    b panic

/* Exception dispatcher */
exception_dispatch:
    /* x0 = exception type, x1 = ESR (if sync) or interrupt ID */
    cmp x0, #0x80
    b.eq handle_sync_exception
    cmp x0, #0x100
    b.eq handle_irq_exception
    /* Unknown exception */
    b panic

/* Handle synchronous exception */
handle_sync_exception:
    /* For now, just panic */
    b panic

/* Handle IRQ exception */
handle_irq_exception:
    /* x0 contains the interrupt ID from ICC_IAR1_EL1 */
    /* Get the low 9 bits for the interrupt number */
    and x1, x0, #0x1ff

    /* Check if it's the timer interrupt (PPI 30 = ID 30 - 16 = 14) */
    cmp x1, #14
    b.ne handle_other_irq

    /* Timer interrupt */
    bl timer_handler
    b exit_exception

handle_other_irq:
    /* For now, just acknowledge and return */
    b exit_exception

/* Exit from exception */
exit_exception:
    eret

/* Panic handler - infinite loop */
panic:
    mrs x0, CurrentEL
    msr sp_el0, x0
    b .