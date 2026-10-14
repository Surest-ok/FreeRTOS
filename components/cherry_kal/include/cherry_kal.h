/*
 * CherryKAL - Kernel Abstraction Layer (OSAL)
 *
 * Copyright (c) 2026 ForgeStitch
 * SPDX-License-Identifier: MIT
 *
 * 100% Linux Kernel API Standard (No Custom Prefix)
 * Provides standard Linux mutex, spinlock, semaphore, kmalloc, kthread,
 * printk, jiffies, atomics, and memory barriers across RTOS/baremetal platforms.
 */

#ifndef _CHERRY_KAL_H_
#define _CHERRY_KAL_H_

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =========================================================================
 * 1. Linux Standard Log Levels & Printk
 * ========================================================================= */
#define KERN_EMERG   "<0>"  /* system is unusable */
#define KERN_ALERT   "<1>"  /* action must be taken immediately */
#define KERN_CRIT    "<2>"  /* critical conditions */
#define KERN_ERR     "<3>"  /* error conditions */
#define KERN_WARNING "<4>"  /* warning conditions */
#define KERN_NOTICE  "<5>"  /* normal but significant condition */
#define KERN_INFO    "<6>"  /* informational */
#define KERN_DEBUG   "<7>"  /* debug-level messages */

int printk(const char *fmt, ...);

#define pr_emerg(fmt, ...)   printk(KERN_EMERG fmt, ##__VA_ARGS__)
#define pr_alert(fmt, ...)   printk(KERN_ALERT fmt, ##__VA_ARGS__)
#define pr_crit(fmt, ...)    printk(KERN_CRIT fmt, ##__VA_ARGS__)
#define pr_err(fmt, ...)     printk(KERN_ERR fmt, ##__VA_ARGS__)
#define pr_warn(fmt, ...)    printk(KERN_WARNING fmt, ##__VA_ARGS__)
#define pr_warning           pr_warn
#define pr_notice(fmt, ...)  printk(KERN_NOTICE fmt, ##__VA_ARGS__)
#define pr_info(fmt, ...)    printk(KERN_INFO fmt, ##__VA_ARGS__)
#define pr_debug(fmt, ...)   printk(KERN_DEBUG fmt, ##__VA_ARGS__)

/* =========================================================================
 * 2. Memory Management (kmalloc, kfree, kzalloc, krealloc)
 * ========================================================================= */
typedef unsigned int gfp_t;
#define GFP_KERNEL 0x00U
#define GFP_ATOMIC 0x01U
#define GFP_NOWAIT 0x02U
#define __GFP_ZERO 0x04U

void *kmalloc(size_t size, gfp_t flags);
void  kfree(const void *ptr);
void *kzalloc(size_t size, gfp_t flags);
void *kcalloc(size_t nmemb, size_t size, gfp_t flags);
void *krealloc(const void *ptr, size_t size, gfp_t flags);

/* =========================================================================
 * 3. Mutex (struct mutex, mutex_init, mutex_lock, mutex_unlock)
 * ========================================================================= */
struct mutex {
    void *handle;
};

void mutex_init(struct mutex *lock);
void mutex_lock(struct mutex *lock);
int  mutex_trylock(struct mutex *lock);
void mutex_unlock(struct mutex *lock);
void mutex_destroy(struct mutex *lock);

/* =========================================================================
 * 4. Spinlock (spinlock_t, spin_lock, spin_unlock, spin_lock_irqsave)
 * ========================================================================= */
typedef struct spinlock {
    volatile uint32_t lock;
    uint32_t          owner_core;
    uint32_t          nesting;
} spinlock_t;

#define SPIN_LOCK_UNLOCKED (spinlock_t){ 0, 0xFFFFFFFFU, 0 }

void spin_lock_init(spinlock_t *lock);
void spin_lock(spinlock_t *lock);
void spin_unlock(spinlock_t *lock);
unsigned long _spin_lock_irqsave(spinlock_t *lock);
void spin_unlock_irqrestore(spinlock_t *lock, unsigned long flags);

#define spin_lock_irqsave(lock, flags) \
    do { (flags) = _spin_lock_irqsave(lock); } while (0)

/* =========================================================================
 * 5. Semaphore (struct semaphore, sema_init, down, up)
 * ========================================================================= */
struct semaphore {
    void *handle;
};

void sema_init(struct semaphore *sem, int val);
void down(struct semaphore *sem);
int  down_interruptible(struct semaphore *sem);
int  down_trylock(struct semaphore *sem);
int  down_timeout(struct semaphore *sem, long timeout_ms);
void up(struct semaphore *sem);
void sema_destroy(struct semaphore *sem);

/* =========================================================================
 * 6. Threads and Scheduling (kthread_create, kthread_run, schedule, msleep)
 * ========================================================================= */
struct task_struct {
    void *handle;
};

typedef int (*kthread_fn_t)(void *data);

struct task_struct *kthread_create(kthread_fn_t threadfn, void *data, const char *name);
struct task_struct *kthread_run(kthread_fn_t threadfn, void *data, const char *name);
int  kthread_bind(struct task_struct *task, unsigned int cpu);
void schedule(void);
void msleep(unsigned int msecs);
void usleep_range(unsigned long min, unsigned long max);
unsigned int smp_processor_id(void);

/* =========================================================================
 * 7. Time and Jiffies
 * ========================================================================= */
#define HZ 1000U
extern volatile unsigned long jiffies;

unsigned long msecs_to_jiffies(const unsigned int m);
unsigned int  jiffies_to_msecs(const unsigned long j);
uint64_t ktime_get_ns(void);
uint64_t ktime_get_ms(void);
uint64_t ktime_get_real_ns(void);

/* =========================================================================
 * 8. Memory Barriers & Atomics
 * ========================================================================= */
#define mb()     __asm__ volatile("dmb sy" ::: "memory")
#define rmb()    __asm__ volatile("dmb ld" ::: "memory")
#define wmb()    __asm__ volatile("dmb st" ::: "memory")
#define smp_mb()  mb()
#define smp_rmb() rmb()
#define smp_wmb() wmb()

typedef struct {
    volatile int counter;
} atomic_t;

#define ATOMIC_INIT(i) { (i) }

static inline int  atomic_read(const atomic_t *v) { return __atomic_load_n(&v->counter, __ATOMIC_SEQ_CST); }
static inline void atomic_set(atomic_t *v, int i) { __atomic_store_n(&v->counter, i, __ATOMIC_SEQ_CST); }
static inline void atomic_add(int i, atomic_t *v) { __atomic_add_fetch(&v->counter, i, __ATOMIC_SEQ_CST); }
static inline void atomic_sub(int i, atomic_t *v) { __atomic_sub_fetch(&v->counter, i, __ATOMIC_SEQ_CST); }
static inline void atomic_inc(atomic_t *v)        { __atomic_add_fetch(&v->counter, 1, __ATOMIC_SEQ_CST); }
static inline void atomic_dec(atomic_t *v)        { __atomic_sub_fetch(&v->counter, 1, __ATOMIC_SEQ_CST); }
static inline int  atomic_inc_return(atomic_t *v) { return __atomic_add_fetch(&v->counter, 1, __ATOMIC_SEQ_CST); }
static inline int  atomic_dec_return(atomic_t *v) { return __atomic_sub_fetch(&v->counter, 1, __ATOMIC_SEQ_CST); }

/* =========================================================================
 * 9. Kernel Utility Macros (container_of)
 * ========================================================================= */
#ifndef offsetof
#define offsetof(TYPE, MEMBER) ((size_t)&((TYPE *)0)->MEMBER)
#endif

#ifndef container_of
#define container_of(ptr, type, member) ({                      \
    const typeof( ((type *)0)->member ) *__mptr = (ptr);    \
    (type *)( (char *)__mptr - offsetof(type,member) );})
#endif

/* =========================================================================
 * 10. Backward Compatibility Aliases for kal_*
 * ========================================================================= */
#define kal_malloc(size)                kmalloc(size, GFP_KERNEL)
#define kal_free(ptr)                   kfree(ptr)
#define kal_calloc(n, s)                kcalloc(n, s, GFP_KERNEL)
#define kal_realloc(p, s)               krealloc(p, s, GFP_KERNEL)
#define kal_mutex_t                     struct mutex
#define kal_mutex_init(m)               mutex_init(m)
#define kal_mutex_lock(m)               mutex_lock(m)
#define kal_mutex_trylock(m)            mutex_trylock(m)
#define kal_mutex_unlock(m)             mutex_unlock(m)
#define kal_mutex_destroy(m)            mutex_destroy(m)
#define kal_spinlock_t                  spinlock_t
#define kal_spin_lock_init(l)           spin_lock_init(l)
#define kal_spin_lock(l)                spin_lock(l)
#define kal_spin_unlock(l)              spin_unlock(l)
#define kal_spin_lock_irqsave(l)        _spin_lock_irqsave(l)
#define kal_spin_unlock_irqrestore(l,f) spin_unlock_irqrestore(l, f)
#define kal_sem_t                       struct semaphore
#define kal_sem_init(s, init, max)      sema_init(s, init)
#define kal_sem_wait(s, t)              down_timeout(s, t)
#define kal_sem_post(s)                 up(s)
#define kal_sem_destroy(s)              sema_destroy(s)
#define kal_thread_t                    struct task_struct *
#define kal_thread_create(n, f, a, s, p) kthread_run((kthread_fn_t)(f), a, n)
#define kal_thread_set_affinity(t, m)   kthread_bind(t, (m == 2 ? 1 : 0))
#define kal_thread_sleep_ms(ms)         msleep(ms)
#define kal_thread_yield()              schedule()
#define kal_get_core_id()               smp_processor_id()
#define kal_get_time_ms()               ktime_get_ms()
#define kal_get_time_us()               (ktime_get_ns() / 1000ULL)
#define kal_get_tick()                  ((uint32_t)jiffies)
#define kal_tick_rate_hz()              HZ
#define kal_printf(fmt, ...)            printk(fmt, ##__VA_ARGS__)

#define KAL_OK     0
#define KAL_ERROR -1

#ifdef __cplusplus
}
#endif

#endif /* _CHERRY_KAL_H_ */
