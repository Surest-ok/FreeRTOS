/*
 * CherryEBPF - Tracepoint Registry & Kernel Activity Hooks Implementation
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
extern void *xTaskGetCurrentTaskHandle(void);
extern const char *pcTaskGetName(void *xTask);

static struct cherry_tracepoint g_tracepoints[CHERRY_TP_MAX] = {
    [CHERRY_TP_SYS_ENTER]         = { .id = CHERRY_TP_SYS_ENTER,         .name = "sys_enter",         .category = "syscall", .enabled = 1 },
    [CHERRY_TP_SYS_EXIT]          = { .id = CHERRY_TP_SYS_EXIT,          .name = "sys_exit",          .category = "syscall", .enabled = 1 },
    [CHERRY_TP_SCHED_SWITCH]      = { .id = CHERRY_TP_SCHED_SWITCH,      .name = "sched_switch",      .category = "sched",   .enabled = 1 },
    [CHERRY_TP_SCHED_WAKEUP]      = { .id = CHERRY_TP_SCHED_WAKEUP,      .name = "sched_wakeup",      .category = "sched",   .enabled = 1 },
    [CHERRY_TP_IRQ_ENTER]         = { .id = CHERRY_TP_IRQ_ENTER,         .name = "irq_enter",         .category = "irq",     .enabled = 1 },
    [CHERRY_TP_IRQ_EXIT]          = { .id = CHERRY_TP_IRQ_EXIT,          .name = "irq_exit",          .category = "irq",     .enabled = 1 },
    [CHERRY_TP_VFS_OPEN]          = { .id = CHERRY_TP_VFS_OPEN,          .name = "vfs_open",          .category = "vfs",     .enabled = 1 },
    [CHERRY_TP_VFS_CLOSE]         = { .id = CHERRY_TP_VFS_CLOSE,         .name = "vfs_close",         .category = "vfs",     .enabled = 1 },
    [CHERRY_TP_VFS_READ]          = { .id = CHERRY_TP_VFS_READ,          .name = "vfs_read",          .category = "vfs",     .enabled = 1 },
    [CHERRY_TP_VFS_WRITE]         = { .id = CHERRY_TP_VFS_WRITE,         .name = "vfs_write",         .category = "vfs",     .enabled = 1 },
    [CHERRY_TP_BLOCK_RQ_ISSUE]    = { .id = CHERRY_TP_BLOCK_RQ_ISSUE,    .name = "block_rq_issue",    .category = "block",   .enabled = 1 },
    [CHERRY_TP_BLOCK_RQ_COMPLETE] = { .id = CHERRY_TP_BLOCK_RQ_COMPLETE, .name = "block_rq_complete", .category = "block",   .enabled = 1 },
    [CHERRY_TP_NET_TX]            = { .id = CHERRY_TP_NET_TX,            .name = "net_tx_pkt",        .category = "net",     .enabled = 1 },
    [CHERRY_TP_NET_RX]            = { .id = CHERRY_TP_NET_RX,            .name = "net_rx_pkt",        .category = "net",     .enabled = 1 },
    [CHERRY_TP_KMOD_LOAD]         = { .id = CHERRY_TP_KMOD_LOAD,         .name = "kmod_load",         .category = "kmod",    .enabled = 1 },
    [CHERRY_TP_KMOD_UNLOAD]       = { .id = CHERRY_TP_KMOD_UNLOAD,       .name = "kmod_unload",       .category = "kmod",    .enabled = 1 },
    [CHERRY_TP_PROC_SPAWN]        = { .id = CHERRY_TP_PROC_SPAWN,        .name = "proc_spawn",        .category = "proc",    .enabled = 1 },
    [CHERRY_TP_PROC_EXIT]         = { .id = CHERRY_TP_PROC_EXIT,         .name = "proc_exit",         .category = "proc",    .enabled = 1 },
};

static uint64_t g_total_trace_events = 0;
static uint64_t g_total_prog_execs = 0;
static volatile uint32_t g_trace_lock = 0;
static bool g_ebpf_initialized = false;

/* Global RingBuffer for Shell live tracing */
static struct bpf_map *g_trace_ringbuf = NULL;

static inline void trace_lock(void) {
    while (__atomic_test_and_set(&g_trace_lock, __ATOMIC_ACQUIRE)) {
        __asm__ volatile("yield");
    }
}

static inline void trace_unlock(void) {
    __atomic_clear(&g_trace_lock, __ATOMIC_RELEASE);
}

static uint64_t get_timestamp_ns(void)
{
    uint64_t cnt = 0, frq = 0;
    __asm__ volatile("mrs %0, cntpct_el0" : "=r"(cnt));
    __asm__ volatile("mrs %0, cntfrq_el0" : "=r"(frq));
    if (frq == 0) frq = 62500000ULL;
    return (cnt * 1000000000ULL) / frq;
}

static uint32_t get_cpu_id(void)
{
    uint64_t mpidr;
    __asm__ volatile("mrs %0, mpidr_el1" : "=r"(mpidr));
    return (uint32_t)(mpidr & 0xff);
}

int cherry_ebpf_trace_init(void)
{
    if (g_ebpf_initialized) return 0;
    g_trace_ringbuf = cherry_ebpf_map_create("trace_rb", BPF_MAP_TYPE_RINGBUF, 0, 0, 16384);
    g_ebpf_initialized = true;
    return 0;
}

int cherry_ebpf_init(void)
{
    return cherry_ebpf_trace_init();
}

bool cherry_ebpf_is_initialized(void)
{
    return g_ebpf_initialized;
}

void cherry_ebpf_get_stats(struct cherry_ebpf_stats *stats)
{
    if (!stats) return;
    stats->total_events = g_total_trace_events;
    stats->total_prog_execs = g_total_prog_execs;
    stats->active_programs = cherry_ebpf_prog_count();
    stats->active_maps = cherry_ebpf_map_count();
}

int cherry_ebpf_prog_attach(struct cherry_ebpf_prog *prog, uint32_t tp_id)
{
    if (!prog || tp_id >= CHERRY_TP_MAX) return -1;

    trace_lock();
    struct cherry_tracepoint *tp = &g_tracepoints[tp_id];
    struct cherry_ebpf_prog_node *cur = tp->progs;
    while (cur) {
        if (cur->prog == prog) {
            trace_unlock();
            return 0; /* Already attached */
        }
        cur = cur->next;
    }

    struct cherry_ebpf_prog_node *node = (struct cherry_ebpf_prog_node *)kmalloc(sizeof(*node), GFP_ATOMIC);
    if (!node) {
        trace_unlock();
        return -2;
    }

    node->prog = prog;
    node->next = tp->progs;
    tp->progs = node;
    prog->attached_tp_mask |= (1U << tp_id);
    trace_unlock();

    return 0;
}

int cherry_ebpf_prog_detach(struct cherry_ebpf_prog *prog, uint32_t tp_id)
{
    if (!prog || tp_id >= CHERRY_TP_MAX) return -1;

    trace_lock();
    struct cherry_tracepoint *tp = &g_tracepoints[tp_id];
    struct cherry_ebpf_prog_node **prev = &tp->progs;
    while (*prev) {
        if ((*prev)->prog == prog) {
            struct cherry_ebpf_prog_node *to_free = *prev;
            *prev = (*prev)->next;
            kfree(to_free);
            prog->attached_tp_mask &= ~(1U << tp_id);
            trace_unlock();
            return 0;
        }
        prev = &(*prev)->next;
    }
    trace_unlock();
    return -2;
}

int cherry_ebpf_tp_find(const char *name)
{
    if (!name) return -1;
    for (uint32_t i = 0; i < CHERRY_TP_MAX; i++) {
        if (strcmp(g_tracepoints[i].name, name) == 0) {
            return (int)i;
        }
    }
    return -1;
}

const struct cherry_tracepoint *cherry_ebpf_get_tp(uint32_t tp_id)
{
    if (tp_id >= CHERRY_TP_MAX) return NULL;
    return &g_tracepoints[tp_id];
}

void cherry_ebpf_tracepoint_fire(uint32_t tp_id, void *ctx)
{
    if (tp_id >= CHERRY_TP_MAX) return;

    __atomic_fetch_add(&g_total_trace_events, 1, __ATOMIC_RELAXED);
    __atomic_fetch_add(&g_tracepoints[tp_id].hit_count, 1, __ATOMIC_RELAXED);

    struct cherry_tracepoint *tp = &g_tracepoints[tp_id];
    if (!tp->enabled) return;

    /* Execute all attached programs */
    struct cherry_ebpf_prog_node *cur = tp->progs;
    while (cur) {
        if (cur->prog) {
            uint64_t ret = 0;
            cherry_ebpf_vm_exec(cur->prog, ctx, &ret);
            __atomic_fetch_add(&g_total_prog_execs, 1, __ATOMIC_RELAXED);
        }
        cur = cur->next;
    }
}

/*
 * ==============================================================================
 * Subsystem Hook Implementations
 * ==============================================================================
 */

void cherry_trace_sys_enter(long id, long a0, long a1, long a2, long a3, long a4, long a5)
{
    if (!g_ebpf_initialized) return;
    if (!g_tracepoints[CHERRY_TP_SYS_ENTER].progs) {
        __atomic_fetch_add(&g_tracepoints[CHERRY_TP_SYS_ENTER].hit_count, 1, __ATOMIC_RELAXED);
        return;
    }
    struct cherry_trace_sys_enter_ctx ctx;
    ctx.timestamp = get_timestamp_ns();
    ctx.cpu = get_cpu_id();
    ctx.pid = 1;
    ctx.syscall_id = id;
    ctx.args[0] = (uint64_t)a0;
    ctx.args[1] = (uint64_t)a1;
    ctx.args[2] = (uint64_t)a2;
    ctx.args[3] = (uint64_t)a3;
    ctx.args[4] = (uint64_t)a4;
    ctx.args[5] = (uint64_t)a5;
    void *task = xTaskGetCurrentTaskHandle();
    const char *name = task ? pcTaskGetName(task) : "task";
    strncpy(ctx.comm, name ? name : "init", sizeof(ctx.comm) - 1);
    ctx.comm[sizeof(ctx.comm) - 1] = '\0';

    cherry_ebpf_tracepoint_fire(CHERRY_TP_SYS_ENTER, &ctx);
}

void cherry_trace_sys_exit(long id, long ret)
{
    if (!g_ebpf_initialized) return;
    if (!g_tracepoints[CHERRY_TP_SYS_EXIT].progs) {
        __atomic_fetch_add(&g_tracepoints[CHERRY_TP_SYS_EXIT].hit_count, 1, __ATOMIC_RELAXED);
        return;
    }
    struct cherry_trace_sys_exit_ctx ctx;
    ctx.timestamp = get_timestamp_ns();
    ctx.cpu = get_cpu_id();
    ctx.pid = 1;
    ctx.syscall_id = id;
    ctx.ret_val = ret;
    void *task = xTaskGetCurrentTaskHandle();
    const char *name = task ? pcTaskGetName(task) : "task";
    strncpy(ctx.comm, name ? name : "init", sizeof(ctx.comm) - 1);
    ctx.comm[sizeof(ctx.comm) - 1] = '\0';

    cherry_ebpf_tracepoint_fire(CHERRY_TP_SYS_EXIT, &ctx);
}

void cherry_trace_sched_switch(uint32_t prev_pid, const char *prev_comm, uint32_t next_pid, const char *next_comm, uint32_t cpu)
{
    if (!g_ebpf_initialized) return;
    if (!g_tracepoints[CHERRY_TP_SCHED_SWITCH].progs) {
        __atomic_fetch_add(&g_tracepoints[CHERRY_TP_SCHED_SWITCH].hit_count, 1, __ATOMIC_RELAXED);
        return;
    }
    struct cherry_trace_sched_switch_ctx ctx;
    ctx.timestamp = get_timestamp_ns();
    ctx.cpu = cpu;
    ctx.prev_pid = prev_pid;
    strncpy(ctx.prev_comm, prev_comm ? prev_comm : "idle", sizeof(ctx.prev_comm) - 1);
    ctx.prev_comm[sizeof(ctx.prev_comm) - 1] = '\0';
    ctx.next_pid = next_pid;
    strncpy(ctx.next_comm, next_comm ? next_comm : "task", sizeof(ctx.next_comm) - 1);
    ctx.next_comm[sizeof(ctx.next_comm) - 1] = '\0';

    cherry_ebpf_tracepoint_fire(CHERRY_TP_SCHED_SWITCH, &ctx);
}

void cherry_trace_sched_switch_hook(void)
{
    if (!g_ebpf_initialized) return;
    __atomic_fetch_add(&g_tracepoints[CHERRY_TP_SCHED_SWITCH].hit_count, 1, __ATOMIC_RELAXED);
    if (!g_tracepoints[CHERRY_TP_SCHED_SWITCH].progs) return;

    void *task = xTaskGetCurrentTaskHandle();
    const char *name = task ? pcTaskGetName(task) : "task";
    cherry_trace_sched_switch(0, "prev", 1, name, get_cpu_id());
}

void cherry_trace_sched_wakeup(uint32_t pid, const char *comm, uint32_t cpu)
{
    if (!g_ebpf_initialized) return;
    if (!g_tracepoints[CHERRY_TP_SCHED_WAKEUP].progs) {
        __atomic_fetch_add(&g_tracepoints[CHERRY_TP_SCHED_WAKEUP].hit_count, 1, __ATOMIC_RELAXED);
        return;
    }
    struct cherry_trace_sched_wakeup_ctx ctx;
    ctx.timestamp = get_timestamp_ns();
    ctx.cpu = cpu;
    ctx.pid = pid;
    strncpy(ctx.comm, comm ? comm : "task", sizeof(ctx.comm) - 1);
    ctx.comm[sizeof(ctx.comm) - 1] = '\0';

    cherry_ebpf_tracepoint_fire(CHERRY_TP_SCHED_WAKEUP, &ctx);
}

void cherry_trace_irq_enter(uint32_t irq, uint32_t cpu)
{
    if (!g_ebpf_initialized) return;
    if (!g_tracepoints[CHERRY_TP_IRQ_ENTER].progs) {
        __atomic_fetch_add(&g_tracepoints[CHERRY_TP_IRQ_ENTER].hit_count, 1, __ATOMIC_RELAXED);
        return;
    }
    struct cherry_trace_irq_enter_ctx ctx;
    ctx.timestamp = get_timestamp_ns();
    ctx.cpu = cpu;
    ctx.irq_nr = irq;

    cherry_ebpf_tracepoint_fire(CHERRY_TP_IRQ_ENTER, &ctx);
}

void cherry_trace_irq_exit(uint32_t irq, uint32_t cpu, int handled)
{
    if (!g_ebpf_initialized) return;
    if (!g_tracepoints[CHERRY_TP_IRQ_EXIT].progs) {
        __atomic_fetch_add(&g_tracepoints[CHERRY_TP_IRQ_EXIT].hit_count, 1, __ATOMIC_RELAXED);
        return;
    }
    struct cherry_trace_irq_exit_ctx ctx;
    ctx.timestamp = get_timestamp_ns();
    ctx.cpu = cpu;
    ctx.irq_nr = irq;
    ctx.handled = handled;

    cherry_ebpf_tracepoint_fire(CHERRY_TP_IRQ_EXIT, &ctx);
}

void cherry_trace_vfs_open(const char *path, int flags, int mode, int fd)
{
    if (!g_ebpf_initialized) return;
    if (!g_tracepoints[CHERRY_TP_VFS_OPEN].progs) {
        __atomic_fetch_add(&g_tracepoints[CHERRY_TP_VFS_OPEN].hit_count, 1, __ATOMIC_RELAXED);
        return;
    }
    struct cherry_trace_vfs_open_ctx ctx;
    ctx.timestamp = get_timestamp_ns();
    ctx.pid = 1;
    ctx.fd = fd;
    ctx.flags = flags;
    ctx.mode = mode;
    strncpy(ctx.path, path ? path : "", sizeof(ctx.path) - 1);
    ctx.path[sizeof(ctx.path) - 1] = '\0';

    cherry_ebpf_tracepoint_fire(CHERRY_TP_VFS_OPEN, &ctx);
}

void cherry_trace_vfs_close(int fd, int ret)
{
    if (!g_ebpf_initialized) return;
    if (!g_tracepoints[CHERRY_TP_VFS_CLOSE].progs) {
        __atomic_fetch_add(&g_tracepoints[CHERRY_TP_VFS_CLOSE].hit_count, 1, __ATOMIC_RELAXED);
        return;
    }
    struct cherry_trace_vfs_close_ctx ctx;
    ctx.timestamp = get_timestamp_ns();
    ctx.pid = 1;
    ctx.fd = fd;
    ctx.ret = ret;

    cherry_ebpf_tracepoint_fire(CHERRY_TP_VFS_CLOSE, &ctx);
}

void cherry_trace_vfs_read(int fd, size_t count, ssize_t ret)
{
    if (!g_ebpf_initialized) return;
    if (!g_tracepoints[CHERRY_TP_VFS_READ].progs) {
        __atomic_fetch_add(&g_tracepoints[CHERRY_TP_VFS_READ].hit_count, 1, __ATOMIC_RELAXED);
        return;
    }
    struct cherry_trace_vfs_rw_ctx ctx;
    ctx.timestamp = get_timestamp_ns();
    ctx.pid = 1;
    ctx.fd = fd;
    ctx.count = count;
    ctx.ret = ret;
    ctx.is_write = 0;

    cherry_ebpf_tracepoint_fire(CHERRY_TP_VFS_READ, &ctx);
}

void cherry_trace_vfs_write(int fd, size_t count, ssize_t ret)
{
    if (!g_ebpf_initialized) return;
    if (!g_tracepoints[CHERRY_TP_VFS_WRITE].progs) {
        __atomic_fetch_add(&g_tracepoints[CHERRY_TP_VFS_WRITE].hit_count, 1, __ATOMIC_RELAXED);
        return;
    }
    struct cherry_trace_vfs_rw_ctx ctx;
    ctx.timestamp = get_timestamp_ns();
    ctx.pid = 1;
    ctx.fd = fd;
    ctx.count = count;
    ctx.ret = ret;
    ctx.is_write = 1;

    cherry_ebpf_tracepoint_fire(CHERRY_TP_VFS_WRITE, &ctx);
}

void cherry_trace_block_rq_issue(const char *dev, uint64_t lba, uint32_t count, int is_write)
{
    if (!g_ebpf_initialized) return;
    if (!g_tracepoints[CHERRY_TP_BLOCK_RQ_ISSUE].progs) {
        __atomic_fetch_add(&g_tracepoints[CHERRY_TP_BLOCK_RQ_ISSUE].hit_count, 1, __ATOMIC_RELAXED);
        return;
    }
    struct cherry_trace_block_rq_ctx ctx;
    ctx.timestamp = get_timestamp_ns();
    strncpy(ctx.dev, dev ? dev : "blk", sizeof(ctx.dev) - 1);
    ctx.dev[sizeof(ctx.dev) - 1] = '\0';
    ctx.lba = lba;
    ctx.count = count;
    ctx.is_write = is_write;
    ctx.status = 0;

    cherry_ebpf_tracepoint_fire(CHERRY_TP_BLOCK_RQ_ISSUE, &ctx);
}

void cherry_trace_block_rq_complete(const char *dev, uint64_t lba, uint32_t count, int status)
{
    if (!g_ebpf_initialized) return;
    if (!g_tracepoints[CHERRY_TP_BLOCK_RQ_COMPLETE].progs) {
        __atomic_fetch_add(&g_tracepoints[CHERRY_TP_BLOCK_RQ_COMPLETE].hit_count, 1, __ATOMIC_RELAXED);
        return;
    }
    struct cherry_trace_block_rq_ctx ctx;
    ctx.timestamp = get_timestamp_ns();
    strncpy(ctx.dev, dev ? dev : "blk", sizeof(ctx.dev) - 1);
    ctx.dev[sizeof(ctx.dev) - 1] = '\0';
    ctx.lba = lba;
    ctx.count = count;
    ctx.is_write = 0;
    ctx.status = status;

    cherry_ebpf_tracepoint_fire(CHERRY_TP_BLOCK_RQ_COMPLETE, &ctx);
}

void cherry_trace_net_tx(const void *buf, size_t len, uint16_t proto, uint32_t cpu)
{
    if (!g_ebpf_initialized) return;
    if (!g_tracepoints[CHERRY_TP_NET_TX].progs) {
        __atomic_fetch_add(&g_tracepoints[CHERRY_TP_NET_TX].hit_count, 1, __ATOMIC_RELAXED);
        return;
    }
    (void)buf;
    struct cherry_trace_net_pkt_ctx ctx;
    ctx.timestamp = get_timestamp_ns();
    ctx.cpu = cpu;
    ctx.len = (uint32_t)len;
    ctx.proto = proto;
    ctx.is_tx = 1;

    cherry_ebpf_tracepoint_fire(CHERRY_TP_NET_TX, &ctx);
}

void cherry_trace_net_rx(const void *buf, size_t len, uint16_t proto, uint32_t cpu)
{
    if (!g_ebpf_initialized) return;
    if (!g_tracepoints[CHERRY_TP_NET_RX].progs) {
        __atomic_fetch_add(&g_tracepoints[CHERRY_TP_NET_RX].hit_count, 1, __ATOMIC_RELAXED);
        return;
    }
    (void)buf;
    struct cherry_trace_net_pkt_ctx ctx;
    ctx.timestamp = get_timestamp_ns();
    ctx.cpu = cpu;
    ctx.len = (uint32_t)len;
    ctx.proto = proto;
    ctx.is_tx = 0;

    cherry_ebpf_tracepoint_fire(CHERRY_TP_NET_RX, &ctx);
}

void cherry_trace_kmod_load(const char *name, void *base, size_t size)
{
    if (!g_ebpf_initialized) return;
    if (!g_tracepoints[CHERRY_TP_KMOD_LOAD].progs) {
        __atomic_fetch_add(&g_tracepoints[CHERRY_TP_KMOD_LOAD].hit_count, 1, __ATOMIC_RELAXED);
        return;
    }
    struct cherry_trace_kmod_ctx ctx;
    ctx.timestamp = get_timestamp_ns();
    strncpy(ctx.name, name ? name : "kmod", sizeof(ctx.name) - 1);
    ctx.name[sizeof(ctx.name) - 1] = '\0';
    ctx.base = (uint64_t)(uintptr_t)base;
    ctx.size = (uint64_t)size;
    ctx.is_load = 1;

    cherry_ebpf_tracepoint_fire(CHERRY_TP_KMOD_LOAD, &ctx);
}

void cherry_trace_kmod_unload(const char *name)
{
    if (!g_ebpf_initialized) return;
    if (!g_tracepoints[CHERRY_TP_KMOD_UNLOAD].progs) {
        __atomic_fetch_add(&g_tracepoints[CHERRY_TP_KMOD_UNLOAD].hit_count, 1, __ATOMIC_RELAXED);
        return;
    }
    struct cherry_trace_kmod_ctx ctx;
    ctx.timestamp = get_timestamp_ns();
    strncpy(ctx.name, name ? name : "kmod", sizeof(ctx.name) - 1);
    ctx.name[sizeof(ctx.name) - 1] = '\0';
    ctx.base = 0;
    ctx.size = 0;
    ctx.is_load = 0;

    cherry_ebpf_tracepoint_fire(CHERRY_TP_KMOD_UNLOAD, &ctx);
}

void cherry_trace_proc_spawn(uint32_t pid, uint32_t ppid, const char *name)
{
    if (!g_ebpf_initialized) return;
    if (!g_tracepoints[CHERRY_TP_PROC_SPAWN].progs) {
        __atomic_fetch_add(&g_tracepoints[CHERRY_TP_PROC_SPAWN].hit_count, 1, __ATOMIC_RELAXED);
        return;
    }
    struct cherry_trace_proc_ctx ctx;
    ctx.timestamp = get_timestamp_ns();
    ctx.pid = pid;
    ctx.ppid_or_code = ppid;
    strncpy(ctx.name, name ? name : "proc", sizeof(ctx.name) - 1);
    ctx.name[sizeof(ctx.name) - 1] = '\0';
    ctx.is_spawn = 1;

    cherry_ebpf_tracepoint_fire(CHERRY_TP_PROC_SPAWN, &ctx);
}

void cherry_trace_proc_exit(uint32_t pid, int exit_code)
{
    if (!g_ebpf_initialized) return;
    if (!g_tracepoints[CHERRY_TP_PROC_EXIT].progs) {
        __atomic_fetch_add(&g_tracepoints[CHERRY_TP_PROC_EXIT].hit_count, 1, __ATOMIC_RELAXED);
        return;
    }
    struct cherry_trace_proc_ctx ctx;
    ctx.timestamp = get_timestamp_ns();
    ctx.pid = pid;
    ctx.ppid_or_code = (uint32_t)exit_code;
    ctx.name[0] = '\0';
    ctx.is_spawn = 0;

    cherry_ebpf_tracepoint_fire(CHERRY_TP_PROC_EXIT, &ctx);
}
