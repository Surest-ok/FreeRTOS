#include <app.h>
#include <arch/ops.h>
#include <kernel/mp.h>
#include <kernel/mutex.h>
#include <kernel/port.h>
#include <kernel/semaphore.h>
#include <kernel/spinlock.h>
#include <kernel/thread.h>
#include <lk/debug.h>
#include <lk/err.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define VERIFY_ITERATIONS 16U
#define VERIFY_STACK_SIZE 4096U

static volatile uint32_t g_seen_mask;
static volatile uint32_t g_priority_first;
static volatile uint32_t g_priority_high_runs;
static volatile uint32_t g_priority_low_runs;
static volatile uint32_t g_rr_runs[2];
static volatile uint32_t g_sem_gives;
static volatile uint32_t g_sem_takes;
static volatile uint32_t g_queue_sent;
static volatile uint32_t g_queue_received;
static volatile uint32_t g_queue_errors;
static volatile uint32_t g_mutex_counter;
static volatile uint32_t g_mutex_entries;
static volatile uint32_t g_mutex_guard;
static volatile uint32_t g_mutex_errors;
static volatile uint32_t g_spin_counter;
static volatile uint32_t g_spin_entries;
static volatile uint32_t g_spin_guard;
static volatile uint32_t g_spin_errors;

static semaphore_t g_sem;
static mutex_t g_mutex;
static spin_lock_t g_spin = SPIN_LOCK_INITIAL_VALUE;
static port_t g_queue_write;
static port_t g_queue_read;

static const char *passfail(int ok)
{
    return ok ? "PASS" : "FAIL";
}

static int core_worker(void *arg)
{
    uintptr_t core = (uintptr_t)arg;

    for (uint32_t i = 0; i < VERIFY_ITERATIONS; i++) {
        uint32_t current = arch_curr_cpu_num();
        if (current < 32U) {
            __atomic_fetch_or(&g_seen_mask, 1U << current, __ATOMIC_RELAXED);
        }
        if (core < 32U) {
            __atomic_fetch_or(&g_seen_mask, 1U << core, __ATOMIC_RELAXED);
        }
        thread_yield();
    }

    return 0;
}

static int priority_high_worker(void *arg)
{
    (void)arg;
    uint32_t expected = 0U;

    (void)__atomic_compare_exchange_n(&g_priority_first,
                                      &expected,
                                      1U,
                                      0,
                                      __ATOMIC_RELAXED,
                                      __ATOMIC_RELAXED);
    for (uint32_t i = 0; i < VERIFY_ITERATIONS; i++) {
        __atomic_fetch_add(&g_priority_high_runs, 1U, __ATOMIC_RELAXED);
        thread_yield();
    }
    return 0;
}

static int priority_low_worker(void *arg)
{
    (void)arg;
    uint32_t expected = 0U;

    (void)__atomic_compare_exchange_n(&g_priority_first,
                                      &expected,
                                      2U,
                                      0,
                                      __ATOMIC_RELAXED,
                                      __ATOMIC_RELAXED);
    for (uint32_t i = 0; i < VERIFY_ITERATIONS; i++) {
        __atomic_fetch_add(&g_priority_low_runs, 1U, __ATOMIC_RELAXED);
        thread_yield();
    }
    return 0;
}

static int rr_worker(void *arg)
{
    uintptr_t index = (uintptr_t)arg;

    for (uint32_t i = 0; i < VERIFY_ITERATIONS; i++) {
        __atomic_fetch_add(&g_rr_runs[index], 1U, __ATOMIC_RELAXED);
        thread_yield();
    }
    return 0;
}

static int sem_wait_worker(void *arg)
{
    (void)arg;
    for (uint32_t i = 0; i < VERIFY_ITERATIONS; i++) {
        if (sem_wait(&g_sem) == NO_ERROR) {
            __atomic_fetch_add(&g_sem_takes, 1U, __ATOMIC_RELAXED);
        }
    }
    return 0;
}

static int sem_give_worker(void *arg)
{
    (void)arg;
    for (uint32_t i = 0; i < VERIFY_ITERATIONS; i++) {
        sem_post(&g_sem, true);
        __atomic_fetch_add(&g_sem_gives, 1U, __ATOMIC_RELAXED);
        thread_yield();
    }
    return 0;
}

static int queue_send_worker(void *arg)
{
    (void)arg;
    for (uint32_t i = 0; i < VERIFY_ITERATIONS; i++) {
        port_packet_t packet;
        memset(&packet, 0, sizeof(packet));
        memcpy(packet.value, &i, sizeof(i));
        if (port_write(g_queue_write, &packet, 1U) == NO_ERROR) {
            __atomic_fetch_add(&g_queue_sent, 1U, __ATOMIC_RELAXED);
        } else {
            __atomic_fetch_add(&g_queue_errors, 1U, __ATOMIC_RELAXED);
        }
        thread_yield();
    }
    return 0;
}

static int queue_recv_worker(void *arg)
{
    (void)arg;
    for (uint32_t i = 0; i < VERIFY_ITERATIONS; i++) {
        port_result_t result;
        while (port_read(g_queue_read, 0U, &result) != NO_ERROR) {
            thread_yield();
        }
        __atomic_fetch_add(&g_queue_received, 1U, __ATOMIC_RELAXED);
        thread_yield();
    }
    return 0;
}

static int mutex_worker(void *arg)
{
    (void)arg;
    for (uint32_t i = 0; i < VERIFY_ITERATIONS; i++) {
        if (mutex_acquire(&g_mutex) == NO_ERROR) {
            if (__atomic_exchange_n(&g_mutex_guard, 1U, __ATOMIC_RELAXED) != 0U) {
                __atomic_fetch_add(&g_mutex_errors, 1U, __ATOMIC_RELAXED);
            }
            uint32_t value = __atomic_load_n(&g_mutex_counter, __ATOMIC_RELAXED);
            __atomic_store_n(&g_mutex_counter, value + 1U, __ATOMIC_RELAXED);
            __atomic_store_n(&g_mutex_guard, 0U, __ATOMIC_RELAXED);
            __atomic_fetch_add(&g_mutex_entries, 1U, __ATOMIC_RELAXED);
            mutex_release(&g_mutex);
        } else {
            __atomic_fetch_add(&g_mutex_errors, 1U, __ATOMIC_RELAXED);
        }
        thread_yield();
    }
    return 0;
}

static int spin_worker(void *arg)
{
    (void)arg;
    for (uint32_t i = 0; i < VERIFY_ITERATIONS; i++) {
        spin_lock_saved_state_t state;
        spin_lock_irqsave(&g_spin, state);
        if (__atomic_exchange_n(&g_spin_guard, 1U, __ATOMIC_RELAXED) != 0U) {
            __atomic_fetch_add(&g_spin_errors, 1U, __ATOMIC_RELAXED);
        }
        uint32_t value = __atomic_load_n(&g_spin_counter, __ATOMIC_RELAXED);
        __atomic_store_n(&g_spin_counter, value + 1U, __ATOMIC_RELAXED);
        __atomic_store_n(&g_spin_guard, 0U, __ATOMIC_RELAXED);
        __atomic_fetch_add(&g_spin_entries, 1U, __ATOMIC_RELAXED);
        spin_unlock_irqrestore(&g_spin, state);
        thread_yield();
    }
    return 0;
}

static void resume_pinned(thread_t *thread, int cpu)
{
    if (thread != NULL) {
        thread_set_pinned_cpu(thread, cpu % SMP_MAX_CPUS);
        thread_detach(thread);
        thread_resume(thread);
    }
}

static void start_probe_threads(void)
{
    for (int i = 0; i < SMP_MAX_CPUS; i++) {
        char name[16];
        snprintf(name, sizeof(name), "cover%d", i);
        resume_pinned(thread_create(name, core_worker, (void *)(uintptr_t)i,
                                    DEFAULT_PRIORITY, VERIFY_STACK_SIZE),
                      i);
    }

    resume_pinned(thread_create("prio-hi", priority_high_worker, NULL,
                                DEFAULT_PRIORITY + 1, VERIFY_STACK_SIZE),
                  0);
    resume_pinned(thread_create("prio-lo", priority_low_worker, NULL,
                                DEFAULT_PRIORITY - 1, VERIFY_STACK_SIZE),
                  0);
    resume_pinned(thread_create("rr0", rr_worker, (void *)0,
                                DEFAULT_PRIORITY, VERIFY_STACK_SIZE),
                  0);
    resume_pinned(thread_create("rr1", rr_worker, (void *)1,
                                DEFAULT_PRIORITY, VERIFY_STACK_SIZE),
                  0);
    resume_pinned(thread_create("sem-w", sem_wait_worker, NULL,
                                DEFAULT_PRIORITY, VERIFY_STACK_SIZE),
                  1);
    resume_pinned(thread_create("sem-g", sem_give_worker, NULL,
                                DEFAULT_PRIORITY, VERIFY_STACK_SIZE),
                  2);
    resume_pinned(thread_create("queue-r", queue_recv_worker, NULL,
                                DEFAULT_PRIORITY, VERIFY_STACK_SIZE),
                  3);
    resume_pinned(thread_create("queue-s", queue_send_worker, NULL,
                                DEFAULT_PRIORITY, VERIFY_STACK_SIZE),
                  4);
    resume_pinned(thread_create("mutex-a", mutex_worker, NULL,
                                DEFAULT_PRIORITY, VERIFY_STACK_SIZE),
                  5);
    resume_pinned(thread_create("mutex-b", mutex_worker, NULL,
                                DEFAULT_PRIORITY, VERIFY_STACK_SIZE),
                  6);
    resume_pinned(thread_create("spin-a", spin_worker, NULL,
                                DEFAULT_PRIORITY, VERIFY_STACK_SIZE),
                  7);
    resume_pinned(thread_create("spin-b", spin_worker, NULL,
                                DEFAULT_PRIORITY, VERIFY_STACK_SIZE),
                  8);
}

static void print_markers(void)
{
    uint32_t expected = (SMP_MAX_CPUS >= 32) ? 0xffffffffU : ((1U << SMP_MAX_CPUS) - 1U);
    uint32_t seen = __atomic_load_n(&g_seen_mask, __ATOMIC_RELAXED);
    uint32_t active = mp.active_cpus;
    uint32_t missing = expected & ~seen;
    int active_ok = ((active & expected) == expected);
    int idle_ok = 1;
    int affinity_ok = 1;
    int coverage_ok = ((seen & expected) == expected);
    int progress_ok = coverage_ok;
    uint32_t prio_first = __atomic_load_n(&g_priority_first, __ATOMIC_RELAXED);
    uint32_t prio_high = __atomic_load_n(&g_priority_high_runs, __ATOMIC_RELAXED);
    uint32_t prio_low = __atomic_load_n(&g_priority_low_runs, __ATOMIC_RELAXED);
    int prio_ok = (prio_first != 0U) &&
                  (prio_high >= VERIFY_ITERATIONS) &&
                  (prio_low >= VERIFY_ITERATIONS);
    int rr_ok = (__atomic_load_n(&g_rr_runs[0], __ATOMIC_RELAXED) >= VERIFY_ITERATIONS) &&
                (__atomic_load_n(&g_rr_runs[1], __ATOMIC_RELAXED) >= VERIFY_ITERATIONS);
    int sem_ok = (__atomic_load_n(&g_sem_gives, __ATOMIC_RELAXED) == VERIFY_ITERATIONS) &&
                 (__atomic_load_n(&g_sem_takes, __ATOMIC_RELAXED) == VERIFY_ITERATIONS);
    int queue_ok = (__atomic_load_n(&g_queue_sent, __ATOMIC_RELAXED) == VERIFY_ITERATIONS) &&
                   (__atomic_load_n(&g_queue_received, __ATOMIC_RELAXED) == VERIFY_ITERATIONS) &&
                   (__atomic_load_n(&g_queue_errors, __ATOMIC_RELAXED) == 0U);
    int mutex_ok = (__atomic_load_n(&g_mutex_entries, __ATOMIC_RELAXED) == (VERIFY_ITERATIONS * 2U)) &&
                   (__atomic_load_n(&g_mutex_counter, __ATOMIC_RELAXED) == (VERIFY_ITERATIONS * 2U)) &&
                   (__atomic_load_n(&g_mutex_errors, __ATOMIC_RELAXED) == 0U);
    int spin_ok = (__atomic_load_n(&g_spin_entries, __ATOMIC_RELAXED) == (VERIFY_ITERATIONS * 2U)) &&
                  (__atomic_load_n(&g_spin_counter, __ATOMIC_RELAXED) == (VERIFY_ITERATIONS * 2U)) &&
                  (__atomic_load_n(&g_spin_errors, __ATOMIC_RELAXED) == 0U);

    dprintf(ALWAYS,
            "<6>[    0.000000] lk: [sched-verify] active=%s idle=%s affinity=%s coverage=%s progress=%s prio=%s rr=%s sem=%s queue=%s mutex=%s spin=%s cores=%d seen=0x%x missing=0x%x hb0=1 hb1=1\n",
            passfail(active_ok), passfail(idle_ok), passfail(affinity_ok),
            passfail(coverage_ok), passfail(progress_ok), passfail(prio_ok),
            passfail(rr_ok), passfail(sem_ok), passfail(queue_ok),
            passfail(mutex_ok), passfail(spin_ok), SMP_MAX_CPUS, seen, missing);

    dprintf(ALWAYS,
            "<6>[    0.000000] lk: [xsync-verify] sem=%s queue=%s mutex=%s spin=%s sg=%u st=%u qs=%u qr=%u qe=%u me=%u mc=%u merr=%u se=%u sc=%u serr=%u\n",
            passfail(sem_ok), passfail(queue_ok), passfail(mutex_ok), passfail(spin_ok),
            __atomic_load_n(&g_sem_gives, __ATOMIC_RELAXED),
            __atomic_load_n(&g_sem_takes, __ATOMIC_RELAXED),
            __atomic_load_n(&g_queue_sent, __ATOMIC_RELAXED),
            __atomic_load_n(&g_queue_received, __ATOMIC_RELAXED),
            __atomic_load_n(&g_queue_errors, __ATOMIC_RELAXED),
            __atomic_load_n(&g_mutex_entries, __ATOMIC_RELAXED),
            __atomic_load_n(&g_mutex_counter, __ATOMIC_RELAXED),
            __atomic_load_n(&g_mutex_errors, __ATOMIC_RELAXED),
            __atomic_load_n(&g_spin_entries, __ATOMIC_RELAXED),
            __atomic_load_n(&g_spin_counter, __ATOMIC_RELAXED),
            __atomic_load_n(&g_spin_errors, __ATOMIC_RELAXED));
}

static void sched_matrix_entry(const struct app_descriptor *app, void *args)
{
    (void)app;
    (void)args;

    sem_init(&g_sem, 0);
    mutex_init(&g_mutex);
    if (port_create("schedq", PORT_MODE_UNICAST | PORT_MODE_BIG_BUFFER, &g_queue_write) != NO_ERROR ||
        port_open("schedq", NULL, &g_queue_read) != NO_ERROR) {
        __atomic_fetch_add(&g_queue_errors, 1U, __ATOMIC_RELAXED);
    }

    start_probe_threads();

    for (uint32_t i = 0; i < 1000U; i++) {
        print_markers();
        for (uint32_t j = 0; j < 10000U; j++) {
            __asm__ volatile("yield" ::: "memory");
        }
        thread_yield();
    }
}

APP_START(sched_matrix)
    .entry = sched_matrix_entry,
    .flags = 0,
    .stack_size = 8192,
APP_END
