/*
 * portASM.S — FreeRTOS AArch64 SMP Port (GICv3 SRE, EL1 GUEST)
 *
 * Based on FreeRTOS-Kernel/portable/GCC/ARM_AARCH64_SRE/portASM.S
 * Modified for SMP:
 *   - pxCurrentTCB → pxCurrentTCBs[core_id]
 *   - Single globals → per-core arrays (ulPortInterruptNestings[], etc.)
 *   - Critical nesting NOT saved on stack (portCRITICAL_NESTING_IN_TCB=1)
 *   - vTaskSwitchContext(xCoreID) takes core ID parameter
 */

    .text

    /* Variables and functions. */
    .extern ullMaxAPIPriorityMask
    .extern ullPortTCBCriticalNestingOffset
    .extern pxCurrentTCBs
    .extern vTaskSwitchContext
    .extern vApplicationIRQHandler
    .extern vPortFatalException
    .extern arch_vector_install
    .extern ulPortInterruptNestings
    .extern ulPortTaskHasFPUContexts
    .extern ulPortYieldRequireds
    .extern xPortCheckAndResetYield
#if ( configPORT_SMP_DIAGNOSTICS == 1 )
    .extern ullPortSvcEntries
    .extern ullPortSvcExits
    .extern ullPortSvcStages
    .extern ullPortSvcEsr
    .extern ullPortSyncAbortEsr
    .extern ullPortSyncAbortElr
    .extern ullPortSyncAbortFar
    .extern ullPortIrqSwitches
#endif
    .extern _freertos_vector_table
    .extern cherry_syscall_dispatch_trap
    .extern arch_fixup_alignment_fault

    .global FreeRTOS_IRQ_Handler
    .global FreeRTOS_SWI_Handler
    .global FreeRTOS_Abort
    .global vPortRestoreTaskContext

/* ==================================================================
 * Helper macro: get current core ID (Aff0 from MPIDR_EL1) into Xreg
 * ================================================================== */
.macro GET_CORE_ID Xreg
    MRS     \Xreg, MPIDR_EL1
    AND     \Xreg, \Xreg, #0xFF
    MRS     X17, MPIDR_EL1
    UBFX    X17, X17, #8, #8
    ADD     \Xreg, \Xreg, X17, LSL #4
.endm

/* ==================================================================
 * portSAVE_CONTEXT
 *
 * Saves the full task context on the task's stack (SP_EL0).
 * Stack frame layout (low to high):
 *   [SP] → FPU indicator (from per-core array)
 *          SPSR_EL1, ELR_EL1
 *          X30, XZR
 *          X28, X29 ... X0, X1
 *          (FPU registers if FPU context present)
 *   [SP+total] → pxCurrentTCBs[core_id]->pxTopOfStack = SP
 *
 * NOTE: Critical nesting is NOT saved here (managed by kernel in TCB).
 * ================================================================== */
.macro portSAVE_CONTEXT

    /* Switch to use the EL0 stack pointer. */
    MSR     SPSEL, #0

    /* Save the entire context. */
    STP     X0, X1, [SP, #-0x10]!
    STP     X2, X3, [SP, #-0x10]!
    STP     X4, X5, [SP, #-0x10]!
    STP     X6, X7, [SP, #-0x10]!
    STP     X8, X9, [SP, #-0x10]!
    STP     X10, X11, [SP, #-0x10]!
    STP     X12, X13, [SP, #-0x10]!
    STP     X14, X15, [SP, #-0x10]!
    STP     X16, X17, [SP, #-0x10]!
    STP     X18, X19, [SP, #-0x10]!
    STP     X20, X21, [SP, #-0x10]!
    STP     X22, X23, [SP, #-0x10]!
    STP     X24, X25, [SP, #-0x10]!
    STP     X26, X27, [SP, #-0x10]!
    STP     X28, X29, [SP, #-0x10]!
    /* Save X30 and TPIDR_EL0 (thread pointer for user/POSIX ELF tasks) */
    MRS     X1, TPIDR_EL0
    STP     X30, X1, [SP, #-0x10]!

    /* Save the SPSR and ELR (EL1 or EL2). */
    MRS     X1, CurrentEL
    TBZ     X1, #3, 11f
    MRS     X3, SPSR_EL2
    MRS     X2, ELR_EL2
    B       12f
11:
    MRS     X3, SPSR_EL1
    MRS     X2, ELR_EL1
12:
    STP     X2, X3, [SP, #-0x10]!

    /* Save the FPU context indicator from per-core array. */
    GET_CORE_ID  X0
    LDR     X1, =ulPortTaskHasFPUContexts
    LDR     X2, [X1, X0, LSL #3]     /* X2 = ulPortTaskHasFPUContexts[core_id] */

    /* Save the FPU context, if any */
    CMP     X2, #0
    B.EQ    1f
    STP     Q0, Q1, [SP,#-0x20]!
    STP     Q2, Q3, [SP,#-0x20]!
    STP     Q4, Q5, [SP,#-0x20]!
    STP     Q6, Q7, [SP,#-0x20]!
    STP     Q8, Q9, [SP,#-0x20]!
    STP     Q10, Q11, [SP,#-0x20]!
    STP     Q12, Q13, [SP,#-0x20]!
    STP     Q14, Q15, [SP,#-0x20]!
    STP     Q16, Q17, [SP,#-0x20]!
    STP     Q18, Q19, [SP,#-0x20]!
    STP     Q20, Q21, [SP,#-0x20]!
    STP     Q22, Q23, [SP,#-0x20]!
    STP     Q24, Q25, [SP,#-0x20]!
    STP     Q26, Q27, [SP,#-0x20]!
    STP     Q28, Q29, [SP,#-0x20]!
    STP     Q30, Q31, [SP,#-0x20]!
    MRS     X9, FPSR
    MRS     X10, FPCR
    STP     X9, X10, [SP, #-0x10]!
1:
    /* Store the FPU context indicator on stack. */
    STP     X2, XZR, [SP, #-0x10]!   /* FPU indicator + padding */

    /* Store SP into pxCurrentTCBs[core_id]->pxTopOfStack */
    GET_CORE_ID  X0
    LDR     X1, =pxCurrentTCBs
    LDR     X1, [X1, X0, LSL #3]     /* X1 = pxCurrentTCBs[core_id] */
    MOV     X0, SP
    STR     X0, [X1]

    /* Switch to use the ELx stack pointer. */
    MSR     SPSEL, #1

    .endm

/* ==================================================================
 * portRESTORE_CONTEXT
 *
 * Restores the full task context from pxCurrentTCBs[core_id].
 * Stack frame must match portSAVE_CONTEXT.
 *
 * NOTE: Critical nesting restored by kernel from TCB, not from stack.
 *       PMR is set based on kernel's per-TCB nesting count.
 * ================================================================== */
.macro portRESTORE_CONTEXT

    /* Switch to use the EL0 stack pointer. */
    MSR     SPSEL, #0

    /* Load SP from pxCurrentTCBs[core_id]->pxTopOfStack */
    GET_CORE_ID  X0
    LDR     X1, =pxCurrentTCBs
    LDR     X1, [X1, X0, LSL #3]     /* X1 = pxCurrentTCBs[core_id] */
    LDR     X0, [X1]                  /* X0 = pxTopOfStack */
    MOV     SP, X0

    /* Restore FPU indicator and padding */
    LDP     X2, X3, [SP], #0x10      /* X2 = FPU indicator */

    /* Set PMR based on kernel-managed critical nesting count.
     * portCRITICAL_NESTING_IN_TCB=1: kernel stores count in TCB.
     * We read it via the kernel macro equivalent:
     *   if (pxCurrentTCBs[core_id]->uxCriticalNesting == 0)
     *     PMR = 0xFF (unmasked)
     *   else
     *     PMR = ullMaxAPIPriorityMask (masked) */
    GET_CORE_ID  X4
    LDR     X5, =pxCurrentTCBs
    LDR     X5, [X5, X4, LSL #3]     /* X5 = TCB pointer */

    LDR     X6, =ullPortTCBCriticalNestingOffset
    LDR     X6, [X6]
    LDR     X7, [X5, X6]              /* X7 = pxCurrentTCB->uxCriticalNesting */
    CBZ     X7, 2f
    LDR     X6, =ullMaxAPIPriorityMask
    LDR     X1, [X6]                  /* Mask FreeRTOS API interrupt priorities */
    B       3f
2:
    MOV     X1, #255                   /* Unmask value */
3:
    MSR     s3_0_c4_c6_0, X1          /* ICC_PMR_EL1 */
    DSB     SY
    ISB     SY

    /* Restore per-core FPU context indicator */
    GET_CORE_ID  X0
    LDR     X1, =ulPortTaskHasFPUContexts
    STR     X2, [X1, X0, LSL #3]     /* ulPortTaskHasFPUContexts[core_id] = X2 */

    /* Restore FPU context if any */
    CMP     X2, #0
    B.EQ    1f
    LDP     X9, X10, [SP], #0x10
    LDP     Q30, Q31, [SP], #0x20
    LDP     Q28, Q29, [SP], #0x20
    LDP     Q26, Q27, [SP], #0x20
    LDP     Q24, Q25, [SP], #0x20
    LDP     Q22, Q23, [SP], #0x20
    LDP     Q20, Q21, [SP], #0x20
    LDP     Q18, Q19, [SP], #0x20
    LDP     Q16, Q17, [SP], #0x20
    LDP     Q14, Q15, [SP], #0x20
    LDP     Q12, Q13, [SP], #0x20
    LDP     Q10, Q11, [SP], #0x20
    LDP     Q8, Q9, [SP], #0x20
    LDP     Q6, Q7, [SP], #0x20
    LDP     Q4, Q5, [SP], #0x20
    LDP     Q2, Q3, [SP], #0x20
    LDP     Q0, Q1, [SP], #0x20
    MSR     FPSR, X9
    MSR     FPCR, X10
1:
    /* Restore SPSR and ELR */
    LDP     X2, X3, [SP], #0x10
    MRS     X1, CurrentEL
    TBZ     X1, #3, 13f
    MSR     SPSR_EL2, X3
    MSR     ELR_EL2, X2
    B       14f
13:
    MSR     SPSR_EL1, X3
    MSR     ELR_EL1, X2
14:

    /* Restore general purpose registers and TPIDR_EL0 */
    LDP     X30, X1, [SP], #0x10
    MSR     TPIDR_EL0, X1
    LDP     X28, X29, [SP], #0x10
    LDP     X26, X27, [SP], #0x10
    LDP     X24, X25, [SP], #0x10
    LDP     X22, X23, [SP], #0x10
    LDP     X20, X21, [SP], #0x10
    LDP     X18, X19, [SP], #0x10
    LDP     X16, X17, [SP], #0x10
    LDP     X14, X15, [SP], #0x10
    LDP     X12, X13, [SP], #0x10
    LDP     X10, X11, [SP], #0x10
    LDP     X8, X9, [SP], #0x10
    LDP     X6, X7, [SP], #0x10
    LDP     X4, X5, [SP], #0x10
    LDP     X2, X3, [SP], #0x10
    LDP     X0, X1, [SP], #0x10

    /* Switch to ELx stack */
    MSR     SPSEL, #1

    ERET

    .endm


/******************************************************************************
 * FreeRTOS_SWI_Handler — SVC for task yield (GUEST mode)
 *****************************************************************************/
.align 8
.type FreeRTOS_SWI_Handler, %function
FreeRTOS_SWI_Handler:
    MSR     DAIFSET, #2
    DSB     SY
    ISB     SY

    STP     X16, X17, [SP, #-0x10]!
    MRS     X16, CurrentEL
    TBZ     X16, #3, 21f
    MRS     X16, ESR_EL2
    B       22f
21:
    MRS     X16, ESR_EL1
22:
    LSR     X17, X16, #26
    CMP     X17, #0x15
    B.EQ    1f

    /* Check for Alignment Fault (Data Abort EC=0x25 / 0x24 with DFSC=0x21) */
    CMP     X17, #0x25
    B.EQ    61f
    CMP     X17, #0x24
    B.EQ    61f
    B       62f
61:
    AND     X17, X16, #0x3F
    CMP     X17, #0x21
    B.EQ    alignment_fixup_trap
62:

#if ( configPORT_SMP_DIAGNOSTICS == 1 )
    GET_CORE_ID  X15
    LDR     X0, =ullPortSyncAbortEsr
    STR     X16, [X0, X15, LSL #3]
    MRS     X1, CurrentEL
    TBZ     X1, #3, 51f
    MRS     X1, ELR_EL2
    LDR     X0, =ullPortSyncAbortElr
    STR     X1, [X0, X15, LSL #3]
    MRS     X1, FAR_EL2
    LDR     X0, =ullPortSyncAbortFar
    STR     X1, [X0, X15, LSL #3]
    B       52f
51:
    MRS     X1, ELR_EL1
    LDR     X0, =ullPortSyncAbortElr
    STR     X1, [X0, X15, LSL #3]
    MRS     X1, FAR_EL1
    LDR     X0, =ullPortSyncAbortFar
    STR     X1, [X0, X15, LSL #3]
52:
#endif
    B       FreeRTOS_Abort

alignment_fixup_trap:
    LDP     X16, X17, [SP], #0x10       /* Restore original X16, X17 */

    /* Save full syscall frame on SP_EL0 */
    MSR     SPSEL, #0
    SUB     SP, SP, #0x110
    STP     X0,  X1,  [SP, #0x00]
    STP     X2,  X3,  [SP, #0x10]
    STP     X4,  X5,  [SP, #0x20]
    STP     X6,  X7,  [SP, #0x30]
    STP     X8,  X9,  [SP, #0x40]
    STP     X10, X11, [SP, #0x50]
    STP     X12, X13, [SP, #0x60]
    STP     X14, X15, [SP, #0x70]
    STP     X16, X17, [SP, #0x80]
    STP     X18, X19, [SP, #0x90]
    STP     X20, X21, [SP, #0xA0]
    STP     X22, X23, [SP, #0xB0]
    STP     X24, X25, [SP, #0xC0]
    STP     X26, X27, [SP, #0xD0]
    STP     X28, X29, [SP, #0xE0]
    STR     X30,      [SP, #0xF0]

    ADD     X0,  SP,  #0x110
    STR     X0,       [SP, #0xF8]       /* sp */

    MRS     X18, CurrentEL
    TBZ     X18, #3, 71f
    MRS     X16, ELR_EL2
    MRS     X17, SPSR_EL2
    MRS     X1,  ESR_EL2
    MRS     X3,  FAR_EL2
    B       72f
71:
    MRS     X16, ELR_EL1
    MRS     X17, SPSR_EL1
    MRS     X1,  ESR_EL1
    MRS     X3,  FAR_EL1
72:
    STP     X16, X17, [SP, #0x100]      /* pc, pstate */

    /* Call arch_fixup_alignment_fault(frame, esr, elr, far) */
    MOV     X0, SP
    MOV     X2, X16
    BL      arch_fixup_alignment_fault
    CBZ     W0, alignment_fixup_failed

    /* Success: update ELR with advanced PC */
    LDR     X16, [SP, #0x100]
    MRS     X18, CurrentEL
    TBZ     X18, #3, 73f
    MSR     ELR_EL2, X16
    B       74f
73:
    MSR     ELR_EL1, X16
74:

    /* Restore registers and return */
    LDR     X16,      [SP, #0xF8]       /* Target SP (with any writeback applied) */
    MOV     X17, SP                     /* Frame pointer */
    MOV     SP,  X16                    /* Switch to target SP */
    LDR     X30,      [X17, #0xF0]
    LDP     X28, X29, [X17, #0xE0]
    LDP     X26, X27, [X17, #0xD0]
    LDP     X24, X25, [X17, #0xC0]
    LDP     X22, X23, [X17, #0xB0]
    LDP     X20, X21, [X17, #0xA0]
    LDP     X18, X19, [X17, #0x90]
    LDP     X14, X15, [X17, #0x70]
    LDP     X12, X13, [X17, #0x60]
    LDP     X10, X11, [X17, #0x50]
    LDP     X8,  X9,  [X17, #0x40]
    LDP     X6,  X7,  [X17, #0x30]
    LDP     X4,  X5,  [X17, #0x20]
    LDP     X2,  X3,  [X17, #0x10]
    LDP     X0,  X1,  [X17, #0x00]
    LDP     X16, X17, [X17, #0x80]
    MSR     SPSEL, #1
    ERET

alignment_fixup_failed:
    ADD     SP, SP, #0x110
    MSR     SPSEL, #1
    B       FreeRTOS_Abort

1:
    /* Check SVC immediate:
     * imm16 == 0  -> Linux POSIX system call (from glibc / musl / BusyBox ELF)
     * imm16 != 0  -> FreeRTOS internal task yield (portYIELD)
     */
    AND     X17, X16, #0xFFFF
    CBNZ    X17, 2f

    /* =========================================================================
     * Linux AArch64 System Call Trap Handler (SVC #0)
     * ========================================================================= */
    LDP     X16, X17, [SP], #0x10       /* Restore original X16, X17 */

    /* Switch to task stack (SP_EL0) for executing POSIX syscalls so interrupts & context switches are thread-safe */
    MSR     SPSEL, #0

    SUB     SP, SP, #0x110
    STP     X0,  X1,  [SP, #0x00]
    STP     X2,  X3,  [SP, #0x10]
    STP     X4,  X5,  [SP, #0x20]
    STP     X6,  X7,  [SP, #0x30]
    STP     X8,  X9,  [SP, #0x40]
    STP     X10, X11, [SP, #0x50]
    STP     X12, X13, [SP, #0x60]
    STP     X14, X15, [SP, #0x70]
    STP     X16, X17, [SP, #0x80]
    STP     X18, X19, [SP, #0x90]
    STP     X20, X21, [SP, #0xA0]
    STP     X22, X23, [SP, #0xB0]
    STP     X24, X25, [SP, #0xC0]
    STP     X26, X27, [SP, #0xD0]
    STP     X28, X29, [SP, #0xE0]
    STR     X30,      [SP, #0xF0]

    ADD     X0,  SP,  #0x110
    STR     X0,       [SP, #0xF8]       /* sp */

    MRS     X18, CurrentEL
    TBZ     X18, #3, 23f
    MRS     X16, ELR_EL2
    MRS     X17, SPSR_EL2
    B       24f
23:
    MRS     X16, ELR_EL1
    MRS     X17, SPSR_EL1
24:
    STP     X16, X17, [SP, #0x100]      /* pc, pstate */

    /* Pass struct syscall_frame * in X0 */
    MOV     X0, SP

    /* Enable interrupts during syscall execution */
    MSR     DAIFCLR, #2

    BL      cherry_syscall_dispatch_trap

    /* Disable interrupts for context restore */
    MSR     DAIFSET, #2

    /* Restore exception state */
    LDP     X16, X17, [SP, #0x100]
    MRS     X18, CurrentEL
    TBZ     X18, #3, 25f
    MSR     ELR_EL2, X16
    MSR     SPSR_EL2, X17
    B       26f
25:
    MSR     ELR_EL1, X16
    MSR     SPSR_EL1, X17
26:

    /* Restore general purpose registers */
    LDR     X30,      [SP, #0xF0]
    LDP     X28, X29, [SP, #0xE0]
    LDP     X26, X27, [SP, #0xD0]
    LDP     X24, X25, [SP, #0xC0]
    LDP     X22, X23, [SP, #0xB0]
    LDP     X20, X21, [SP, #0xA0]
    LDP     X18, X19, [SP, #0x90]
    LDP     X16, X17, [SP, #0x80]
    LDP     X14, X15, [SP, #0x70]
    LDP     X12, X13, [SP, #0x60]
    LDP     X10, X11, [SP, #0x50]
    LDP     X8,  X9,  [SP, #0x40]
    LDP     X6,  X7,  [SP, #0x30]
    LDP     X4,  X5,  [SP, #0x20]
    LDP     X2,  X3,  [SP, #0x10]
    LDP     X0,  X1,  [SP, #0x00]
    ADD     SP, SP, #0x110

    MSR     SPSEL, #1
    ERET

2:
    LDP     X16, X17, [SP], #0x10

    /* Save the context of the current task and select a new task. */
    portSAVE_CONTEXT

    GET_CORE_ID  X0
#if ( configPORT_SMP_DIAGNOSTICS == 1 )
    LDR     X1, =ullPortSvcStages
    MOV     X2, #1
    STR     X2, [X1, X0, LSL #3]
    LDR     X1, =ullPortSvcEntries
    LDR     X2, [X1, X0, LSL #3]
    ADD     X2, X2, #1
    STR     X2, [X1, X0, LSL #3]
#endif

    MRS     X1, CurrentEL
    TBZ     X1, #3, 27f
    MRS     X0, ESR_EL2
    B       28f
27:
    MRS     X0, ESR_EL1
28:
#if ( configPORT_SMP_DIAGNOSTICS == 1 )
    GET_CORE_ID  X3
    LDR     X4, =ullPortSvcEsr
    STR     X0, [X4, X3, LSL #3]
#endif
    LSR     X1, X0, #26
    CMP     X1, #0x15                  /* 0x15 = SVC instruction */
    B.NE    FreeRTOS_Abort

#if ( configPORT_SMP_DIAGNOSTICS == 1 )
    GET_CORE_ID  X0
    LDR     X1, =ullPortSvcStages
    MOV     X2, #2
    STR     X2, [X1, X0, LSL #3]
#endif

    /* Pass core ID to vTaskSwitchContext */
    GET_CORE_ID  X0
    BL      vTaskSwitchContext

#if ( configPORT_SMP_DIAGNOSTICS == 1 )
    GET_CORE_ID  X0
    LDR     X1, =ullPortSvcStages
    MOV     X2, #3
    STR     X2, [X1, X0, LSL #3]
    GET_CORE_ID  X0
    LDR     X1, =ullPortSvcExits
    LDR     X2, [X1, X0, LSL #3]
    ADD     X2, X2, #1
    STR     X2, [X1, X0, LSL #3]
#endif

    portRESTORE_CONTEXT

FreeRTOS_Abort:
    MRS     X4, CurrentEL
    TBZ     X4, #3, 41f
    MRS     X0, ESR_EL2
    MRS     X1, ELR_EL2
    MRS     X2, FAR_EL2
    MRS     X3, SPSR_EL2
    B       42f
41:
    MRS     X0, ESR_EL1
    MRS     X1, ELR_EL1
    MRS     X2, FAR_EL1
    MRS     X3, SPSR_EL1
42:
    MSR     SPSEL, #1
    BL      vPortFatalException
1:
    WFI
    B       1b

/******************************************************************************
 * vPortRestoreTaskContext — Start the scheduler on this core
 *
 * Installs _freertos_vector_table into VBAR_EL1, then restores
 * the first task context from pxCurrentTCBs[core_id].
 *****************************************************************************/
.align 8
.type vPortRestoreTaskContext, %function
vPortRestoreTaskContext:
    /* Install the FreeRTOS vector table */
    LDR     X0, =_freertos_vector_table
    BL      arch_vector_install

    /* Start the first task on this core */
    portRESTORE_CONTEXT


/******************************************************************************
 * FreeRTOS_IRQ_Handler — IRQ entry and exit (GICv3)
 *
 * Uses ICC_IAR1_EL1 (Group 1) for interrupt acknowledgment
 * and ICC_EOIR1_EL1 for end-of-interrupt.
 *
 * Per-core state: ulPortInterruptNestings[], ulPortYieldRequireds[]
 *****************************************************************************/
.align 8
.type FreeRTOS_IRQ_Handler, %function
FreeRTOS_IRQ_Handler:
    /* Save volatile registers */
    STP     X0, X1, [SP, #-0x10]!
    STP     X2, X3, [SP, #-0x10]!
    STP     X4, X5, [SP, #-0x10]!
    STP     X6, X7, [SP, #-0x10]!
    STP     X8, X9, [SP, #-0x10]!
    STP     X10, X11, [SP, #-0x10]!
    STP     X12, X13, [SP, #-0x10]!
    STP     X14, X15, [SP, #-0x10]!
    STP     X16, X17, [SP, #-0x10]!
    STP     X18, X19, [SP, #-0x10]!
    STP     X29, X30, [SP, #-0x10]!

    /* Save the SPSR and ELR */
    MRS     X1, CurrentEL
    TBZ     X1, #3, 31f
    MRS     X3, SPSR_EL2
    MRS     X2, ELR_EL2
    B       32f
31:
    MRS     X3, SPSR_EL1
    MRS     X2, ELR_EL1
32:
    STP     X2, X3, [SP, #-0x10]!

    /* Increment the per-core interrupt nesting counter.
     * X5 = &ulPortInterruptNestings[core_id] */
    GET_CORE_ID  X5
    LDR     X6, =ulPortInterruptNestings
    ADD     X5, X6, X5, LSL #3        /* X5 = &array[core_id] */
    LDR     X1, [X5]                   /* Old count */
    ADD     X7, X1, #1
    STR     X7, [X5]                   /* New count */

    /* Maintain nesting info across the function call */
    STP     X1, X5, [SP, #-0x10]!

    /* Read interrupt ID from ICC_IAR1_EL1 */
    MRS     X0, S3_0_C12_C12_0

    /* Maintain INTID across the function call */
    STP     X0, X1, [SP, #-0x10]!

    /* Call the C handler */
    BL      vApplicationIRQHandler

    /* Disable interrupts */
    MSR     DAIFSET, #2
    DSB     SY
    ISB     SY

    /* Restore INTID */
    LDP     X0, X1, [SP], #0x10

    /* End IRQ: write INTID to ICC_EOIR1_EL1 */
    MSR     S3_0_C12_C12_1, X0

    /* Restore nesting count */
    LDP     X1, X5, [SP], #0x10
    STR     X1, [X5]

    /* Has interrupt nesting unwound? */
    CMP     X1, #0
    B.NE    Exit_IRQ_No_Context_Switch

    /* Is a context switch required? (per-core yield flag & critical nesting) */
    GET_CORE_ID  X0
    BL      xPortCheckAndResetYield
    CMP     X0, #0
    B.EQ    Exit_IRQ_No_Context_Switch

    /* Restore volatile registers before context switch */
    LDP     X4, X5, [SP], #0x10       /* SPSR and ELR */
    MRS     X1, CurrentEL
    TBZ     X1, #3, 33f
    MSR     SPSR_EL2, X5
    MSR     ELR_EL2, X4
    B       34f
33:
    MSR     SPSR_EL1, X5
    MSR     ELR_EL1, X4
34:
    DSB     SY
    ISB     SY

    LDP     X29, X30, [SP], #0x10
    LDP     X18, X19, [SP], #0x10
    LDP     X16, X17, [SP], #0x10
    LDP     X14, X15, [SP], #0x10
    LDP     X12, X13, [SP], #0x10
    LDP     X10, X11, [SP], #0x10
    LDP     X8, X9, [SP], #0x10
    LDP     X6, X7, [SP], #0x10
    LDP     X4, X5, [SP], #0x10
    LDP     X2, X3, [SP], #0x10
    LDP     X0, X1, [SP], #0x10

    /* Save context, switch task, restore new context */
    portSAVE_CONTEXT

    /* Pass core ID to vTaskSwitchContext */
    GET_CORE_ID  X0
    BL      vTaskSwitchContext

    portRESTORE_CONTEXT

Exit_IRQ_No_Context_Switch:
    /* Restore volatile registers and return */
    LDP     X4, X5, [SP], #0x10       /* SPSR and ELR */
    MRS     X1, CurrentEL
    TBZ     X1, #3, 35f
    MSR     SPSR_EL2, X5
    MSR     ELR_EL2, X4
    B       36f
35:
    MSR     SPSR_EL1, X5
    MSR     ELR_EL1, X4
36:
    DSB     SY
    ISB     SY

    LDP     X29, X30, [SP], #0x10
    LDP     X18, X19, [SP], #0x10
    LDP     X16, X17, [SP], #0x10
    LDP     X14, X15, [SP], #0x10
    LDP     X12, X13, [SP], #0x10
    LDP     X10, X11, [SP], #0x10
    LDP     X8, X9, [SP], #0x10
    LDP     X6, X7, [SP], #0x10
    LDP     X4, X5, [SP], #0x10
    LDP     X2, X3, [SP], #0x10
    LDP     X0, X1, [SP], #0x10

    ERET

    .end
