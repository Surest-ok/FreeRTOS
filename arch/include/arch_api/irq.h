#ifndef ARCH_IRQ_H
#define ARCH_IRQ_H

#ifndef TX_PORT_H
#include <stdint.h>
#endif
#include "arch_api/barrier.h"

typedef uint64_t arch_irq_flags_t;
typedef int (*arch_irq_handler_t)(uint32_t vector, void *arg);

typedef struct arch_irq_stats {
    volatile uint64_t entries;
    volatile uint64_t exits;
    volatile uint64_t handled;
    volatile uint64_t unhandled;
    volatile uint32_t last_vector;
    volatile uint32_t last_cpu;
} arch_irq_stats_t;

static inline arch_irq_flags_t arch_irq_save(void)
{
    arch_irq_flags_t flags;
    __asm__ volatile("mrs %0, daif" : "=r"(flags));
    __asm__ volatile("msr daifset, #2" ::: "memory");
    arch_isb();
    return flags;
}

static inline arch_irq_flags_t arch_irq_save_all(void)
{
    arch_irq_flags_t flags;
    __asm__ volatile("mrs %0, daif" : "=r"(flags));
    __asm__ volatile("msr daifset, #3" ::: "memory");
    arch_isb();
    return flags;
}

static inline void arch_irq_restore(arch_irq_flags_t flags)
{
    if ((flags & (1UL << 7)) == 0U) {
        __asm__ volatile("msr daifclr, #2" ::: "memory");
    }
    arch_isb();
}

static inline void arch_irq_restore_all(arch_irq_flags_t flags)
{
    __asm__ volatile("msr daif, %0" :: "r"(flags) : "memory");
    arch_isb();
}

static inline void arch_irq_disable(void)
{
    __asm__ volatile("msr daifset, #2" ::: "memory");
    arch_isb();
}

static inline void arch_irq_enable(void)
{
    __asm__ volatile("msr daifclr, #2" ::: "memory");
    arch_isb();
}

int arch_irq_register(uint32_t vector, arch_irq_handler_t handler, void *arg);
int arch_irq_dispatch(uint32_t vector);
void arch_irq_note_enter(uint32_t vector, uint32_t cpu);
void arch_irq_note_exit(uint32_t vector, uint32_t cpu, int handled);
const arch_irq_stats_t *arch_irq_get_stats(void);

#endif /* ARCH_IRQ_H */
