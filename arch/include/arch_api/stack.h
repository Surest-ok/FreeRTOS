#ifndef ARCH_STACK_H
#define ARCH_STACK_H

static inline unsigned long arch_current_stack_pointer(void)
{
    unsigned long sp;
    __asm__ volatile("mov %0, sp" : "=r"(sp));
    return sp;
}

static inline unsigned long arch_stack_align_down(unsigned long sp)
{
    return sp & ~0xfUL;
}

#endif /* ARCH_STACK_H */
