#ifndef ARCH_SPINLOCK_H
#define ARCH_SPINLOCK_H

#include <stdint.h>
#include "arch_api/arch.h"
#include "arch_api/irq.h"

typedef struct {
    volatile uint32_t value;
} arch_raw_spinlock_t;

typedef struct {
    volatile uint32_t owner;
    volatile uint32_t count;
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

static inline void arch_recursive_spin_lock(arch_recursive_spinlock_t *lock)
{
    uint32_t owner = arch_curr_cpu_id() + 1U;

    if (__atomic_load_n(&lock->owner, __ATOMIC_ACQUIRE) == owner) {
        __atomic_fetch_add(&lock->count, 1U, __ATOMIC_RELAXED);
        return;
    }

    for (;;) {
        while (__atomic_load_n(&lock->owner, __ATOMIC_ACQUIRE) != 0U) {
            __asm__ volatile("yield");
        }

        uint32_t expected = 0U;
        if (__atomic_compare_exchange_n(&lock->owner,
                                        &expected,
                                        owner,
                                        0,
                                        __ATOMIC_ACQ_REL,
                                        __ATOMIC_RELAXED)) {
            __atomic_store_n(&lock->count, 1U, __ATOMIC_RELEASE);
            return;
        }
    }
}

static inline void arch_recursive_spin_unlock(arch_recursive_spinlock_t *lock)
{
    uint32_t count = __atomic_sub_fetch(&lock->count, 1U, __ATOMIC_RELEASE);
    if (count == 0U) {
        __atomic_store_n(&lock->owner, 0U, __ATOMIC_RELEASE);
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
