/*
 * CherryEBPF - Full-Featured In-Kernel eBPF Subsystem
 *
 * Copyright (c) 2026 ForgeStitch
 * SPDX-License-Identifier: MIT
 *
 * Combines 64-bit eBPF Virtual Machine, Verifier, Maps (Hash, Array, RingBuf),
 * Linux Standard Helpers, Kernel Tracepoints, and CLI Tooling.
 */

#ifndef _CHERRY_EBPF_H_
#define _CHERRY_EBPF_H_

#include "ebpf_insn.h"
#include "ebpf_map.h"
#include "ebpf_trace.h"
#include "ebpf_vm.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Linux Standard Helper Function IDs */
#define BPF_FUNC_map_lookup_elem         1
#define BPF_FUNC_map_update_elem         2
#define BPF_FUNC_map_delete_elem         3
#define BPF_FUNC_probe_read              4
#define BPF_FUNC_ktime_get_ns            5
#define BPF_FUNC_trace_printk            6
#define BPF_FUNC_get_smp_processor_id    7
#define BPF_FUNC_get_current_pid_tgid    8
#define BPF_FUNC_get_current_comm        9
#define BPF_FUNC_ringbuf_output          10
#define BPF_FUNC_ringbuf_reserve         11
#define BPF_FUNC_ringbuf_submit          12

/* Global Subsystem Statistics */
struct cherry_ebpf_stats {
    uint64_t total_events;
    uint64_t total_prog_execs;
    uint64_t total_helper_calls;
    uint32_t active_programs;
    uint32_t active_maps;
};

/* Core Subsystem APIs */
int  cherry_ebpf_init(void);
int  cherry_ebpf_run_tests(void);
bool cherry_ebpf_is_initialized(void);
void cherry_ebpf_get_stats(struct cherry_ebpf_stats *stats);

#ifdef __cplusplus
}
#endif

#endif /* _CHERRY_EBPF_H_ */
