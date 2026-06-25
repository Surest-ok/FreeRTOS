#ifndef ARCH_EXCEPTION_H
#define ARCH_EXCEPTION_H

#include <stdint.h>

typedef struct arch_exception_frame {
    uint64_t regs[31];
    uint64_t elr;
    uint64_t spsr;
    uint64_t esr;
    uint64_t far;
} arch_exception_frame_t;

typedef struct arch_exception_stats {
    volatile uint64_t count;
    volatile uint64_t last_elr;
    volatile uint64_t last_spsr;
    volatile uint64_t last_esr;
    volatile uint64_t last_far;
} arch_exception_stats_t;

void arch_exception_record(const arch_exception_frame_t *frame);
void arch_exception_dispatch(arch_exception_frame_t *frame);
const arch_exception_stats_t *arch_exception_get_stats(void);

#endif /* ARCH_EXCEPTION_H */
