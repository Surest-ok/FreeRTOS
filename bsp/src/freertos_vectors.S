/*
 * FreeRTOS Exception Vector Table for AArch64 EL1 (GUEST mode)
 *
 * Provides _freertos_vector_table symbol required by portASM.S.
 *
 * IMPORTANT: FreeRTOS tasks run with SPSEL=0 (SP_EL0). When an exception
 * occurs during a task, the CPU uses the SP_EL0 handler slots (+0x000..0x180).
 * We MUST route both SP_EL0 and SP_ELx entries to the same FreeRTOS handlers.
 */

.section ".text.vectors", "ax", %progbits
.align 11

.macro ventry label
    .align 7
    b \label
.endm

.global _freertos_vector_table

_freertos_vector_table:
    /* Current EL with SP_EL0 — used by FreeRTOS tasks (SPSEL=0) */
    ventry  FreeRTOS_SWI_Handler  /* +0x000 Sync  (SVC = task yield) */
    ventry  FreeRTOS_IRQ_Handler  /* +0x080 IRQ   (GICv3 interrupts) */
    ventry  hang_stub             /* +0x100 FIQ */
    ventry  hang_stub             /* +0x180 SError */

    /* Current EL with SP_ELx (SP_EL1) — used by exception handlers */
    ventry  FreeRTOS_SWI_Handler  /* +0x200 Sync  (SVC = task yield) */
    ventry  FreeRTOS_IRQ_Handler  /* +0x280 IRQ   (GICv3 interrupts) */
    ventry  hang_stub             /* +0x300 FIQ */
    ventry  hang_stub             /* +0x380 SError */

    /* Lower EL AArch64 — unused */
    ventry  hang_stub
    ventry  hang_stub
    ventry  hang_stub
    ventry  hang_stub

    /* Lower EL AArch32 — unused */
    ventry  hang_stub
    ventry  hang_stub
    ventry  hang_stub
    ventry  hang_stub

hang_stub:
    b FreeRTOS_Abort
