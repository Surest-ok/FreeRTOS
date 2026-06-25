#include <stdint.h>
#include "FreeRTOS.h"
#include "task.h"
#include "port_smp.h"
#include "gic.h"
#include "app_irq.h"

uint64_t ulPortInterruptNestings[configNUMBER_OF_CORES] = {0};
uint64_t ulPortYieldRequireds[configNUMBER_OF_CORES] = {0};
uint64_t ulPortTaskHasFPUContexts[configNUMBER_OF_CORES] = {0};
Spinlock_t xTaskSpinlock = {0, 0};
Spinlock_t xISRSpinlock  = {0, 0};
static volatile uint64_t ullPortActiveCoreMask;
static volatile uint64_t ullPortIdleCoreMask;
static volatile uint64_t ullPortYieldIPICounts[configNUMBER_OF_CORES];
static volatile uint64_t ullPortYieldIPIReceives[configNUMBER_OF_CORES];
static volatile uint64_t ullPortSwitchCounts[configNUMBER_OF_CORES];
#if ( configPORT_SMP_DIAGNOSTICS == 1 )
volatile uint64_t ullPortSvcEntries[configNUMBER_OF_CORES];
volatile uint64_t ullPortSvcExits[configNUMBER_OF_CORES];
volatile uint64_t ullPortIrqSwitches[configNUMBER_OF_CORES];
volatile uint64_t ullPortSvcStages[configNUMBER_OF_CORES];
volatile uint64_t ullPortSvcEsr[configNUMBER_OF_CORES];
volatile uint64_t ullPortSyncAbortEsr[configNUMBER_OF_CORES];
volatile uint64_t ullPortSyncAbortElr[configNUMBER_OF_CORES];
volatile uint64_t ullPortSyncAbortFar[configNUMBER_OF_CORES];
#endif

void vPortRawSpinLockAcquire(RawSpinlock_t *pxLock)
{
    arch_raw_spin_lock(pxLock);
}

void vPortRawSpinLockRelease(RawSpinlock_t *pxLock)
{
    arch_raw_spin_unlock(pxLock);
}

void vPortRecursiveLockAcquire(Spinlock_t *pxLock)
{
    arch_recursive_spin_lock(pxLock);
}

void vPortRecursiveLockRelease(Spinlock_t *pxLock)
{
    arch_recursive_spin_unlock(pxLock);
}

void vPortSetCoreActive(long xCoreID)
{
    if ((xCoreID >= 0) && (xCoreID < configNUMBER_OF_CORES)) {
        __atomic_fetch_or(&ullPortActiveCoreMask, 1ULL << (uint64_t)xCoreID, __ATOMIC_RELEASE);
    }
}

unsigned long vPortGetActiveCoreMask(void)
{
    return (unsigned long)__atomic_load_n(&ullPortActiveCoreMask, __ATOMIC_ACQUIRE);
}

void vPortSetCoreIdle(long xCoreID, int idle)
{
    if ((xCoreID >= 0) && (xCoreID < configNUMBER_OF_CORES)) {
        uint64_t bit = 1ULL << (uint64_t)xCoreID;
        __atomic_fetch_add(&ullPortSwitchCounts[xCoreID], 1ULL, __ATOMIC_RELAXED);
        if (idle != 0) {
            __atomic_fetch_or(&ullPortIdleCoreMask, bit, __ATOMIC_RELEASE);
        } else {
            __atomic_fetch_and(&ullPortIdleCoreMask, ~bit, __ATOMIC_RELEASE);
        }
    }
}

unsigned long vPortGetIdleCoreMask(void)
{
    return (unsigned long)__atomic_load_n(&ullPortIdleCoreMask, __ATOMIC_ACQUIRE);
}

unsigned long vPortGetYieldIPICount(long xCoreID)
{
    if ((xCoreID < 0) || (xCoreID >= configNUMBER_OF_CORES)) {
        return 0;
    }
    return (unsigned long)__atomic_load_n(&ullPortYieldIPIReceives[xCoreID], __ATOMIC_RELAXED);
}

unsigned long vPortGetSwitchCount(long xCoreID)
{
    if ((xCoreID < 0) || (xCoreID >= configNUMBER_OF_CORES)) {
        return 0;
    }
    return (unsigned long)__atomic_load_n(&ullPortSwitchCounts[xCoreID], __ATOMIC_RELAXED);
}

int vPortYieldCore(BaseType_t xCoreID) {
    BaseType_t local = portGET_CORE_ID();

    if ((xCoreID < 0) || (xCoreID >= configNUMBER_OF_CORES) || (xCoreID == local)) {
        return 0;
    }

    if ((__atomic_load_n(&ullPortActiveCoreMask, __ATOMIC_ACQUIRE) & (1ULL << (uint64_t)xCoreID)) == 0U) {
        return 0;
    }

    uint64_t aff1 = ((uint64_t)xCoreID >> 4) & 0xFFULL;
    uint64_t target_list = 1ULL << ((uint64_t)xCoreID & 0xFULL);
    uint64_t v = (aff1 << 16) | target_list; /* SGI 0, Aff2/Aff3 = 0 */
    __asm__ volatile("msr S3_0_C12_C11_5, %0\nisb\n" :: "r"(v) : "memory");
    __atomic_fetch_add(&ullPortYieldIPICounts[xCoreID], 1ULL, __ATOMIC_RELAXED);
    return 1;
}

void vPortBroadcastTick(void)
{
    BaseType_t local = portGET_CORE_ID();
    uint64_t active = __atomic_load_n(&ullPortActiveCoreMask, __ATOMIC_ACQUIRE);

    for (BaseType_t core = 0; core < (BaseType_t)configNUMBER_OF_CORES; core++) {
        if ((core != local) && ((active & (1ULL << (uint64_t)core)) != 0ULL)) {
            (void)vPortYieldCore(core);
        }
    }
}

void SGI_Yield_Handler(void)
{
    BaseType_t c = portGET_CORE_ID();
    *(volatile uint32_t *)((uint64_t)0x080A0000 + (uint64_t)c * 0x20000 + 0x1100) = 1U;
    __atomic_fetch_add(&ullPortYieldIPIReceives[c], 1ULL, __ATOMIC_RELAXED);
    ulPortYieldRequireds[c] = 1;
}
extern void vPortRestoreTaskContext(void);
void vPortSecondaryCoreEntry(void) {
    BaseType_t core_id = portGET_CORE_ID();
    extern void * volatile pxCurrentTCBs[];
    /* WFE spin — gentle on TCG */
    while (pxCurrentTCBs[core_id] == NULL) {
        __asm__ volatile("dmb sy\nwfe" ::: "memory");
    }
    gic_init((uint32_t)core_id); gic_set_priority(0,0xB0); gic_enable_irq(0); app_irq_install(0,SGI_Yield_Handler);
    vPortSetCoreActive(core_id);
    __asm__ volatile("msr daifclr, #2\nisb\n");
    vPortRestoreTaskContext(); for(;;) __asm__ volatile("wfi");
}
