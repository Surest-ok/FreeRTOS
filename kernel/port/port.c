/*
 * port.c — FreeRTOS AArch64 SMP Port (GICv3 SRE, EL1 GUEST)
 *
 * Based on FreeRTOS-Kernel/portable/GCC/ARM_AARCH64_SRE/port.c
 * Modified for SMP: per-core state arrays, portCRITICAL_NESTING_IN_TCB=1.
 *
 * Single-core globals replaced with arrays indexed by core_id:
 *   ulPortInterruptNestings[]   — defined in port_smp.c
 *   ulPortYieldRequireds[]     — defined in port_smp.c
 *   ulPortTaskHasFPUContexts[]  — defined in port_smp.c
 *   ullCriticalNesting         — removed (managed by kernel in TCB)
 */

#include <stdlib.h>
#include "FreeRTOS.h"
#include "task.h"
#include "port_smp.h"

#ifndef configUNIQUE_INTERRUPT_PRIORITIES
    #error "configUNIQUE_INTERRUPT_PRIORITIES must be defined."
#endif
#ifndef configSETUP_TICK_INTERRUPT
    #error "configSETUP_TICK_INTERRUPT() must be defined."
#endif
#ifndef configMAX_API_CALL_INTERRUPT_PRIORITY
    #error "configMAX_API_CALL_INTERRUPT_PRIORITY must be defined."
#endif

#if configMAX_API_CALL_INTERRUPT_PRIORITY == 0
    #error "configMAX_API_CALL_INTERRUPT_PRIORITY must not be set to 0"
#endif
#if configMAX_API_CALL_INTERRUPT_PRIORITY > configUNIQUE_INTERRUPT_PRIORITIES
    #error "configMAX_API_CALL_INTERRUPT_PRIORITY must be <= configUNIQUE_INTERRUPT_PRIORITIES"
#endif
#if ( configUSE_PORT_OPTIMISED_TASK_SELECTION == 1 ) && ( configMAX_PRIORITIES > 32 )
    #error "configMAX_PRIORITIES must be <= 32 with optimised task selection"
#endif
#if configMAX_API_CALL_INTERRUPT_PRIORITY <= ( configUNIQUE_INTERRUPT_PRIORITIES / 2 )
    #error "configMAX_API_CALL_INTERRUPT_PRIORITY must be > (configUNIQUE_INTERRUPT_PRIORITIES / 2)"
#endif

#ifndef configCLEAR_TICK_INTERRUPT
    #define configCLEAR_TICK_INTERRUPT()
#endif

/* --------------------------------------------------------------- */

#define portNO_CRITICAL_NESTING       ( ( size_t ) 0 )
#define portUNMASK_VALUE              ( 0xFFUL )
#define portNO_FLOATING_POINT_CONTEXT ( ( StackType_t ) 0 )

#define portSP_ELx   ( ( StackType_t ) 0x01 )
#define portSP_EL0   ( ( StackType_t ) 0x00 )
#define portEL1      ( ( StackType_t ) 0x04 )
#define portEL2      ( ( StackType_t ) 0x08 )

static inline StackType_t port_get_initial_pstate(void)
{
    uint64_t current_el;
    __asm__ volatile("mrs %0, CurrentEL" : "=r"(current_el));
    if (((current_el >> 2) & 0x3) == 2) {
        return (StackType_t)(portEL2 | portSP_EL0);
    }
    return (StackType_t)(portEL1 | portSP_EL0);
}

#define portAPSR_MODE_BITS_MASK  ( 0x0C )
#define portDAIF_I               ( 0x80 )
#define portFPU_REGISTER_WORDS   ( (32 * 2) + 2 )

/* Zero-argument version used internally by port.c — writes PMR = 0xFF */
#undef portCLEAR_INTERRUPT_MASK
#define portCLEAR_INTERRUPT_MASK()                     \
    {                                                  \
        __asm__ volatile("MSR DAIFSET, #2\n"           \
                         "DSB SY\n"                    \
                         "ISB SY\n"                    \
                         "MSR s3_0_c4_c6_0, %0\n"     \
                         "DSB SY\n"                    \
                         "ISB SY\n"                    \
                         "MSR DAIFCLR, #2\n"           \
                         "DSB SY\n"                    \
                         "ISB SY\n"                    \
                         ::"r"(portUNMASK_VALUE));      \
    }

/* --------------------------------------------------------------- */

extern void vPortRestoreTaskContext(void);

void vApplicationFPUSafeIRQHandler(uint32_t ulICCIAR) __attribute__((weak));

/* ullMaxAPIPriorityMask — used by portASM.S */
__attribute__((used)) const uint64_t ullMaxAPIPriorityMask =
    ( configMAX_API_CALL_INTERRUPT_PRIORITY << portPRIORITY_SHIFT );

/* ================================================================
 * pxPortInitialiseStack
 *
 * With portCRITICAL_NESTING_IN_TCB=1, the kernel manages critical
 * nesting in the TCB. We do NOT push a nesting slot to the stack.
 * Stack layout for configUSE_TASK_FPU_SUPPORT=1 (no FPU):
 *   [high addr]
 *     FPU indicator (0 = no FPU context)
 *     ELR_EL1 (task entry point)
 *     SPSR_EL1 (portINITIAL_PSTATE)
 *     X30 (LR = 0)
 *     XZR (padding)
 *     X28, X29
 *     ...
 *     X0, X1 (pvParameters, filler)
 *   [low addr = pxTopOfStack return value]
 * ================================================================ */

StackType_t *pxPortInitialiseStack(StackType_t *pxTopOfStack,
                                   TaskFunction_t pxCode,
                                   void *pvParameters)
{
    /* General purpose registers (descending) */
    pxTopOfStack--;
    *pxTopOfStack = 0x0101010101010101ULL;        /* X1 */
    pxTopOfStack--;
    *pxTopOfStack = (StackType_t)pvParameters;    /* X0 */
    pxTopOfStack--;
    *pxTopOfStack = 0x0303030303030303ULL;        /* X3 */
    pxTopOfStack--;
    *pxTopOfStack = 0x0202020202020202ULL;        /* X2 */
    pxTopOfStack--;
    *pxTopOfStack = 0x0505050505050505ULL;        /* X5 */
    pxTopOfStack--;
    *pxTopOfStack = 0x0404040404040404ULL;        /* X4 */
    pxTopOfStack--;
    *pxTopOfStack = 0x0707070707070707ULL;        /* X7 */
    pxTopOfStack--;
    *pxTopOfStack = 0x0606060606060606ULL;        /* X6 */
    pxTopOfStack--;
    *pxTopOfStack = 0x0909090909090909ULL;        /* X9 */
    pxTopOfStack--;
    *pxTopOfStack = 0x0808080808080808ULL;        /* X8 */
    pxTopOfStack--;
    *pxTopOfStack = 0x1111111111111111ULL;        /* X11 */
    pxTopOfStack--;
    *pxTopOfStack = 0x1010101010101010ULL;        /* X10 */
    pxTopOfStack--;
    *pxTopOfStack = 0x1313131313131313ULL;        /* X13 */
    pxTopOfStack--;
    *pxTopOfStack = 0x1212121212121212ULL;        /* X12 */
    pxTopOfStack--;
    *pxTopOfStack = 0x1515151515151515ULL;        /* X15 */
    pxTopOfStack--;
    *pxTopOfStack = 0x1414141414141414ULL;        /* X14 */
    pxTopOfStack--;
    *pxTopOfStack = 0x1717171717171717ULL;        /* X17 */
    pxTopOfStack--;
    *pxTopOfStack = 0x1616161616161616ULL;        /* X16 */
    pxTopOfStack--;
    *pxTopOfStack = 0x1919191919191919ULL;        /* X19 */
    pxTopOfStack--;
    *pxTopOfStack = 0x1818181818181818ULL;        /* X18 */
    pxTopOfStack--;
    *pxTopOfStack = 0x2121212121212121ULL;        /* X21 */
    pxTopOfStack--;
    *pxTopOfStack = 0x2020202020202020ULL;        /* X20 */
    pxTopOfStack--;
    *pxTopOfStack = 0x2323232323232323ULL;        /* X23 */
    pxTopOfStack--;
    *pxTopOfStack = 0x2222222222222222ULL;        /* X22 */
    pxTopOfStack--;
    *pxTopOfStack = 0x2525252525252525ULL;        /* X25 */
    pxTopOfStack--;
    *pxTopOfStack = 0x2424242424242424ULL;        /* X24 */
    pxTopOfStack--;
    *pxTopOfStack = 0x2727272727272727ULL;        /* X27 */
    pxTopOfStack--;
    *pxTopOfStack = 0x2626262626262626ULL;        /* X26 */
    pxTopOfStack--;
    *pxTopOfStack = 0x2929292929292929ULL;        /* X29 */
    pxTopOfStack--;
    *pxTopOfStack = 0x2828282828282828ULL;        /* X28 */
    pxTopOfStack--;
    *pxTopOfStack = (StackType_t)0x00;            /* XZR padding */
    pxTopOfStack--;
    *pxTopOfStack = (StackType_t)0x00;            /* X30 (LR) */

    /* SPSR and ELR */
    pxTopOfStack--;
    *pxTopOfStack = port_get_initial_pstate();    /* SPSR */
    pxTopOfStack--;
    *pxTopOfStack = (StackType_t)pxCode;          /* ELR (task entry) */

    /* FPU context indicator — no FPU context for this task.
     * With portCRITICAL_NESTING_IN_TCB=1, we do NOT push a
     * critical nesting slot. Push FPU indicator + padding
     * to match portSAVE_CONTEXT's STP X2, XZR layout. */
    pxTopOfStack--;
    *pxTopOfStack = portNO_FLOATING_POINT_CONTEXT;
    pxTopOfStack--;
    *pxTopOfStack = (StackType_t)0;  /* Padding (was critical nesting) */

    return pxTopOfStack;
}

/* ================================================================
 * xPortStartScheduler
 * ================================================================ */

BaseType_t xPortStartScheduler(void)
{
    uint32_t ulAPSR;

    __asm__ volatile("MRS %0, CurrentEL" : "=r"(ulAPSR));
    ulAPSR &= portAPSR_MODE_BITS_MASK;

    configASSERT((ulAPSR == portEL1) || (ulAPSR == portEL2));

    if ((ulAPSR == portEL1) || (ulAPSR == portEL2))
    {
        /* Disable interrupts during scheduler startup */
        portDISABLE_INTERRUPTS();

        /* Only core 0 sets up the tick interrupt.
         * Secondary cores get their scheduling via SGI IPI. */
        if (portGET_CORE_ID() == 0)
        {
            configSETUP_TICK_INTERRUPT();
        }

        vPortSetCoreActive(portGET_CORE_ID());

        /* Install vector table and start first task.
         * vPortRestoreTaskContext writes _freertos_vector_table to VBAR_EL1,
         * then restores context from pxCurrentTCBs[core_id]. */
        vPortRestoreTaskContext();
    }

    return 0;
}

/* ================================================================
 * vPortEndScheduler — not implemented for bare-metal
 * ================================================================ */

void vPortEndScheduler(void)
{
    configASSERT(0);
}

/* ================================================================
 * FreeRTOS_Tick_Handler
 * ================================================================ */

void FreeRTOS_Tick_Handler(void)
{
    BaseType_t core_id = portGET_CORE_ID();

    /* Must be the lowest possible priority (skip on QEMU — no ICC_RPR_EL1) */
    #if !defined(QEMU)
    {
        uint64_t ullRunningInterruptPriority;
        __asm__ volatile("MRS %0, s3_0_c12_c11_3" : "=r"(ullRunningInterruptPriority));
        configASSERT(ullRunningInterruptPriority ==
                     (portLOWEST_USABLE_INTERRUPT_PRIORITY << portPRIORITY_SHIFT));
    }
    #endif

    /* Interrupts should not be enabled before this point */
    #if (configASSERT_DEFINED == 1)
    {
        uint32_t ulMaskBits;
        __asm__ volatile("MRS %0, DAIF" : "=r"(ulMaskBits)::"memory");
        configASSERT((ulMaskBits & portDAIF_I) != 0);
    }
    #endif

    /* Mask interrupts up to max syscall priority */
    __asm__ volatile(
        "MSR s3_0_c4_c6_0, %0\n"
        "DSB SY\n"
        "ISB SY\n"
        :: "r"(configMAX_API_CALL_INTERRUPT_PRIORITY << portPRIORITY_SHIFT)
        : "memory"
    );

    /* Clear tick interrupt source, then enable interrupts */
    configCLEAR_TICK_INTERRUPT();
    portENABLE_INTERRUPTS();

    #if (configNUMBER_OF_CORES > 1)
    {
        /* SMP: xTaskIncrementTick calls prvYieldForTask which requires
         * a critical section. Use proper ISR critical section which
         * acquires the ISR spinlock and manages nesting count. */
        UBaseType_t uxSavedStatus = taskENTER_CRITICAL_FROM_ISR();
        if (xTaskIncrementTick() != pdFALSE)
        {
            ulPortYieldRequireds[core_id] = pdTRUE;
        }
        taskEXIT_CRITICAL_FROM_ISR(uxSavedStatus);
        #if ( configSMP_TICK_BROADCAST == 1 )
        {
            vPortBroadcastTick();
        }
        #endif
    }
    #else
    {
        /* Increment the RTOS tick */
        if (xTaskIncrementTick() != pdFALSE)
        {
            ulPortYieldRequireds[core_id] = pdTRUE;
        }
    }
    #endif

    /* Restore all interrupt priorities */
    portCLEAR_INTERRUPT_MASK();
}

/* ================================================================
 * Interrupt Mask (per-core PMR)
 * ================================================================ */

void vPortClearInterruptMask(UBaseType_t uxNewMaskValue)
{
    if (uxNewMaskValue == pdFALSE)
    {
        portCLEAR_INTERRUPT_MASK();
    }
}

UBaseType_t uxPortSetInterruptMask(void)
{
    uint32_t ulReturn;
    uint64_t ullPMRValue;

    portDISABLE_INTERRUPTS();
    __asm__ volatile("MRS %0, s3_0_c4_c6_0" : "=r"(ullPMRValue));

    if (ullPMRValue == (configMAX_API_CALL_INTERRUPT_PRIORITY << portPRIORITY_SHIFT))
    {
        ulReturn = pdTRUE; /* Already masked */
    }
    else
    {
        ulReturn = pdFALSE;
        __asm__ volatile(
            "MSR s3_0_c4_c6_0, %0\n"
            "DSB SY\n"
            "ISB SY\n"
            :: "r"(configMAX_API_CALL_INTERRUPT_PRIORITY << portPRIORITY_SHIFT)
            : "memory"
        );
    }

    portENABLE_INTERRUPTS();
    return ulReturn;
}

/* ================================================================
 * vPortTaskUsesFPU (configUSE_TASK_FPU_SUPPORT == 1)
 * ================================================================ */

#if (configUSE_TASK_FPU_SUPPORT != 2)
void vPortTaskUsesFPU(void)
{
    BaseType_t core_id = portGET_CORE_ID();
    ulPortTaskHasFPUContexts[core_id] = pdTRUE;
}
#endif

/* ================================================================
 * Interrupt Priority Validation
 * ================================================================ */

#if (configASSERT_DEFINED == 1)
void vPortValidateInterruptPriority(void)
{
    uint64_t ullRunningInterruptPriority;
    __asm__ volatile("MRS %0, s3_0_c12_c11_3" : "=r"(ullRunningInterruptPriority));
    configASSERT(ullRunningInterruptPriority >=
                 (configMAX_API_CALL_INTERRUPT_PRIORITY << portPRIORITY_SHIFT));
}
#endif

/* ================================================================
 * Weak FPU-safe IRQ handler (not used — app provides vApplicationIRQHandler)
 * ================================================================ */

void vApplicationFPUSafeIRQHandler(uint32_t ulICCIAR)
{
    (void)ulICCIAR;
    configASSERT((volatile void *)NULL);
}
