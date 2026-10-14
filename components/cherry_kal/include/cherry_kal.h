/*
 * CherryKAL - Kernel Abstraction Layer (OSAL)
 *
 * Copyright (c) 2026 ForgeStitch
 * SPDX-License-Identifier: MIT
 *
 * Standard Linux-style Abstraction for RTOS / Baremetal / SMP
 */

#ifndef _CHERRY_KAL_H_
#define _CHERRY_KAL_H_

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define KAL_WAIT_FOREVER    0xFFFFFFFFU
#define KAL_NO_WAIT         0x00000000U

#define KAL_OK              0
#define KAL_ERROR          -1
#define KAL_TIMEOUT        -2
#define KAL_BUSY           -3
#define KAL_NOMEM          -4
#define KAL_INVAL          -5

/* =========================================================================
 * 1. Memory Management
 * ========================================================================= */
void *kal_malloc(size_t size);
void  kal_free(void *ptr);
void *kal_calloc(size_t nmemb, size_t size);
void *kal_realloc(void *ptr, size_t size);

/* =========================================================================
 * 2. Mutex Synchronization (Linux mutex_lock / mutex_unlock semantics)
 * ========================================================================= */
typedef struct kal_mutex {
    void *handle;
} kal_mutex_t;

int kal_mutex_init(kal_mutex_t *mutex);
int kal_mutex_lock(kal_mutex_t *mutex);
int kal_mutex_trylock(kal_mutex_t *mutex);
int kal_mutex_unlock(kal_mutex_t *mutex);
int kal_mutex_destroy(kal_mutex_t *mutex);

/* =========================================================================
 * 3. Spinlock (SMP safe, with IRQ save/restore)
 * ========================================================================= */
typedef struct kal_spinlock {
    volatile uint32_t lock;
    uint32_t          owner_core;
    uint32_t          nesting;
} kal_spinlock_t;

int  kal_spin_lock_init(kal_spinlock_t *lock);
void kal_spin_lock(kal_spinlock_t *lock);
void kal_spin_unlock(kal_spinlock_t *lock);
unsigned long kal_spin_lock_irqsave(kal_spinlock_t *lock);
void kal_spin_unlock_irqrestore(kal_spinlock_t *lock, unsigned long flags);

/* =========================================================================
 * 4. Semaphore Synchronization
 * ========================================================================= */
typedef struct kal_sem {
    void *handle;
} kal_sem_t;

int kal_sem_init(kal_sem_t *sem, unsigned int initial_count, unsigned int max_count);
int kal_sem_wait(kal_sem_t *sem, uint32_t timeout_ms);
int kal_sem_post(kal_sem_t *sem);
int kal_sem_destroy(kal_sem_t *sem);

/* =========================================================================
 * 5. Thread / Task Management
 * ========================================================================= */
typedef void *kal_thread_t;
typedef void (*kal_thread_func_t)(void *arg);

kal_thread_t kal_thread_create(const char *name,
                               kal_thread_func_t func,
                               void *arg,
                               size_t stack_size,
                               int priority);

int  kal_thread_set_affinity(kal_thread_t thread, uint32_t core_mask);
void kal_thread_sleep_ms(uint32_t ms);
void kal_thread_yield(void);
void kal_thread_exit(void);
kal_thread_t kal_thread_self(void);
uint32_t kal_get_core_id(void);

/* =========================================================================
 * 6. Time and Ticks
 * ========================================================================= */
uint64_t kal_get_time_ms(void);
uint64_t kal_get_time_us(void);
uint32_t kal_get_tick(void);
uint32_t kal_tick_rate_hz(void);

/* =========================================================================
 * 7. Critical Sections & Memory Barriers
 * ========================================================================= */
void kal_enter_critical(void);
void kal_exit_critical(void);

static inline void kal_mb(void)
{
    __asm__ volatile("dmb sy" ::: "memory");
}

static inline void kal_rmb(void)
{
    __asm__ volatile("dmb ld" ::: "memory");
}

static inline void kal_wmb(void)
{
    __asm__ volatile("dmb st" ::: "memory");
}

/* =========================================================================
 * 8. Formatted Console Logging Output
 * ========================================================================= */
void kal_printf(const char *fmt, ...);

#ifdef __cplusplus
}
#endif

#endif /* _CHERRY_KAL_H_ */
