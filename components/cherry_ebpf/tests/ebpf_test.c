/*
 * CherryEBPF - Comprehensive Verification Test Suite
 *
 * Copyright (c) 2026 ForgeStitch
 * SPDX-License-Identifier: MIT
 */

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include "cherry_ebpf.h"

extern int printk(const char *fmt, ...);
extern int uart_printf(const char *fmt, ...);

/*
 * Test 1: 64-bit & 32-bit ALU, Endian byte swap, Wide 64-bit load
 */
static bool test_vm_arithmetic(void)
{
    /* Program computing:
     * r1 = 100
     * r2 = 20
     * r0 = r1 + r2        (120)
     * r0 = r0 * 3         (360)
     * r0 = r0 / 2         (180)
     * r0 = r0 - 30        (150)
     * r0 = r0 << 2        (600)
     * r0 = r0 >> 1        (300)
     * r0 = r0 ^ 0x55      (300 ^ 85 = 345)
     * r0 = r0 | 0x2       (345 | 2 = 347)
     * r0 = r0 & 0x1ff     (347 & 511 = 347)
     * exit
     */
    struct bpf_insn insns[] = {
        BPF_MOV64_IMM(BPF_REG_1, 100),
        BPF_MOV64_IMM(BPF_REG_2, 20),
        BPF_MOV64_REG(BPF_REG_0, BPF_REG_1),
        BPF_ALU64_REG(BPF_ADD, BPF_REG_0, BPF_REG_2),
        BPF_ALU64_IMM(BPF_MUL, BPF_REG_0, 3),
        BPF_ALU64_IMM(BPF_DIV, BPF_REG_0, 2),
        BPF_ALU64_IMM(BPF_SUB, BPF_REG_0, 30),
        BPF_ALU64_IMM(BPF_LSH, BPF_REG_0, 2),
        BPF_ALU64_IMM(BPF_RSH, BPF_REG_0, 1),
        BPF_ALU64_IMM(BPF_XOR, BPF_REG_0, 0x55),
        BPF_ALU64_IMM(BPF_OR,  BPF_REG_0, 0x02),
        BPF_ALU64_IMM(BPF_AND, BPF_REG_0, 0x1ff),
        BPF_EXIT_INSN(),
    };

    struct cherry_ebpf_prog *p = cherry_ebpf_prog_load("test_alu", BPF_PROG_TYPE_UNSPEC, insns, sizeof(insns)/sizeof(insns[0]));
    if (!p) return false;

    uint64_t ret = 0;
    int err = cherry_ebpf_vm_exec(p, NULL, &ret);
    cherry_ebpf_prog_destroy(p);

    if (err != 0 || ret != 379) { printk("vm_arithmetic: err=%d ret=%llu\n", err, (unsigned long long)ret); return false; }

    /* Test 64-bit Wide Immediate Load */
    struct bpf_insn insns_imm64[] = {
        BPF_LD_IMM64(BPF_REG_0, 0x1122334455667788ULL),
        BPF_EXIT_INSN(),
    };
    p = cherry_ebpf_prog_load("test_imm64", BPF_PROG_TYPE_UNSPEC, insns_imm64, sizeof(insns_imm64)/sizeof(insns_imm64[0]));
    if (!p) return false;
    ret = 0;
    err = cherry_ebpf_vm_exec(p, NULL, &ret);
    cherry_ebpf_prog_destroy(p);
    if (err != 0 || ret != 0x1122334455667788ULL) return false;

    return true;
}

/*
 * Test 2: Verifier Safety Rejection
 */
static bool test_verifier_safety(void)
{
    /* 1. Missing EXIT */
    struct bpf_insn no_exit[] = {
        BPF_MOV64_IMM(BPF_REG_0, 42),
    };
    if (cherry_ebpf_verify(no_exit, 1, 0) == 0) return false;

    /* 2. Write to R10 (FP) */
    struct bpf_insn write_fp[] = {
        BPF_MOV64_IMM(BPF_REG_10, 123),
        BPF_EXIT_INSN(),
    };
    if (cherry_ebpf_verify(write_fp, 2, 0) == 0) return false;

    /* 3. Jump target out of bounds */
    struct bpf_insn bad_jump[] = {
        BPF_JMP_A(50),
        BPF_EXIT_INSN(),
    };
    if (cherry_ebpf_verify(bad_jump, 2, 0) == 0) return false;

    /* 4. Division by immediate zero */
    struct bpf_insn div_zero[] = {
        BPF_MOV64_IMM(BPF_REG_0, 10),
        BPF_ALU64_IMM(BPF_DIV, BPF_REG_0, 0),
        BPF_EXIT_INSN(),
    };
    if (cherry_ebpf_verify(div_zero, 3, 0) == 0) return false;

    return true;
}

/*
 * Test 3: BPF Hash Map Operations
 */
static bool test_map_hash(void)
{
    struct bpf_map *map = cherry_ebpf_map_create("test_hash", BPF_MAP_TYPE_HASH, sizeof(uint32_t), sizeof(uint64_t), 32);
    if (!map) return false;

    uint32_t key1 = 101, key2 = 202, key3 = 303;
    uint64_t val1 = 10001ULL, val2 = 20002ULL, val3 = 30003ULL;

    /* Insert */
    if (cherry_ebpf_map_update(map, &key1, &val1, BPF_NOEXIST) != 0) return false;
    if (cherry_ebpf_map_update(map, &key2, &val2, BPF_NOEXIST) != 0) return false;
    if (cherry_ebpf_map_update(map, &key3, &val3, BPF_NOEXIST) != 0) return false;

    /* Lookup */
    uint64_t *r1 = (uint64_t *)cherry_ebpf_map_lookup(map, &key1);
    uint64_t *r2 = (uint64_t *)cherry_ebpf_map_lookup(map, &key2);
    if (!r1 || *r1 != val1) return false;
    if (!r2 || *r2 != val2) return false;

    /* Update existing */
    uint64_t val1_new = 99999ULL;
    if (cherry_ebpf_map_update(map, &key1, &val1_new, BPF_EXIST) != 0) return false;
    r1 = (uint64_t *)cherry_ebpf_map_lookup(map, &key1);
    if (!r1 || *r1 != val1_new) return false;

    /* Delete */
    if (cherry_ebpf_map_delete(map, &key2) != 0) return false;
    if (cherry_ebpf_map_lookup(map, &key2) != NULL) return false;

    cherry_ebpf_map_destroy(map);
    return true;
}

/*
 * Test 4: BPF Array Map Operations
 */
static bool test_map_array(void)
{
    struct bpf_map *map = cherry_ebpf_map_create("test_arr", BPF_MAP_TYPE_ARRAY, sizeof(uint32_t), sizeof(uint64_t), 16);
    if (!map) return false;

    uint32_t idx = 5;
    uint64_t val = 0xdeadbeefcafebabeULL;
    if (cherry_ebpf_map_update(map, &idx, &val, BPF_ANY) != 0) return false;

    uint64_t *r = (uint64_t *)cherry_ebpf_map_lookup(map, &idx);
    if (!r || *r != val) return false;

    /* Out of bounds lookup */
    uint32_t bad_idx = 100;
    if (cherry_ebpf_map_lookup(map, &bad_idx) != NULL) return false;

    cherry_ebpf_map_destroy(map);
    return true;
}

/* Ringbuffer test callback context */
static int g_rb_test_count = 0;
static uint64_t g_rb_test_last_val = 0;

static void ringbuf_test_cb(const void *data, size_t size, void *user_ctx)
{
    (void)user_ctx;
    if (size == sizeof(uint64_t)) {
        g_rb_test_count++;
        g_rb_test_last_val = *(const uint64_t *)data;
    }
}

/*
 * Test 5: BPF RingBuffer Operations
 */
static bool test_map_ringbuf(void)
{
    struct bpf_map *rb = cherry_ebpf_map_create("test_rb", BPF_MAP_TYPE_RINGBUF, 0, 0, 4096);
    if (!rb) return false;

    g_rb_test_count = 0;
    g_rb_test_last_val = 0;

    uint64_t event1 = 12345ULL;
    uint64_t event2 = 67890ULL;

    if (cherry_ebpf_ringbuf_output(rb, &event1, sizeof(event1), 0) != 0) return false;
    if (cherry_ebpf_ringbuf_output(rb, &event2, sizeof(event2), 0) != 0) return false;

    int read_cnt = cherry_ebpf_ringbuf_read(rb, ringbuf_test_cb, NULL);
    if (read_cnt != 2 || g_rb_test_count != 2 || g_rb_test_last_val != event2) {
        cherry_ebpf_map_destroy(rb);
        return false;
    }

    cherry_ebpf_map_destroy(rb);
    return true;
}

/*
 * Test 6: BPF Standard Helpers
 */
static bool test_helpers(void)
{
    /* Program calling:
     * 1. bpf_ktime_get_ns() -> stores into r6
     * 2. bpf_get_smp_processor_id() -> stores into r7
     * 3. r0 = (r6 > 0 && r7 < 8) ? 42 : 0
     * 4. exit
     */
    struct bpf_insn insns[] = {
        BPF_EMIT_CALL(BPF_FUNC_ktime_get_ns),
        BPF_MOV64_REG(BPF_REG_6, BPF_REG_0),

        BPF_EMIT_CALL(BPF_FUNC_get_smp_processor_id),
        BPF_MOV64_REG(BPF_REG_7, BPF_REG_0),

        BPF_MOV64_IMM(BPF_REG_0, 0),
        BPF_JMP_IMM(BPF_JEQ, BPF_REG_6, 0, 2), /* If ktime == 0, jump to exit */
        BPF_JMP_IMM(BPF_JGE, BPF_REG_7, 8, 1), /* If cpu >= 8, jump to exit */
        BPF_MOV64_IMM(BPF_REG_0, 42),          /* Success */
        BPF_EXIT_INSN(),
    };

    struct cherry_ebpf_prog *p = cherry_ebpf_prog_load("test_helpers", BPF_PROG_TYPE_UNSPEC, insns, sizeof(insns)/sizeof(insns[0]));
    if (!p) return false;

    uint64_t ret = 0;
    int err = cherry_ebpf_vm_exec(p, NULL, &ret);
    cherry_ebpf_prog_destroy(p);

    return (err == 0 && ret == 42);
}

/*
 * Test 7: Tracepoint Integration - Syscall Tracing
 */
static bool test_trace_syscall(void)
{
    struct bpf_map *map = cherry_ebpf_map_create("sys_trace_map", BPF_MAP_TYPE_ARRAY, sizeof(uint32_t), sizeof(uint64_t), 4);
    if (!map) return false;

    /* eBPF program attaches to CHERRY_TP_SYS_ENTER.
     * Context (r1): struct cherry_trace_sys_enter_ctx
     * ctx->syscall_id is at offset 16 (8 bytes).
     * Program reads ctx->syscall_id and increments map[0]!
     */
    struct bpf_insn insns[] = {
        /* Read ctx->syscall_id into r2 */
        BPF_LDX_MEM(BPF_DW, BPF_REG_2, BPF_REG_1, offsetof(struct cherry_trace_sys_enter_ctx, syscall_id)),

        /* Prepare key = 0 on stack: *(r10 - 4) = 0 */
        BPF_ST_MEM(BPF_W, BPF_REG_10, -4, 0),

        /* Call bpf_map_lookup_elem(map, &key) */
        BPF_LD_IMM64(BPF_REG_1, (uintptr_t)map),
        BPF_MOV64_REG(BPF_REG_2, BPF_REG_10),
        BPF_ALU64_IMM(BPF_ADD, BPF_REG_2, -4),
        BPF_EMIT_CALL(BPF_FUNC_map_lookup_elem),

        /* If lookup result r0 != 0, increment value (*r0)++ */
        BPF_JMP_IMM(BPF_JEQ, BPF_REG_0, 0, 3),
        BPF_LDX_MEM(BPF_DW, BPF_REG_1, BPF_REG_0, 0),
        BPF_ALU64_IMM(BPF_ADD, BPF_REG_1, 1),
        BPF_STX_MEM(BPF_DW, BPF_REG_0, BPF_REG_1, 0),

        BPF_MOV64_IMM(BPF_REG_0, 0),
        BPF_EXIT_INSN(),
    };

    struct cherry_ebpf_prog *p = cherry_ebpf_prog_load("tp_sys_prog", BPF_PROG_TYPE_TRACEPOINT, insns, sizeof(insns)/sizeof(insns[0]));
    if (!p) {
        cherry_ebpf_map_destroy(map);
        return false;
    }

    cherry_ebpf_prog_attach(p, CHERRY_TP_SYS_ENTER);

    /* Trigger 3 syscalls via hook */
    cherry_trace_sys_enter(64, 1, 2, 3, 0, 0, 0); /* sys_write */
    cherry_trace_sys_enter(63, 0, 0, 0, 0, 0, 0); /* sys_read */
    cherry_trace_sys_enter(172, 0, 0, 0, 0, 0, 0);/* sys_getpid */

    cherry_ebpf_prog_detach(p, CHERRY_TP_SYS_ENTER);
    uint32_t key = 0;
    uint64_t *val = (uint64_t *)cherry_ebpf_map_lookup(map, &key);
    bool pass = (val && *val == 3);

    cherry_ebpf_prog_destroy(p);
    cherry_ebpf_map_destroy(map);
    return pass;
}

/*
 * Test 8: Tracepoint Integration - Scheduler Context Switch Tracing
 */
static bool test_trace_sched(void)
{
    struct bpf_map *map = cherry_ebpf_map_create("sched_trace_map", BPF_MAP_TYPE_ARRAY, sizeof(uint32_t), sizeof(uint64_t), 4);
    if (!map) return false;

    struct bpf_insn insns[] = {
        BPF_ST_MEM(BPF_W, BPF_REG_10, -4, 0),
        BPF_LD_IMM64(BPF_REG_1, (uintptr_t)map),
        BPF_MOV64_REG(BPF_REG_2, BPF_REG_10),
        BPF_ALU64_IMM(BPF_ADD, BPF_REG_2, -4),
        BPF_EMIT_CALL(BPF_FUNC_map_lookup_elem),

        BPF_JMP_IMM(BPF_JEQ, BPF_REG_0, 0, 3),
        BPF_LDX_MEM(BPF_DW, BPF_REG_1, BPF_REG_0, 0),
        BPF_ALU64_IMM(BPF_ADD, BPF_REG_1, 1),
        BPF_STX_MEM(BPF_DW, BPF_REG_0, BPF_REG_1, 0),

        BPF_MOV64_IMM(BPF_REG_0, 0),
        BPF_EXIT_INSN(),
    };

    struct cherry_ebpf_prog *p = cherry_ebpf_prog_load("tp_sched_prog", BPF_PROG_TYPE_TRACEPOINT, insns, sizeof(insns)/sizeof(insns[0]));
    if (!p) {
        cherry_ebpf_map_destroy(map);
        return false;
    }

    cherry_ebpf_prog_attach(p, CHERRY_TP_SCHED_SWITCH);

    cherry_trace_sched_switch(1, "proc1", 2, "proc2", 0);
    cherry_trace_sched_switch(2, "proc2", 1, "proc1", 0);

    cherry_ebpf_prog_detach(p, CHERRY_TP_SCHED_SWITCH);
    cherry_ebpf_prog_destroy(p);

    uint32_t key = 0;
    uint64_t *val = (uint64_t *)cherry_ebpf_map_lookup(map, &key);
    bool pass = (val && *val >= 2);
    cherry_ebpf_map_destroy(map);
    return pass;
}

/*
 * Test 9: Tracepoint Integration - IRQ Tracing
 */
static bool test_trace_irq(void)
{
    struct bpf_map *map = cherry_ebpf_map_create("irq_trace_map", BPF_MAP_TYPE_ARRAY, sizeof(uint32_t), sizeof(uint64_t), 4);
    if (!map) return false;

    struct bpf_insn insns[] = {
        BPF_ST_MEM(BPF_W, BPF_REG_10, -4, 0),
        BPF_LD_IMM64(BPF_REG_1, (uintptr_t)map),
        BPF_MOV64_REG(BPF_REG_2, BPF_REG_10),
        BPF_ALU64_IMM(BPF_ADD, BPF_REG_2, -4),
        BPF_EMIT_CALL(BPF_FUNC_map_lookup_elem),

        BPF_JMP_IMM(BPF_JEQ, BPF_REG_0, 0, 3),
        BPF_LDX_MEM(BPF_DW, BPF_REG_1, BPF_REG_0, 0),
        BPF_ALU64_IMM(BPF_ADD, BPF_REG_1, 1),
        BPF_STX_MEM(BPF_DW, BPF_REG_0, BPF_REG_1, 0),

        BPF_MOV64_IMM(BPF_REG_0, 0),
        BPF_EXIT_INSN(),
    };

    struct cherry_ebpf_prog *p = cherry_ebpf_prog_load("tp_irq_prog", BPF_PROG_TYPE_TRACEPOINT, insns, sizeof(insns)/sizeof(insns[0]));
    if (!p) {
        cherry_ebpf_map_destroy(map);
        return false;
    }

    cherry_ebpf_prog_attach(p, CHERRY_TP_IRQ_ENTER);

    cherry_trace_irq_enter(27, 0);
    cherry_trace_irq_enter(33, 0);

    cherry_ebpf_prog_detach(p, CHERRY_TP_IRQ_ENTER);
    cherry_ebpf_prog_destroy(p);

    uint32_t key = 0;
    uint64_t *val = (uint64_t *)cherry_ebpf_map_lookup(map, &key);
    bool pass = (val && *val >= 2);
    cherry_ebpf_map_destroy(map);
    return pass;
}

/*
 * Test 10: Tracepoint Integration - VFS Tracing
 */
static bool test_trace_vfs(void)
{
    struct bpf_map *map = cherry_ebpf_map_create("vfs_trace_map", BPF_MAP_TYPE_ARRAY, sizeof(uint32_t), sizeof(uint64_t), 4);
    if (!map) return false;

    struct bpf_insn insns[] = {
        BPF_ST_MEM(BPF_W, BPF_REG_10, -4, 0),
        BPF_LD_IMM64(BPF_REG_1, (uintptr_t)map),
        BPF_MOV64_REG(BPF_REG_2, BPF_REG_10),
        BPF_ALU64_IMM(BPF_ADD, BPF_REG_2, -4),
        BPF_EMIT_CALL(BPF_FUNC_map_lookup_elem),

        BPF_JMP_IMM(BPF_JEQ, BPF_REG_0, 0, 3),
        BPF_LDX_MEM(BPF_DW, BPF_REG_1, BPF_REG_0, 0),
        BPF_ALU64_IMM(BPF_ADD, BPF_REG_1, 1),
        BPF_STX_MEM(BPF_DW, BPF_REG_0, BPF_REG_1, 0),

        BPF_MOV64_IMM(BPF_REG_0, 0),
        BPF_EXIT_INSN(),
    };

    struct cherry_ebpf_prog *p = cherry_ebpf_prog_load("tp_vfs_prog", BPF_PROG_TYPE_TRACEPOINT, insns, sizeof(insns)/sizeof(insns[0]));
    if (!p) {
        cherry_ebpf_map_destroy(map);
        return false;
    }

    cherry_ebpf_prog_attach(p, CHERRY_TP_VFS_WRITE);

    cherry_trace_vfs_write(1, 128, 128);
    cherry_trace_vfs_write(2, 64, 64);

    cherry_ebpf_prog_detach(p, CHERRY_TP_VFS_WRITE);
    cherry_ebpf_prog_destroy(p);

    uint32_t key = 0;
    uint64_t *val = (uint64_t *)cherry_ebpf_map_lookup(map, &key);
    bool pass = (val && *val >= 2);
    cherry_ebpf_map_destroy(map);
    return pass;
}

/*
 * Test 11: Tracepoint Integration - Block I/O Tracing
 */
static bool test_trace_block(void)
{
    struct bpf_map *map = cherry_ebpf_map_create("blk_trace_map", BPF_MAP_TYPE_ARRAY, sizeof(uint32_t), sizeof(uint64_t), 4);
    if (!map) return false;

    struct bpf_insn insns[] = {
        BPF_ST_MEM(BPF_W, BPF_REG_10, -4, 0),
        BPF_LD_IMM64(BPF_REG_1, (uintptr_t)map),
        BPF_MOV64_REG(BPF_REG_2, BPF_REG_10),
        BPF_ALU64_IMM(BPF_ADD, BPF_REG_2, -4),
        BPF_EMIT_CALL(BPF_FUNC_map_lookup_elem),

        BPF_JMP_IMM(BPF_JEQ, BPF_REG_0, 0, 3),
        BPF_LDX_MEM(BPF_DW, BPF_REG_1, BPF_REG_0, 0),
        BPF_ALU64_IMM(BPF_ADD, BPF_REG_1, 1),
        BPF_STX_MEM(BPF_DW, BPF_REG_0, BPF_REG_1, 0),

        BPF_MOV64_IMM(BPF_REG_0, 0),
        BPF_EXIT_INSN(),
    };

    struct cherry_ebpf_prog *p = cherry_ebpf_prog_load("tp_blk_prog", BPF_PROG_TYPE_TRACEPOINT, insns, sizeof(insns)/sizeof(insns[0]));
    if (!p) {
        cherry_ebpf_map_destroy(map);
        return false;
    }

    cherry_ebpf_prog_attach(p, CHERRY_TP_BLOCK_RQ_ISSUE);

    cherry_trace_block_rq_issue("sda", 2048, 8, 0);

    cherry_ebpf_prog_detach(p, CHERRY_TP_BLOCK_RQ_ISSUE);
    cherry_ebpf_prog_destroy(p);

    uint32_t key = 0;
    uint64_t *val = (uint64_t *)cherry_ebpf_map_lookup(map, &key);
    bool pass = (val && *val == 1);
    cherry_ebpf_map_destroy(map);
    return pass;
}

/*
 * Test 12: Tracepoint Integration - Network Packet Tracing
 */
static bool test_trace_net(void)
{
    struct bpf_map *map = cherry_ebpf_map_create("net_trace_map", BPF_MAP_TYPE_ARRAY, sizeof(uint32_t), sizeof(uint64_t), 4);
    if (!map) return false;

    struct bpf_insn insns[] = {
        BPF_ST_MEM(BPF_W, BPF_REG_10, -4, 0),
        BPF_LD_IMM64(BPF_REG_1, (uintptr_t)map),
        BPF_MOV64_REG(BPF_REG_2, BPF_REG_10),
        BPF_ALU64_IMM(BPF_ADD, BPF_REG_2, -4),
        BPF_EMIT_CALL(BPF_FUNC_map_lookup_elem),

        BPF_JMP_IMM(BPF_JEQ, BPF_REG_0, 0, 3),
        BPF_LDX_MEM(BPF_DW, BPF_REG_1, BPF_REG_0, 0),
        BPF_ALU64_IMM(BPF_ADD, BPF_REG_1, 1),
        BPF_STX_MEM(BPF_DW, BPF_REG_0, BPF_REG_1, 0),

        BPF_MOV64_IMM(BPF_REG_0, 0),
        BPF_EXIT_INSN(),
    };

    struct cherry_ebpf_prog *p = cherry_ebpf_prog_load("tp_net_prog", BPF_PROG_TYPE_TRACEPOINT, insns, sizeof(insns)/sizeof(insns[0]));
    if (!p) {
        cherry_ebpf_map_destroy(map);
        return false;
    }

    cherry_ebpf_prog_attach(p, CHERRY_TP_NET_TX);

    cherry_trace_net_tx(NULL, 1500, 0x0800, 0);

    cherry_ebpf_prog_detach(p, CHERRY_TP_NET_TX);
    cherry_ebpf_prog_destroy(p);

    uint32_t key = 0;
    uint64_t *val = (uint64_t *)cherry_ebpf_map_lookup(map, &key);
    bool pass = (val && *val == 1);
    cherry_ebpf_map_destroy(map);
    return pass;
}

/*
 * Master Acceptance Test Runner
 */
int cherry_ebpf_run_tests(void)
{
    printk("cherry_ebpf: starting acceptance test suite...\n");

    bool vm_pass         = test_vm_arithmetic();
    bool verifier_pass   = test_verifier_safety();
    bool map_hash_pass   = test_map_hash();
    bool map_array_pass  = test_map_array();
    bool ringbuf_pass    = test_map_ringbuf();
    bool helpers_pass    = test_helpers();
    bool trace_sys_pass  = test_trace_syscall();
    bool trace_sched_pass= test_trace_sched();
    bool trace_irq_pass  = test_trace_irq();
    bool trace_vfs_pass  = test_trace_vfs();
    bool trace_blk_pass  = test_trace_block();
    bool trace_net_pass  = test_trace_net();

    bool all_pass = vm_pass && verifier_pass && map_hash_pass && map_array_pass &&
                    ringbuf_pass && helpers_pass && trace_sys_pass && trace_sched_pass &&
                    trace_irq_pass && trace_vfs_pass && trace_blk_pass && trace_net_pass;

    printk("cherry_ebpf: [ebpf-verify] vm=%s verifier=%s map_hash=%s map_array=%s ringbuf=%s helpers=%s trace_syscall=%s trace_sched=%s trace_irq=%s trace_vfs=%s trace_block=%s trace_net=%s all=%s\n",
           vm_pass ? "pass" : "FAIL",
           verifier_pass ? "pass" : "FAIL",
           map_hash_pass ? "pass" : "FAIL",
           map_array_pass ? "pass" : "FAIL",
           ringbuf_pass ? "pass" : "FAIL",
           helpers_pass ? "pass" : "FAIL",
           trace_sys_pass ? "pass" : "FAIL",
           trace_sched_pass ? "pass" : "FAIL",
           trace_irq_pass ? "pass" : "FAIL",
           trace_vfs_pass ? "pass" : "FAIL",
           trace_blk_pass ? "pass" : "FAIL",
           trace_net_pass ? "pass" : "FAIL",
           all_pass ? "pass" : "FAIL");

    return all_pass ? 0 : -1;
}
