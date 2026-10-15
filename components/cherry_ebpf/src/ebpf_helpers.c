/*
 * CherryEBPF - Linux Standard eBPF Helper Functions Implementation
 *
 * Copyright (c) 2026 ForgeStitch
 * SPDX-License-Identifier: MIT
 */

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include "cherry_ebpf.h"
#include "cherry_kal.h"

extern int printk(const char *fmt, ...);
extern int sys_getpid(void) __attribute__((weak));
extern void *xTaskGetCurrentTaskHandle(void);
extern const char *pcTaskGetName(void *xTask);
extern uint32_t xPortGetCoreID(void);

static uint64_t g_total_helper_calls = 0;

/*
 * Helper 5: bpf_ktime_get_ns
 * High-resolution monotonic timestamp from ARM Generic Timer
 */
static uint64_t helper_ktime_get_ns(void)
{
    uint64_t cnt = 0;
    uint64_t frq = 0;
    __asm__ volatile("mrs %0, cntpct_el0" : "=r"(cnt));
    __asm__ volatile("mrs %0, cntfrq_el0" : "=r"(frq));
    if (frq == 0) frq = 62500000ULL;
    return (cnt * 1000000000ULL) / frq;
}

/*
 * Helper 6: bpf_trace_printk
 */
static int helper_trace_printk(const char *fmt, uint32_t fmt_size, uint64_t a1, uint64_t a2, uint64_t a3)
{
    (void)fmt_size;
    if (!fmt) return -1;
    printk("[ebpf-trace] ");
    printk(fmt, a1, a2, a3);
    printk("\n");
    return 0;
}

/*
 * Helper 7: bpf_get_smp_processor_id
 */
static uint32_t helper_get_smp_processor_id(void)
{
    uint64_t mpidr;
    __asm__ volatile("mrs %0, mpidr_el1" : "=r"(mpidr));
    return (uint32_t)(mpidr & 0xff);
}

/*
 * Helper 8: bpf_get_current_pid_tgid
 */
static uint64_t helper_get_current_pid_tgid(void)
{
    uint32_t pid = 1;
#if defined(sys_getpid)
    pid = (uint32_t)sys_getpid();
#endif
    return ((uint64_t)pid << 32) | pid;
}

/*
 * Helper 9: bpf_get_current_comm
 */
static int helper_get_current_comm(void *buf, uint32_t size_of_buf)
{
    if (!buf || size_of_buf == 0) return -1;
    void *task = xTaskGetCurrentTaskHandle();
    const char *name = task ? pcTaskGetName(task) : "kernel";
    strncpy((char *)buf, name ? name : "unknown", size_of_buf - 1);
    ((char *)buf)[size_of_buf - 1] = '\0';
    return 0;
}

/*
 * Helper 4: bpf_probe_read
 */
static int helper_probe_read(void *dst, uint32_t size, const void *unsafe_ptr)
{
    if (!dst || !unsafe_ptr || size == 0) return -1;
    memcpy(dst, unsafe_ptr, size);
    return 0;
}

/*
 * Main Helper Dispatcher
 */
uint64_t cherry_ebpf_call_helper(uint32_t func_id, uint64_t r1, uint64_t r2, uint64_t r3, uint64_t r4, uint64_t r5)
{
    __atomic_fetch_add(&g_total_helper_calls, 1, __ATOMIC_RELAXED);

    switch (func_id) {
    case BPF_FUNC_map_lookup_elem:
        return (uint64_t)(uintptr_t)cherry_ebpf_map_lookup((struct bpf_map *)(uintptr_t)r1, (const void *)(uintptr_t)r2);

    case BPF_FUNC_map_update_elem:
        return (uint64_t)(int64_t)cherry_ebpf_map_update((struct bpf_map *)(uintptr_t)r1,
                                                        (const void *)(uintptr_t)r2,
                                                        (const void *)(uintptr_t)r3,
                                                        r4);

    case BPF_FUNC_map_delete_elem:
        return (uint64_t)(int64_t)cherry_ebpf_map_delete((struct bpf_map *)(uintptr_t)r1, (const void *)(uintptr_t)r2);

    case BPF_FUNC_probe_read:
        return (uint64_t)(int64_t)helper_probe_read((void *)(uintptr_t)r1, (uint32_t)r2, (const void *)(uintptr_t)r3);

    case BPF_FUNC_ktime_get_ns:
        return helper_ktime_get_ns();

    case BPF_FUNC_trace_printk:
        return (uint64_t)(int64_t)helper_trace_printk((const char *)(uintptr_t)r1, (uint32_t)r2, r3, r4, r5);

    case BPF_FUNC_get_smp_processor_id:
        return (uint64_t)helper_get_smp_processor_id();

    case BPF_FUNC_get_current_pid_tgid:
        return helper_get_current_pid_tgid();

    case BPF_FUNC_get_current_comm:
        return (uint64_t)(int64_t)helper_get_current_comm((void *)(uintptr_t)r1, (uint32_t)r2);

    case BPF_FUNC_ringbuf_output:
        return (uint64_t)cherry_ebpf_ringbuf_output((struct bpf_map *)(uintptr_t)r1, (const void *)(uintptr_t)r2, r3, r4);

    default:
        return (uint64_t)-1;
    }
}
