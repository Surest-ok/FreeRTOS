#include <stdint.h>
#include "FreeRTOS.h"
#include "task.h"
#include "platform.h"
#include "uart.h"
#include "gic.h"
#include "gtimer.h"
#include "ukernel.h"
#include "arch_api/arch.h"

extern void _start(void);
extern void vPortSecondaryCoreEntry(void);
extern void *secondary_entry[];

static volatile uint64_t g_core_seen_mask;
static volatile uint64_t g_core_heartbeat[configNUMBER_OF_CORES];
static UkernelBootInfo g_boot_info;

static uint32_t current_core_id(void)
{
    uint64_t mpidr;
    __asm__ volatile("mrs %0, mpidr_el1" : "=r"(mpidr));
    return (uint32_t)((mpidr & 0xFFU) + (((mpidr >> 8) & 0xFFU) << 4));
}

void ukernel_note_core_alive(uint32_t core_id)
{
    if (core_id < (uint32_t)configNUMBER_OF_CORES) {
        __atomic_fetch_or(&g_core_seen_mask, 1ULL << core_id, __ATOMIC_RELAXED);
        __atomic_fetch_add(&g_core_heartbeat[core_id], 1ULL, __ATOMIC_RELAXED);
    }
}

uint64_t ukernel_core_seen_mask(void)
{
    return __atomic_load_n(&g_core_seen_mask, __ATOMIC_RELAXED);
}

uint64_t ukernel_core_heartbeat(uint32_t core_id)
{
    if (core_id >= (uint32_t)configNUMBER_OF_CORES) {
        return 0;
    }
    return __atomic_load_n(&g_core_heartbeat[core_id], __ATOMIC_RELAXED);
}

void ukernel_panic(const char *reason)
{
    uart_log(0, "freertos", "[panic] core=%d reason=%s\r\n", (int)current_core_id(), reason);
    for (;;) {
        __asm__ volatile("wfi");
    }
}

extern void uart_puts(const char *s);
static inline int psci_cpu_on(uint64_t mpidr, uint64_t entrypoint)
{
    register uint64_t x0 __asm__("x0") = 0xC4000003; /* PSCI CPU_ON 64-bit */
    register uint64_t x1 __asm__("x1") = mpidr;
    register uint64_t x2 __asm__("x2") = entrypoint;
    register uint64_t x3 __asm__("x3") = 0;
#if defined(CONFIG_RUN_AT_EL2) && (CONFIG_RUN_AT_EL2 == 1)
    __asm__ volatile("smc #0" : "+r"(x0) : "r"(x1), "r"(x2), "r"(x3) : "memory");
#else
    __asm__ volatile("hvc #0" : "+r"(x0) : "r"(x1), "r"(x2), "r"(x3) : "memory");
#endif
    return (int)x0;
}

void ukernel_wake_secondary_cores(void)
{
    for (int i = 1; i < configNUMBER_OF_CORES; i++) {
        __atomic_store_n(&secondary_entry[i], vPortSecondaryCoreEntry, __ATOMIC_RELEASE);
        uint64_t mpidr = arch_linear_cpu_to_mpidr((uint32_t)i);
        (void)psci_cpu_on(mpidr, (uint64_t)_start);
    }
    __asm__ volatile("dmb sy\nsev" ::: "memory");
}

extern void cherry_printk_init(void);

void ukernel_start(void)
{
    uart_init();
    cherry_printk_init();
    uart_log(6, "freertos", "freertos smp unikernel boot\r\n");

    gic_init(0);
    gtimer_init();

    g_boot_info.cores = (uint32_t)configNUMBER_OF_CORES;
    g_boot_info.timer_hz = gtimer_get_frequency();

    uart_log(6, "freertos", "[boot] cores=%d timer=%dhz\r\n",
             (int)g_boot_info.cores,
             (int)g_boot_info.timer_hz);

    if (ukernel_app_init(&g_boot_info) != 0) {
        ukernel_panic("app init failed");
    }

    vTaskStartScheduler();
    ukernel_panic("scheduler returned");
}

void vApplicationStackOverflowHook(TaskHandle_t task, char *name)
{
    (void)task;
    uart_log(0, "freertos", "[panic] stack overflow task=%s\r\n", name ? name : "?");
    ukernel_panic("stack overflow");
}

void vApplicationMallocFailedHook(void)
{
    ukernel_panic("malloc failed");
}

void vAssertCalled(const char *file, int line)
{
    uart_log(0, "freertos", "[assert] %s:%d\r\n", file ? file : "?", line);
    ukernel_panic("assert");
}
