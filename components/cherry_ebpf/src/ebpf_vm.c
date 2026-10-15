/*
 * CherryEBPF - eBPF 64-bit Virtual Machine Interpreter & Verifier
 *
 * Copyright (c) 2026 ForgeStitch
 * SPDX-License-Identifier: MIT
 */

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include "ebpf_insn.h"
#include "ebpf_vm.h"
#include "cherry_kal.h"

extern int printk(const char *fmt, ...);
extern int snprintf(char *str, size_t size, const char *format, ...);

static struct cherry_ebpf_prog *g_prog_list = NULL;
static uint32_t g_next_prog_id = 1;
static volatile uint32_t g_prog_lock = 0;

static inline void prog_lock(void) {
    while (__atomic_test_and_set(&g_prog_lock, __ATOMIC_ACQUIRE)) {
        __asm__ volatile("yield");
    }
}

static inline void prog_unlock(void) {
    __atomic_clear(&g_prog_lock, __ATOMIC_RELEASE);
}

/*
 * Static Safety Verifier
 */
int cherry_ebpf_verify(const struct bpf_insn *insns, uint32_t insn_cnt, uint32_t prog_type)
{
    (void)prog_type;
    if (!insns || insn_cnt == 0 || insn_cnt > CHERRY_EBPF_MAX_INSNS) {
        return -1; /* Invalid count */
    }

    bool has_exit = false;

    for (uint32_t i = 0; i < insn_cnt; i++) {
        const struct bpf_insn *insn = &insns[i];
        uint8_t cls = BPF_CLASS(insn->code);

        /* Register bounds check */
        if (insn->dst_reg >= MAX_BPF_REG || insn->src_reg >= MAX_BPF_REG) {
            return -2; /* Bad register */
        }

        /* R10 (FP) is strictly read-only */
        if (cls != BPF_JMP && cls != BPF_JMP32) {
            if (insn->dst_reg == BPF_REG_FP && cls != BPF_STX && cls != BPF_ST) {
                return -3; /* Cannot write to FP */
            }
        }

        /* Check wide 64-bit immediate load */
        if (insn->code == (BPF_LD | BPF_DW | BPF_IMM)) {
            if (i + 1 >= insn_cnt) {
                return -4; /* Incomplete LD_IMM64 */
            }
            if (insns[i + 1].code != 0) {
                return -5; /* Expected zero code in next slot */
            }
            i++; /* Skip pseudo-instruction */
            continue;
        }

        /* Check division by constant zero */
        if (cls == BPF_ALU || cls == BPF_ALU64) {
            uint8_t op = BPF_OP(insn->code);
            if ((op == BPF_DIV || op == BPF_MOD) && BPF_SRC(insn->code) == BPF_K) {
                if (insn->imm == 0) {
                    return -6; /* Division by zero */
                }
            }
        }

        /* Check jump targets */
        if (cls == BPF_JMP || cls == BPF_JMP32) {
            uint8_t op = BPF_OP(insn->code);
            if (op == BPF_EXIT) {
                has_exit = true;
            } else if (op != BPF_CALL) {
                int32_t target = (int32_t)i + 1 + insn->off;
                if (target < 0 || (uint32_t)target >= insn_cnt) {
                    return -7; /* Jump out of bounds */
                }
            }
        }

        /* Check stack access bounds relative to R10 */
        if ((cls == BPF_LDX || cls == BPF_STX || cls == BPF_ST) &&
            (insn->src_reg == BPF_REG_FP || insn->dst_reg == BPF_REG_FP)) {
            int16_t off = insn->off;
            if (off > 0 || off < -CHERRY_EBPF_STACK_SIZE) {
                return -8; /* Stack out of bounds */
            }
        }
    }

    if (!has_exit) {
        return -9; /* Program must have an EXIT instruction */
    }

    return 0;
}

/*
 * Load Program
 */
struct cherry_ebpf_prog *cherry_ebpf_prog_load(const char *name, uint32_t type,
                                               const struct bpf_insn *insns,
                                               uint32_t insn_cnt)
{
    if (cherry_ebpf_verify(insns, insn_cnt, type) != 0) {
        return NULL;
    }

    struct cherry_ebpf_prog *prog = (struct cherry_ebpf_prog *)kmalloc(sizeof(*prog), GFP_KERNEL);
    if (!prog) return NULL;

    memset(prog, 0, sizeof(*prog));
    prog->insns = (struct bpf_insn *)kmalloc(sizeof(struct bpf_insn) * insn_cnt, GFP_KERNEL);
    if (!prog->insns) {
        kfree(prog);
        return NULL;
    }

    memcpy(prog->insns, insns, sizeof(struct bpf_insn) * insn_cnt);
    prog->insn_cnt = insn_cnt;
    prog->type = type;
    if (name) {
        strncpy(prog->name, name, sizeof(prog->name) - 1);
    } else {
        snprintf(prog->name, sizeof(prog->name), "prog_%u", g_next_prog_id);
    }

    prog_lock();
    prog->id = g_next_prog_id++;
    prog->next = g_prog_list;
    g_prog_list = prog;
    prog_unlock();

    return prog;
}

/*
 * Destroy Program
 */
void cherry_ebpf_prog_destroy(struct cherry_ebpf_prog *prog)
{
    if (!prog) return;

    prog_lock();
    struct cherry_ebpf_prog **curr = &g_prog_list;
    while (*curr) {
        if (*curr == prog) {
            *curr = prog->next;
            break;
        }
        curr = &(*curr)->next;
    }
    prog_unlock();

    if (prog->insns) {
        kfree(prog->insns);
    }
    kfree(prog);
}

struct cherry_ebpf_prog *cherry_ebpf_prog_find_by_id(uint32_t id)
{
    prog_lock();
    struct cherry_ebpf_prog *p = g_prog_list;
    while (p) {
        if (p->id == id) {
            prog_unlock();
            return p;
        }
        p = p->next;
    }
    prog_unlock();
    return NULL;
}

struct cherry_ebpf_prog *cherry_ebpf_prog_find_by_name(const char *name)
{
    if (!name) return NULL;
    prog_lock();
    struct cherry_ebpf_prog *p = g_prog_list;
    while (p) {
        if (strcmp(p->name, name) == 0) {
            prog_unlock();
            return p;
        }
        p = p->next;
    }
    prog_unlock();
    return NULL;
}

int cherry_ebpf_prog_for_each(int (*cb)(struct cherry_ebpf_prog *prog, void *arg), void *arg)
{
    if (!cb) return -1;
    prog_lock();
    struct cherry_ebpf_prog *p = g_prog_list;
    while (p) {
        int ret = cb(p, arg);
        if (ret != 0) {
            prog_unlock();
            return ret;
        }
        p = p->next;
    }
    prog_unlock();
    return 0;
}

uint32_t cherry_ebpf_prog_count(void)
{
    uint32_t cnt = 0;
    prog_lock();
    struct cherry_ebpf_prog *p = g_prog_list;
    while (p) {
        cnt++;
        p = p->next;
    }
    prog_unlock();
    return cnt;
}

/*
 * 64-bit eBPF Virtual Machine Execution Loop
 */
#ifndef configNUMBER_OF_CORES
#define configNUMBER_OF_CORES 8
#endif

static uint8_t g_percpu_ebpf_stack[configNUMBER_OF_CORES][CHERRY_EBPF_STACK_SIZE] __attribute__((aligned(8)));

static inline uint32_t vm_get_cpu_id(void)
{
    uint64_t mpidr;
    __asm__ volatile("mrs %0, mpidr_el1" : "=r"(mpidr));
    return (uint32_t)(mpidr & 0xff);
}

int cherry_ebpf_vm_exec(struct cherry_ebpf_prog *prog, void *ctx, uint64_t *ret_val)
{
    if (!prog || !prog->insns) return -1;

    uint64_t regs[MAX_BPF_REG] = {0};
    uint32_t cpu = vm_get_cpu_id();
    if (cpu >= configNUMBER_OF_CORES) cpu = 0;
    uint8_t *stack = g_percpu_ebpf_stack[cpu];
    memset(stack, 0, CHERRY_EBPF_STACK_SIZE);

    /* Initialize registers */
    regs[BPF_REG_FP] = (uint64_t)(uintptr_t)&stack[CHERRY_EBPF_STACK_SIZE];
    regs[BPF_REG_1]  = (uint64_t)(uintptr_t)ctx;

    uint32_t pc = 0;
    uint32_t steps = 0;
    const struct bpf_insn *insns = prog->insns;
    uint32_t count = prog->insn_cnt;

    while (pc < count && steps < CHERRY_EBPF_MAX_STEPS) {
        steps++;
        const struct bpf_insn *insn = &insns[pc];
        uint8_t cls = BPF_CLASS(insn->code);
        uint8_t dst = insn->dst_reg;
        uint8_t src = insn->src_reg;

        switch (cls) {
        /* 64-bit ALU */
        case BPF_ALU64: {
            uint64_t s_val = (BPF_SRC(insn->code) == BPF_X) ? regs[src] : (uint64_t)(int64_t)insn->imm;
            switch (BPF_OP(insn->code)) {
            case BPF_ADD:  regs[dst] += s_val; break;
            case BPF_SUB:  regs[dst] -= s_val; break;
            case BPF_MUL:  regs[dst] *= s_val; break;
            case BPF_DIV:  if (s_val == 0) return -2; regs[dst] /= s_val; break;
            case BPF_OR:   regs[dst] |= s_val; break;
            case BPF_AND:  regs[dst] &= s_val; break;
            case BPF_LSH:  regs[dst] <<= (s_val & 63); break;
            case BPF_RSH:  regs[dst] >>= (s_val & 63); break;
            case BPF_NEG:  regs[dst] = (uint64_t)(-(int64_t)regs[dst]); break;
            case BPF_MOD:  if (s_val == 0) return -2; regs[dst] %= s_val; break;
            case BPF_XOR:  regs[dst] ^= s_val; break;
            case BPF_MOV:  regs[dst] = s_val; break;
            case BPF_ARSH: regs[dst] = (uint64_t)((int64_t)regs[dst] >> (s_val & 63)); break;
            default: return -3;
            }
            break;
        }

        /* 32-bit ALU */
        case BPF_ALU: {
            if (BPF_OP(insn->code) == BPF_END) {
                /* Byte swap */
                uint32_t bits = (uint32_t)insn->imm;
                if (bits == 16) {
                    regs[dst] = __builtin_bswap16((uint16_t)regs[dst]);
                } else if (bits == 32) {
                    regs[dst] = __builtin_bswap32((uint32_t)regs[dst]);
                } else if (bits == 64) {
                    regs[dst] = __builtin_bswap64(regs[dst]);
                }
                break;
            }

            uint32_t s_val = (BPF_SRC(insn->code) == BPF_X) ? (uint32_t)regs[src] : (uint32_t)insn->imm;
            uint32_t d_val = (uint32_t)regs[dst];

            switch (BPF_OP(insn->code)) {
            case BPF_ADD:  d_val += s_val; break;
            case BPF_SUB:  d_val -= s_val; break;
            case BPF_MUL:  d_val *= s_val; break;
            case BPF_DIV:  if (s_val == 0) return -2; d_val /= s_val; break;
            case BPF_OR:   d_val |= s_val; break;
            case BPF_AND:  d_val &= s_val; break;
            case BPF_LSH:  d_val <<= (s_val & 31); break;
            case BPF_RSH:  d_val >>= (s_val & 31); break;
            case BPF_NEG:  d_val = (uint32_t)(-(int32_t)d_val); break;
            case BPF_MOD:  if (s_val == 0) return -2; d_val %= s_val; break;
            case BPF_XOR:  d_val ^= s_val; break;
            case BPF_MOV:  d_val = s_val; break;
            case BPF_ARSH: d_val = (uint32_t)((int32_t)d_val >> (s_val & 31)); break;
            default: return -3;
            }
            regs[dst] = d_val; /* Zero-extended to 64-bit */
            break;
        }

        /* 64-bit Jump */
        case BPF_JMP: {
            uint8_t op = BPF_OP(insn->code);
            if (op == BPF_EXIT) {
                if (ret_val) *ret_val = regs[BPF_REG_0];
                prog->run_count++;
                return 0;
            }
            if (op == BPF_CALL) {
                regs[BPF_REG_0] = cherry_ebpf_call_helper((uint32_t)insn->imm,
                                                          regs[BPF_REG_1],
                                                          regs[BPF_REG_2],
                                                          regs[BPF_REG_3],
                                                          regs[BPF_REG_4],
                                                          regs[BPF_REG_5]);
                break;
            }
            if (op == BPF_JA) {
                pc += insn->off;
                break;
            }

            uint64_t s_val = (BPF_SRC(insn->code) == BPF_X) ? regs[src] : (uint64_t)(int64_t)insn->imm;
            bool cond = false;

            switch (op) {
            case BPF_JEQ:  cond = (regs[dst] == s_val); break;
            case BPF_JGT:  cond = (regs[dst] > s_val); break;
            case BPF_JGE:  cond = (regs[dst] >= s_val); break;
            case BPF_JSET: cond = ((regs[dst] & s_val) != 0); break;
            case BPF_JNE:  cond = (regs[dst] != s_val); break;
            case BPF_JSGT: cond = ((int64_t)regs[dst] > (int64_t)s_val); break;
            case BPF_JSGE: cond = ((int64_t)regs[dst] >= (int64_t)s_val); break;
            case BPF_JLT:  cond = (regs[dst] < s_val); break;
            case BPF_JLE:  cond = (regs[dst] <= s_val); break;
            case BPF_JSLT: cond = ((int64_t)regs[dst] < (int64_t)s_val); break;
            case BPF_JSLE: cond = ((int64_t)regs[dst] <= (int64_t)s_val); break;
            default: return -4;
            }

            if (cond) {
                pc += insn->off;
            }
            break;
        }

        /* 32-bit Jump */
        case BPF_JMP32: {
            uint8_t op = BPF_OP(insn->code);
            uint32_t s_val = (BPF_SRC(insn->code) == BPF_X) ? (uint32_t)regs[src] : (uint32_t)insn->imm;
            uint32_t d_val = (uint32_t)regs[dst];
            bool cond = false;

            switch (op) {
            case BPF_JEQ:  cond = (d_val == s_val); break;
            case BPF_JGT:  cond = (d_val > s_val); break;
            case BPF_JGE:  cond = (d_val >= s_val); break;
            case BPF_JSET: cond = ((d_val & s_val) != 0); break;
            case BPF_JNE:  cond = (d_val != s_val); break;
            case BPF_JSGT: cond = ((int32_t)d_val > (int32_t)s_val); break;
            case BPF_JSGE: cond = ((int32_t)d_val >= (int32_t)s_val); break;
            case BPF_JLT:  cond = (d_val < s_val); break;
            case BPF_JLE:  cond = (d_val <= s_val); break;
            case BPF_JSLT: cond = ((int32_t)d_val < (int32_t)s_val); break;
            case BPF_JSLE: cond = ((int32_t)d_val <= (int32_t)s_val); break;
            default: return -4;
            }

            if (cond) {
                pc += insn->off;
            }
            break;
        }

        /* Load from memory */
        case BPF_LDX: {
            uintptr_t addr = (uintptr_t)(regs[src] + insn->off);
            switch (BPF_SIZE(insn->code)) {
            case BPF_B:  regs[dst] = *(const uint8_t *)addr; break;
            case BPF_H:  regs[dst] = *(const uint16_t *)addr; break;
            case BPF_W:  regs[dst] = *(const uint32_t *)addr; break;
            case BPF_DW: regs[dst] = *(const uint64_t *)addr; break;
            default: return -5;
            }
            break;
        }

        /* Store register to memory */
        case BPF_STX: {
            uintptr_t addr = (uintptr_t)(regs[dst] + insn->off);
            switch (BPF_SIZE(insn->code)) {
            case BPF_B:  *(uint8_t *)addr = (uint8_t)regs[src]; break;
            case BPF_H:  *(uint16_t *)addr = (uint16_t)regs[src]; break;
            case BPF_W:  *(uint32_t *)addr = (uint32_t)regs[src]; break;
            case BPF_DW: *(uint64_t *)addr = regs[src]; break;
            default: return -5;
            }
            break;
        }

        /* Store immediate to memory */
        case BPF_ST: {
            uintptr_t addr = (uintptr_t)(regs[dst] + insn->off);
            switch (BPF_SIZE(insn->code)) {
            case BPF_B:  *(uint8_t *)addr = (uint8_t)insn->imm; break;
            case BPF_H:  *(uint16_t *)addr = (uint16_t)insn->imm; break;
            case BPF_W:  *(uint32_t *)addr = (uint32_t)insn->imm; break;
            case BPF_DW: *(uint64_t *)addr = (uint64_t)(uint32_t)insn->imm; break;
            default: return -5;
            }
            break;
        }

        /* 64-bit Wide Immediate Load */
        case BPF_LD: {
            if (insn->code == (BPF_LD | BPF_DW | BPF_IMM)) {
                uint32_t low = (uint32_t)insn->imm;
                uint32_t high = (uint32_t)insns[pc + 1].imm;
                regs[dst] = ((uint64_t)high << 32) | (uint64_t)low;
                pc++; /* Skip second half */
                break;
            }
            return -6;
        }

        default:
            return -7;
        }

        pc++;
    }

    if (steps >= CHERRY_EBPF_MAX_STEPS) {
        return -8; /* Execution timeout / infinite loop detected */
    }

    if (ret_val) *ret_val = regs[BPF_REG_0];
    prog->run_count++;
    return 0;
}
