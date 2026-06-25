/*
 * portmacro.h — SMP-aware port macros for FreeRTOS on AArch64 (GICv3 SRE)
 *
 * This file is found via -Iinclude before the kernel's copy.
 * It includes the kernel's original portmacro.h using a relative path
 * to get all standard definitions, then overrides what SMP needs.
 */

#ifndef PORTMACRO_H_SMP
#define PORTMACRO_H_SMP

/* Include the kernel's original portmacro.h with all standard definitions.
 * #include_next skips this file (include/portmacro.h) and finds the next
 * portmacro.h in the include path (kernel/portable/GCC/ARM_AARCH64_SRE/). */
#include_next "portmacro.h"

/* ================================================================
 * SMP Overrides — only needed when configNUMBER_OF_CORES > 1
 * ================================================================ */

#if (configNUMBER_OF_CORES > 1)

    /* Critical sections: use kernel's SMP-aware vTaskEnterCritical
     * which acquires our spinlocks. Override the single-core versions
     * from the original portmacro.h. */
    #ifdef portENTER_CRITICAL
        #undef portENTER_CRITICAL
    #endif
    #ifdef portEXIT_CRITICAL
        #undef portEXIT_CRITICAL
    #endif
    #define portENTER_CRITICAL()    vTaskEnterCritical()
    #define portEXIT_CRITICAL()     vTaskExitCritical()

    /* Override SET/CLEAR_INTERRUPT_MASK to use our per-core PMR implementations.
     * The kernel's portmacro.h defines these first; we undef and redefine. */
    #undef portSET_INTERRUPT_MASK_FROM_ISR
    #undef portCLEAR_INTERRUPT_MASK_FROM_ISR
    #define portSET_INTERRUPT_MASK_FROM_ISR()           uxPortSetInterruptMask()
    #define portCLEAR_INTERRUPT_MASK_FROM_ISR(x)        vPortClearInterruptMask(x)

    #undef portEND_SWITCHING_ISR
    #undef portYIELD_FROM_ISR
    #define portEND_SWITCHING_ISR(xSwitchRequired)                                      \
        do {                                                                           \
            if ((xSwitchRequired) != pdFALSE) {                                         \
                ulPortYieldRequireds[portGET_CORE_ID()] = pdTRUE;                      \
            }                                                                          \
        } while (0)
    #define portYIELD_FROM_ISR(x)    portEND_SWITCHING_ISR(x)

#endif /* configNUMBER_OF_CORES > 1 */

#endif /* PORTMACRO_H_SMP */
