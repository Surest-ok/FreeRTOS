/*
 * CherryProcess - Architecture Execution Trampoline for AArch64
 *
 * Copyright (c) 2026 ForgeStitch
 * SPDX-License-Identifier: MIT
 *
 * Provides decoupled ELF user-mode entry & host register restoration:
 *   int cherry_arch_enter_elf(uintptr_t entry_point, uintptr_t user_sp);
 */

.section .text
.align 4
.global cherry_arch_enter_elf
.type cherry_arch_enter_elf, %function

cherry_arch_enter_elf:
    /* Arguments:
     *   X0 = entry_point
     *   X1 = user_sp (System V ABI aligned stack pointer)
     */

    /* 1. Save host callee-saved registers on host stack */
    stp     x29, x30, [sp, #-16]!
    stp     x27, x28, [sp, #-16]!
    stp     x25, x26, [sp, #-16]!
    stp     x23, x24, [sp, #-16]!
    stp     x21, x22, [sp, #-16]!
    stp     x19, x20, [sp, #-16]!
    mov     x19, sp             /* Save host SP in callee-saved X19 */

    /* 2. Switch to ELF user ABI stack */
    mov     sp, x1
    mov     x16, x0             /* Target entry point in X16 */

    /* 3. Setup initial register state per Linux System V ABI */
    mov     x0, xzr             /* X0 = 0 (no rtld_fini exit handler) */
    mov     x1, xzr
    mov     x2, xzr
    mov     x3, xzr
    mov     x4, xzr
    mov     x5, xzr
    mov     x6, xzr
    mov     x7, xzr
    mov     x8, xzr
    mov     x9, xzr
    mov     x10, xzr
    mov     x11, xzr
    mov     x12, xzr
    mov     x13, xzr
    mov     x14, xzr
    mov     x15, xzr
    mov     x17, xzr
    mov     x18, xzr
    mov     x20, xzr
    mov     x21, xzr
    mov     x22, xzr
    mov     x23, xzr
    mov     x24, xzr
    mov     x25, xzr
    mov     x26, xzr
    mov     x27, xzr
    mov     x28, xzr
    mov     x29, xzr            /* Frame pointer = 0 */
    adr     x30, 1f             /* Link register returns to 1f if user code returns */

    /* 4. Branch to entry point */
    br      x16

1:
    /* User program returned via ret:
     * X0 holds exit return code. Restore host stack and registers.
     */
    mov     sp, x19             /* Restore host SP */
    ldp     x19, x20, [sp], #16
    ldp     x21, x22, [sp], #16
    ldp     x23, x24, [sp], #16
    ldp     x25, x26, [sp], #16
    ldp     x27, x28, [sp], #16
    ldp     x29, x30, [sp], #16
    ret                         /* Return to host C caller with X0 */
