/*
 * Copyright (c) 2014 Travis Geiselbrecht
 *
 * Use of this source code is governed by a MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT
 */
#include <lk/asm.h>
#include <arch/asm_macros.h>

// stay in sync with arm64/thread.c arm64_context_switch()

/* void arm64_context_switch(vaddr_t *old_sp, vaddr_t new_sp); */
FUNCTION(arm64_context_switch)
    /* save old frame */
    push x29, lr
    push x27, x28
    push x25, x26
    push x23, x24
    push x21, x22
    push x19, x20
    // skip x18, it is our per cpu pointer
    mrs  x16, tpidr_el0
    mrs  x17, tpidrro_el0
    push x16, x17

    /* save old sp */
    mov  x15, sp
    str  x15, [x0]

    /* load new sp */
    mov  sp, x1

    /* restore new frame */
    pop  x16, x17
    msr  tpidr_el0, x16
    msr  tpidrro_el0, x17
    pop  x19, x20
    pop  x21, x22
    pop  x23, x24
    pop  x25, x26
    pop  x27, x28
    pop  x29, lr

    ret

    /* drop from whatever EL we may already be in to EL1.
     * carefully avoids using x0-x3 since this is called from start.S
     * which is trying to preserve them.
     */
FUNCTION(arm64_elX_to_el1)
    mrs x4, CurrentEL

    cmp x4, #(0b01 << 2)
    bne .notEL1
    /* Already in EL1 */
    ret

.notEL1:
    cmp x4, #(0b10 << 2)
    beq .inEL2

    /* set EL2 to 64bit */
    mrs x4, scr_el3
    orr x4, x4, #(1<<10)
    msr scr_el3, x4

    /* prep this mode's ELR and SPSR to drop into EL2 first */
    adr x4, .inEL2
    msr elr_el3, x4

    mov x4, #((0b1111 << 6) | (0b1001)) /* EL2h runlevel */
    msr spsr_el3, x4
    isb
    eret

.inEL2:
    /* Enhanced EL2 initialization for QEMU virt */

    /* Initialize HCR_EL2 to zero first */
    mov x4, #0
    msr hcr_el2, x4
    isb

    /* Initialize vcpu associated registers */
    mrs x4, midr_el1
    mrs x5, mpidr_el1
    msr vpidr_el2, x4
    msr vmpidr_el2, x5

    /* disable EL2 coprocessor traps - allow FP/NEON access */
    ldr x4, =0x300000
    msr cptr_el2, x4

    /* Zero stage 2 translation */
    msr vttbr_el2, xzr
    dsb sy
    isb

    /* set EL1 to 64bit (HCR_EL2.RW = 1) and disable EL2 instruction traps */
    mrs x4, hcr_el2
    mov x5, #(1<<31)  /* HCR_EL2.RW = 1 for AArch64 */
    orr x5, x5, x4
    msr hcr_el2, x5
    isb

    /* Clear SCTLR_EL2 */
    mov x4, #0
    msr sctlr_el2, x4
    isb

    /* Set up counter access for EL1 */
    mov x4, #3
    msr cnthctl_el2, x4
    isb

    /* Enable GICv3 system register interface for EL1 */
    /* Set ICC_SRE_EL2: SRE=1, Enable=1, DIB=1, DFB=1 */
    mov x4, #0xf
    msr S3_4_C12_C9_5, x4
    isb

    /* Enable FP/NEON access for EL1 */
    msr cpacr_el1, xzr
    mov x4, #((0b11 << 20) | (0b11 << 22))
    msr cpacr_el1, x4
    isb

    /* set up the EL1 bounce interrupt */
    mov x4, sp
    msr sp_el1, x4

    /* prep this mode's ELR and SPSR to drop into EL1 */
    adr x4, .Ltarget
    msr elr_el2, x4
    mov x4, #0x3C5  /* EL1h with interrupts masked (DAIF=1111, M=0101) */
    msr spsr_el2, x4

    dsb nsh
    isb
    eret

.Ltarget:
    ret
