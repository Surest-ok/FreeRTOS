/*
 * CherryEBPF - eBPF 64-bit Instruction Set Architecture Definitions
 *
 * Copyright (c) 2026 ForgeStitch
 * SPDX-License-Identifier: MIT
 *
 * Standard Linux eBPF Instruction Set Architecture (RV64/eBPF 64-bit ISA).
 * Supports 64-bit ALU, 32-bit ALU, JMP, JMP32, LD, LDX, ST, STX, and Wide 64-bit imm.
 */

#ifndef _CHERRY_EBPF_INSN_H_
#define _CHERRY_EBPF_INSN_H_

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * eBPF Register Identifiers (0-10)
 * R0     - Return value from program or helper call
 * R1-R5  - Function call argument registers / scratch registers
 * R6-R9  - Callee-saved registers
 * R10    - Read-only Frame Pointer (FP) to 512-byte stack
 */
#define BPF_REG_0   0
#define BPF_REG_1   1
#define BPF_REG_2   2
#define BPF_REG_3   3
#define BPF_REG_4   4
#define BPF_REG_5   5
#define BPF_REG_6   6
#define BPF_REG_7   7
#define BPF_REG_8   8
#define BPF_REG_9   9
#define BPF_REG_10  10
#define BPF_REG_FP  BPF_REG_10
#define MAX_BPF_REG 11

/*
 * Standard 8-byte eBPF Instruction Format
 */
struct bpf_insn {
    uint8_t  code;        /* Opcode */
    uint8_t  dst_reg:4;   /* Destination register */
    uint8_t  src_reg:4;   /* Source register */
    int16_t  off;         /* Signed 16-bit offset */
    int32_t  imm;         /* Signed 32-bit immediate */
};

/* Instruction Classes (code & 0x07) */
#define BPF_CLASS(code) ((code) & 0x07)
#define BPF_LD          0x00
#define BPF_LDX         0x01
#define BPF_ST          0x02
#define BPF_STX         0x03
#define BPF_ALU         0x04
#define BPF_JMP         0x05
#define BPF_JMP32       0x06
#define BPF_ALU64       0x07

/* Size Modifiers for LD/LDX/ST/STX (code & 0x18) */
#define BPF_SIZE(code)  ((code) & 0x18)
#define BPF_W           0x00    /* 32-bit word */
#define BPF_H           0x08    /* 16-bit half-word */
#define BPF_B           0x10    /* 8-bit byte */
#define BPF_DW          0x18    /* 64-bit double-word */

/* Mode Modifiers for LD/LDX/ST/STX (code & 0xe0) */
#define BPF_MODE(code)  ((code) & 0xe0)
#define BPF_IMM         0x00
#define BPF_ABS         0x20
#define BPF_IND         0x40
#define BPF_MEM         0x60
#define BPF_ATOMIC      0xc0

/* Source Modifiers (code & 0x08) */
#define BPF_SRC(code)   ((code) & 0x08)
#define BPF_K           0x00    /* Use 32-bit immediate as source */
#define BPF_X           0x08    /* Use src_reg as source */

/* ALU / JMP Operations (code & 0xf0) */
#define BPF_OP(code)    ((code) & 0xf0)
#define BPF_ADD         0x00
#define BPF_SUB         0x10
#define BPF_MUL         0x20
#define BPF_DIV         0x30
#define BPF_OR          0x40
#define BPF_AND         0x50
#define BPF_LSH         0x60
#define BPF_RSH         0x70
#define BPF_NEG         0x80
#define BPF_MOD         0x90
#define BPF_XOR         0xa0
#define BPF_MOV         0xb0
#define BPF_ARSH        0xc0    /* Arithmetic right shift */
#define BPF_END         0xd0    /* Byte swap / Endianness */

/* Endianness modifiers for BPF_END (source flag) */
#define BPF_TO_LE       0x00
#define BPF_TO_BE       0x08

/* Jump Operations (code & 0xf0) */
#define BPF_JA          0x00    /* Unconditional jump: pc += off */
#define BPF_JEQ         0x10    /* == */
#define BPF_JGT         0x20    /* >  (unsigned) */
#define BPF_JGE         0x30    /* >= (unsigned) */
#define BPF_JSET        0x40    /* & */
#define BPF_JNE         0x50    /* != */
#define BPF_JSGT        0x60    /* >  (signed) */
#define BPF_JSGE        0x70    /* >= (signed) */
#define BPF_CALL        0x80    /* Function call */
#define BPF_EXIT        0x90    /* Program exit */
#define BPF_JLT         0xa0    /* <  (unsigned) */
#define BPF_JLE         0xb0    /* <= (unsigned) */
#define BPF_JSLT        0xc0    /* <  (signed) */
#define BPF_JSLE        0xd0    /* <= (signed) */

/*
 * Helper Macros to construct eBPF Bytecode in C
 */
#define BPF_RAW_INSN(c, dst, src, o, i) \
    ((struct bpf_insn){ .code = (uint8_t)(c), .dst_reg = (uint8_t)(dst), .src_reg = (uint8_t)(src), .off = (int16_t)(o), .imm = (int32_t)(i) })

/* ALU64 Instructions */
#define BPF_ALU64_REG(op, dst, src) \
    BPF_RAW_INSN(BPF_ALU64 | BPF_OP(op) | BPF_X, dst, src, 0, 0)
#define BPF_ALU64_IMM(op, dst, imm) \
    BPF_RAW_INSN(BPF_ALU64 | BPF_OP(op) | BPF_K, dst, 0, 0, imm)
#define BPF_MOV64_REG(dst, src) \
    BPF_RAW_INSN(BPF_ALU64 | BPF_MOV | BPF_X, dst, src, 0, 0)
#define BPF_MOV64_IMM(dst, imm) \
    BPF_RAW_INSN(BPF_ALU64 | BPF_MOV | BPF_K, dst, 0, 0, imm)
#define BPF_NEG64(dst) \
    BPF_RAW_INSN(BPF_ALU64 | BPF_NEG | BPF_K, dst, 0, 0, 0)

/* ALU32 Instructions */
#define BPF_ALU32_REG(op, dst, src) \
    BPF_RAW_INSN(BPF_ALU | BPF_OP(op) | BPF_X, dst, src, 0, 0)
#define BPF_ALU32_IMM(op, dst, imm) \
    BPF_RAW_INSN(BPF_ALU | BPF_OP(op) | BPF_K, dst, 0, 0, imm)
#define BPF_MOV32_REG(dst, src) \
    BPF_RAW_INSN(BPF_ALU | BPF_MOV | BPF_X, dst, src, 0, 0)
#define BPF_MOV32_IMM(dst, imm) \
    BPF_RAW_INSN(BPF_ALU | BPF_MOV | BPF_K, dst, 0, 0, imm)
#define BPF_NEG32(dst) \
    BPF_RAW_INSN(BPF_ALU | BPF_NEG | BPF_K, dst, 0, 0, 0)

/* Endianness byte swap */
#define BPF_ENDIAN(src, dst, bits) \
    BPF_RAW_INSN(BPF_ALU | BPF_END | (src), dst, 0, 0, bits)

/* Memory Operations */
#define BPF_LDX_MEM(sz, dst, src, off) \
    BPF_RAW_INSN(BPF_LDX | BPF_SIZE(sz) | BPF_MEM, dst, src, off, 0)
#define BPF_STX_MEM(sz, dst, src, off) \
    BPF_RAW_INSN(BPF_STX | BPF_SIZE(sz) | BPF_MEM, dst, src, off, 0)
#define BPF_ST_MEM(sz, dst, off, imm) \
    BPF_RAW_INSN(BPF_ST | BPF_SIZE(sz) | BPF_MEM, dst, 0, off, imm)

/* 64-bit Wide Immediate Load (BPF_LD_IMM64 takes 2 insn slots) */
#define BPF_LD_IMM64(dst, imm64) \
    BPF_RAW_INSN(BPF_LD | BPF_DW | BPF_IMM, dst, 0, 0, (int32_t)((uint64_t)(imm64) & 0xffffffffULL)), \
    BPF_RAW_INSN(0, 0, 0, 0, (int32_t)(((uint64_t)(imm64) >> 32) & 0xffffffffULL))

/* Jump Instructions (64-bit comparison) */
#define BPF_JMP_A(off) \
    BPF_RAW_INSN(BPF_JMP | BPF_JA, 0, 0, off, 0)
#define BPF_JMP_REG(op, dst, src, off) \
    BPF_RAW_INSN(BPF_JMP | BPF_OP(op) | BPF_X, dst, src, off, 0)
#define BPF_JMP_IMM(op, dst, imm, off) \
    BPF_RAW_INSN(BPF_JMP | BPF_OP(op) | BPF_K, dst, 0, off, imm)

/* Jump Instructions (32-bit comparison) */
#define BPF_JMP32_REG(op, dst, src, off) \
    BPF_RAW_INSN(BPF_JMP32 | BPF_OP(op) | BPF_X, dst, src, off, 0)
#define BPF_JMP32_IMM(op, dst, imm, off) \
    BPF_RAW_INSN(BPF_JMP32 | BPF_OP(op) | BPF_K, dst, 0, off, imm)

/* Helper Call and Exit */
#define BPF_EMIT_CALL(func_id) \
    BPF_RAW_INSN(BPF_JMP | BPF_CALL, 0, 0, 0, func_id)
#define BPF_EXIT_INSN() \
    BPF_RAW_INSN(BPF_JMP | BPF_EXIT, 0, 0, 0, 0)

#ifdef __cplusplus
}
#endif

#endif /* _CHERRY_EBPF_INSN_H_ */
