/*
 * CherryEBPF - Virtual Machine Interpreter & Verifier
 *
 * Copyright (c) 2026 ForgeStitch
 * SPDX-License-Identifier: MIT
 */

#ifndef _CHERRY_EBPF_VM_H_
#define _CHERRY_EBPF_VM_H_

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "ebpf_insn.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Program Types */
#define BPF_PROG_TYPE_UNSPEC         0
#define BPF_PROG_TYPE_SOCKET_FILTER  1
#define BPF_PROG_TYPE_KPROBE         2
#define BPF_PROG_TYPE_TRACEPOINT     5
#define BPF_PROG_TYPE_PERF_EVENT     7

/* VM Limits */
#define CHERRY_EBPF_STACK_SIZE       512
#define CHERRY_EBPF_MAX_INSNS        4096
#define CHERRY_EBPF_MAX_STEPS        100000

/* Loaded eBPF Program Structure */
struct cherry_ebpf_prog {
    uint32_t id;
    char     name[32];
    uint32_t type;
    struct bpf_insn *insns;
    uint32_t insn_cnt;
    uint32_t attached_tp_mask;
    uint64_t run_count;
    uint64_t total_ns;

    struct cherry_ebpf_prog *next;
};

/* Verifier */
int cherry_ebpf_verify(const struct bpf_insn *insns, uint32_t insn_cnt, uint32_t prog_type);

/* Program Lifecycle */
struct cherry_ebpf_prog *cherry_ebpf_prog_load(const char *name, uint32_t type,
                                               const struct bpf_insn *insns,
                                               uint32_t insn_cnt);
void cherry_ebpf_prog_destroy(struct cherry_ebpf_prog *prog);
struct cherry_ebpf_prog *cherry_ebpf_prog_find_by_id(uint32_t id);
struct cherry_ebpf_prog *cherry_ebpf_prog_find_by_name(const char *name);
int  cherry_ebpf_prog_for_each(int (*cb)(struct cherry_ebpf_prog *prog, void *arg), void *arg);
uint32_t cherry_ebpf_prog_count(void);

/* VM Execution */
int cherry_ebpf_vm_exec(struct cherry_ebpf_prog *prog, void *ctx, uint64_t *ret_val);

/* Helper Call Dispatcher */
uint64_t cherry_ebpf_call_helper(uint32_t func_id, uint64_t r1, uint64_t r2, uint64_t r3, uint64_t r4, uint64_t r5);

#ifdef __cplusplus
}
#endif

#endif /* _CHERRY_EBPF_VM_H_ */
