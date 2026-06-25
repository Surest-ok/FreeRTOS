#ifndef ARCH_ATOMIC_H
#define ARCH_ATOMIC_H

#include <stdint.h>

typedef struct {
    volatile int value;
} arch_atomic_t;

static inline int arch_atomic_read(const arch_atomic_t *v)
{
    return __atomic_load_n(&v->value, __ATOMIC_RELAXED);
}

static inline void arch_atomic_set(arch_atomic_t *v, int value)
{
    __atomic_store_n(&v->value, value, __ATOMIC_RELAXED);
}

static inline int arch_atomic_add_return(arch_atomic_t *v, int value)
{
    return __atomic_add_fetch(&v->value, value, __ATOMIC_ACQ_REL);
}

static inline int arch_atomic_sub_return(arch_atomic_t *v, int value)
{
    return __atomic_sub_fetch(&v->value, value, __ATOMIC_ACQ_REL);
}

static inline int arch_atomic_cmpxchg(arch_atomic_t *v, int old_value, int new_value)
{
    (void)__atomic_compare_exchange_n(&v->value,
                                      &old_value,
                                      new_value,
                                      0,
                                      __ATOMIC_ACQ_REL,
                                      __ATOMIC_ACQUIRE);
    return old_value;
}

static inline uint32_t arch_xchg32(volatile uint32_t *ptr, uint32_t value)
{
    return __atomic_exchange_n(ptr, value, __ATOMIC_ACQ_REL);
}

#endif /* ARCH_ATOMIC_H */
