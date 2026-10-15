/*
 * CherryEBPF - Comprehensive Kernel Activity Tracing Subsystem
 *
 * Copyright (c) 2026 ForgeStitch
 * SPDX-License-Identifier: MIT
 *
 * Full tracepoint coverage across ALL kernel activities:
 * 1. System Calls: sys_enter, sys_exit
 * 2. Scheduler: sched_switch, sched_wakeup
 * 3. Interrupts: irq_enter, irq_exit
 * 4. Virtual File System: vfs_open, vfs_close, vfs_read, vfs_write
 * 5. Block Storage I/O: block_rq_issue, block_rq_complete
 * 6. Networking: net_tx_pkt, net_rx_pkt
 * 7. Kernel Modules: kmod_load, kmod_unload
 * 8. Process Lifecycle: proc_spawn, proc_exit
 */

#ifndef _CHERRY_EBPF_TRACE_H_
#define _CHERRY_EBPF_TRACE_H_

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <sys/types.h>
typedef long ssize_t;

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Enumeration of ALL Kernel Tracepoints (18 Tracepoints)
 */
enum cherry_tracepoint_id {
    CHERRY_TP_SYS_ENTER = 0,     /* Syscall entry: args, id, pid */
    CHERRY_TP_SYS_EXIT,          /* Syscall return: ret, id, pid */
    CHERRY_TP_SCHED_SWITCH,      /* Scheduler context switch: prev/next task */
    CHERRY_TP_SCHED_WAKEUP,      /* Scheduler task unblocked / wakeup */
    CHERRY_TP_IRQ_ENTER,         /* Interrupt entry: vector, cpu */
    CHERRY_TP_IRQ_EXIT,          /* Interrupt exit: vector, cpu, handled */
    CHERRY_TP_VFS_OPEN,          /* VFS open: path, flags, mode, fd */
    CHERRY_TP_VFS_CLOSE,         /* VFS close: fd, ret */
    CHERRY_TP_VFS_READ,          /* VFS read: fd, count, ret */
    CHERRY_TP_VFS_WRITE,         /* VFS write: fd, count, ret */
    CHERRY_TP_BLOCK_RQ_ISSUE,    /* Block device I/O issue: dev, lba, count, rw */
    CHERRY_TP_BLOCK_RQ_COMPLETE, /* Block device I/O complete: dev, status */
    CHERRY_TP_NET_TX,            /* Network packet transmit: len, proto, cpu */
    CHERRY_TP_NET_RX,            /* Network packet receive: len, proto, cpu */
    CHERRY_TP_KMOD_LOAD,         /* Kernel module loaded: name, addr, size */
    CHERRY_TP_KMOD_UNLOAD,       /* Kernel module unloaded: name */
    CHERRY_TP_PROC_SPAWN,        /* Process created: pid, ppid, name */
    CHERRY_TP_PROC_EXIT,         /* Process exited: pid, exit_code */
    CHERRY_TP_MAX
};

/*
 * Tracepoint Event Context Structures Passed to eBPF Programs (in R1)
 */
struct cherry_trace_sys_enter_ctx {
    uint64_t timestamp;
    uint32_t pid;
    uint32_t cpu;
    int64_t  syscall_id;
    uint64_t args[6];
    char     comm[16];
};

struct cherry_trace_sys_exit_ctx {
    uint64_t timestamp;
    uint32_t pid;
    uint32_t cpu;
    int64_t  syscall_id;
    int64_t  ret_val;
    char     comm[16];
};

struct cherry_trace_sched_switch_ctx {
    uint64_t timestamp;
    uint32_t cpu;
    uint32_t prev_pid;
    char     prev_comm[16];
    uint32_t next_pid;
    char     next_comm[16];
};

struct cherry_trace_sched_wakeup_ctx {
    uint64_t timestamp;
    uint32_t cpu;
    uint32_t pid;
    char     comm[16];
};

struct cherry_trace_irq_enter_ctx {
    uint64_t timestamp;
    uint32_t cpu;
    uint32_t irq_nr;
};

struct cherry_trace_irq_exit_ctx {
    uint64_t timestamp;
    uint32_t cpu;
    uint32_t irq_nr;
    int      handled;
};

struct cherry_trace_vfs_open_ctx {
    uint64_t timestamp;
    uint32_t pid;
    int      fd;
    int      flags;
    int      mode;
    char     path[64];
};

struct cherry_trace_vfs_close_ctx {
    uint64_t timestamp;
    uint32_t pid;
    int      fd;
    int      ret;
};

struct cherry_trace_vfs_rw_ctx {
    uint64_t timestamp;
    uint32_t pid;
    int      fd;
    uint64_t count;
    int64_t  ret;
    int      is_write;
};

struct cherry_trace_block_rq_ctx {
    uint64_t timestamp;
    char     dev[16];
    uint64_t lba;
    uint32_t count;
    int      is_write;
    int      status;
};

struct cherry_trace_net_pkt_ctx {
    uint64_t timestamp;
    uint32_t cpu;
    uint32_t len;
    uint16_t proto;
    int      is_tx;
};

struct cherry_trace_kmod_ctx {
    uint64_t timestamp;
    char     name[32];
    uint64_t base;
    uint64_t size;
    int      is_load;
};

struct cherry_trace_proc_ctx {
    uint64_t timestamp;
    uint32_t pid;
    uint32_t ppid_or_code;
    char     name[32];
    int      is_spawn;
};

/* Forward declaration */
struct cherry_ebpf_prog;

/* Attached Program Node */
struct cherry_ebpf_prog_node {
    struct cherry_ebpf_prog      *prog;
    struct cherry_ebpf_prog_node *next;
};

/* Tracepoint Metadata & Registry */
struct cherry_tracepoint {
    uint32_t id;
    const char *name;
    const char *category;
    uint32_t enabled;
    uint64_t hit_count;
    struct cherry_ebpf_prog_node *progs;
};

/* Tracepoint Management APIs */
int  cherry_ebpf_trace_init(void);
int  cherry_ebpf_prog_attach(struct cherry_ebpf_prog *prog, uint32_t tp_id);
int  cherry_ebpf_prog_detach(struct cherry_ebpf_prog *prog, uint32_t tp_id);
int  cherry_ebpf_tp_find(const char *name);
const struct cherry_tracepoint *cherry_ebpf_get_tp(uint32_t tp_id);
void cherry_ebpf_tracepoint_fire(uint32_t tp_id, void *ctx);

/*
 * Kernel Trace Hooks - Invoked Across Subsystems
 */
void cherry_trace_sys_enter(long id, long a0, long a1, long a2, long a3, long a4, long a5);
void cherry_trace_sys_exit(long id, long ret);
void cherry_trace_sched_switch(uint32_t prev_pid, const char *prev_comm, uint32_t next_pid, const char *next_comm, uint32_t cpu);
void cherry_trace_sched_wakeup(uint32_t pid, const char *comm, uint32_t cpu);
void cherry_trace_irq_enter(uint32_t irq, uint32_t cpu);
void cherry_trace_irq_exit(uint32_t irq, uint32_t cpu, int handled);
void cherry_trace_vfs_open(const char *path, int flags, int mode, int fd);
void cherry_trace_vfs_close(int fd, int ret);
void cherry_trace_vfs_read(int fd, size_t count, ssize_t ret);
void cherry_trace_vfs_write(int fd, size_t count, ssize_t ret);
void cherry_trace_block_rq_issue(const char *dev, uint64_t lba, uint32_t count, int is_write);
void cherry_trace_block_rq_complete(const char *dev, uint64_t lba, uint32_t count, int status);
void cherry_trace_net_tx(const void *buf, size_t len, uint16_t proto, uint32_t cpu);
void cherry_trace_net_rx(const void *buf, size_t len, uint16_t proto, uint32_t cpu);
void cherry_trace_kmod_load(const char *name, void *base, size_t size);
void cherry_trace_kmod_unload(const char *name);
void cherry_trace_proc_spawn(uint32_t pid, uint32_t ppid, const char *name);
void cherry_trace_proc_exit(uint32_t pid, int exit_code);

/* FreeRTOS Hook */
void cherry_trace_sched_switch_hook(void);

#ifdef __cplusplus
}
#endif

#endif /* _CHERRY_EBPF_TRACE_H_ */
