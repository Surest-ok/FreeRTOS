/*
 * port_smp.h — SMP Port Macros for FreeRTOS on AArch64 (GICv3 SRE)
 *
 * This file is included from FreeRTOSConfig.h, which is itself included
 * from FreeRTOS.h BEFORE portmacro.h/types are available. Therefore:
 *   - Use only basic C types (int, unsigned long, etc.)
 *   - Define only preprocessor macros and struct typedefs
 *   - Do NOT override portENTER_CRITICAL here (done in portmacro.h instead)
 *
 * Provides macros required when configNUMBER_OF_CORES > 1.
 */

#ifndef PORT_SMP_H
#define PORT_SMP_H

#include "arch_api/arch.h"
#include "arch_api/spinlock.h"

/* ======= Core Identification ======= */

#define portGET_CORE_ID()    ((int)arch_curr_cpu_id())

/* ======= Inter-Processor Interrupt (IPI) ======= */

extern int vPortYieldCore(long xCoreID);
extern void vPortBroadcastTick(void);
extern void vPortSetCoreActive(long xCoreID);
extern void vPortSetCoreIdle(long xCoreID, int idle);
extern unsigned long vPortGetActiveCoreMask(void);
extern unsigned long vPortGetIdleCoreMask(void);
extern unsigned long vPortGetYieldIPICount(long xCoreID);
extern unsigned long vPortGetSwitchCount(long xCoreID);
#define portYIELD_CORE(x)    vPortYieldCore(x)

/* ======= Recursive Spinlocks ======= */

typedef arch_recursive_spinlock_t Spinlock_t;
typedef arch_raw_spinlock_t RawSpinlock_t;

extern void vPortRecursiveLockAcquire(Spinlock_t *pxLock);
extern void vPortRecursiveLockRelease(Spinlock_t *pxLock);
extern void vPortRawSpinLockAcquire(RawSpinlock_t *pxLock);
extern void vPortRawSpinLockRelease(RawSpinlock_t *pxLock);

extern Spinlock_t xTaskSpinlock;
extern Spinlock_t xISRSpinlock;

#define portGET_TASK_LOCK(xCoreID)      vPortRecursiveLockAcquire(&xTaskSpinlock)
#define portRELEASE_TASK_LOCK(xCoreID)  vPortRecursiveLockRelease(&xTaskSpinlock)
#define portGET_ISR_LOCK(xCoreID)       vPortRecursiveLockAcquire(&xISRSpinlock)
#define portRELEASE_ISR_LOCK(xCoreID)   vPortRecursiveLockRelease(&xISRSpinlock)

/* ======= Interrupt Mask (per-core PMR) ======= */

extern unsigned long uxPortSetInterruptMask(void);
extern void vPortClearInterruptMask(unsigned long uxNewMaskValue);

#define portSET_INTERRUPT_MASK()                    uxPortSetInterruptMask()
#define portCLEAR_INTERRUPT_MASK(x)                 vPortClearInterruptMask(x)

/* _FROM_ISR macros are defined in portmacro.h (after kernel's portmacro.h is loaded) */

/* ======= Critical Section in TCB ======= */

#define portCRITICAL_NESTING_IN_TCB    1

/* ======= ISR Critical Sections ======= */

#define portENTER_CRITICAL_FROM_ISR()    vTaskEnterCriticalFromISR()
#define portEXIT_CRITICAL_FROM_ISR(x)    vTaskExitCriticalFromISR(x)

/* ======= Per-Core Port State (defined in port_smp.c) ======= */

extern unsigned long ulPortInterruptNestings[];
extern unsigned long ulPortYieldRequireds[];
extern unsigned long ulPortTaskHasFPUContexts[];
extern long xPortCheckAndResetYield(unsigned int core_id);

#endif /* PORT_SMP_H */
