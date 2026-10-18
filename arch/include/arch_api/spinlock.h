#ifndef ARCH_SPINLOCK_H
#define ARCH_SPINLOCK_H

#include <stdint.h>
#include "arch_api/arch.h"
#include "arch_api/irq.h"

typedef struct {
    volatile uint32_t value;
} arch_raw_spinlock_t;

typedef union {
    struct {
        volatile uint32_t owner;
        volatile uint32_t count;
    };
    volatile uint64_t raw;
} arch_recursive_spinlock_t;

static inline void arch_raw_spin_lock_init(arch_raw_spinlock_t *lock)
{
    __atomic_store_n(&lock->value, 0U, __ATOMIC_RELAXED);
}

static inline void arch_raw_spin_lock(arch_raw_spinlock_t *lock)
{
    for (;;) {
        uint32_t expected = 0U;
        if (__atomic_compare_exchange_n(&lock->value,
                                        &expected,
                                        1U,
                                        0,
                                        __ATOMIC_ACQUIRE,
                                        __ATOMIC_RELAXED)) {
            return;
        }
        __asm__ volatile("yield");
    }
}

static inline int arch_raw_spin_trylock(arch_raw_spinlock_t *lock)
{
    uint32_t expected = 0U;
    return __atomic_compare_exchange_n(&lock->value,
                                       &expected,
                                       1U,
                                       0,
                                       __ATOMIC_ACQUIRE,
                                       __ATOMIC_RELAXED);
}

static inline void arch_raw_spin_unlock(arch_raw_spinlock_t *lock)
{
    __atomic_store_n(&lock->value, 0U, __ATOMIC_RELEASE);
}

static inline void arch_recursive_spin_lock_init(arch_recursive_spinlock_t *lock)
{
    __atomic_store_n(&lock->raw, 0ULL, __ATOMIC_RELAXED);
}

static inline void arch_recursive_spin_lock(arch_recursive_spinlock_t *lock)
{
    uint32_t cpu = arch_curr_cpu_id() + 1U;
    uint64_t val = __atomic_load_n(&lock->raw, __ATOMIC_ACQUIRE);

    if ((uint32_t)val == cpu) {
        lock->count++;
        return;
    }

    uint64_t desired = (1ULL << 32) | (uint64_t)cpu;
    for (;;) {
        while (__atomic_load_n(&lock->raw, __ATOMIC_ACQUIRE) != 0ULL) {
            __asm__ volatile("yield");
        }

        uint64_t expected = 0ULL;
        if (__atomic_compare_exchange_n(&lock->raw,
                                        &expected,
                                        desired,
                                        0,
                                        __ATOMIC_ACQ_REL,
                                        __ATOMIC_RELAXED)) {
            return;
        }
    }
}

static inline int arch_recursive_spin_trylock(arch_recursive_spinlock_t *lock)
{
    uint32_t cpu = arch_curr_cpu_id() + 1U;
    uint64_t val = __atomic_load_n(&lock->raw, __ATOMIC_ACQUIRE);

    if ((uint32_t)val == cpu) {
        lock->count++;
        return 1;
    }

    uint64_t expected = 0ULL;
    uint64_t desired = (1ULL << 32) | (uint64_t)cpu;
    return __atomic_compare_exchange_n(&lock->raw,
                                       &expected,
                                       desired,
                                       0,
                                       __ATOMIC_ACQ_REL,
                                       __ATOMIC_RELAXED);
}

static inline void arch_recursive_spin_unlock(arch_recursive_spinlock_t *lock)
{
    if (lock->count > 1U) {
        lock->count--;
    } else {
        __atomic_store_n(&lock->raw, 0ULL, __ATOMIC_RELEASE);
    }
}

static inline arch_irq_flags_t arch_raw_spin_lock_irqsave(arch_raw_spinlock_t *lock)
{
    arch_irq_flags_t flags = arch_irq_save();
    arch_raw_spin_lock(lock);
    return flags;
}

static inline void arch_raw_spin_unlock_irqrestore(arch_raw_spinlock_t *lock,
                                                     arch_irq_flags_t flags)
{
    arch_raw_spin_unlock(lock);
    arch_irq_restore(flags);
}

#endif /* ARCH_SPINLOCK_H */
