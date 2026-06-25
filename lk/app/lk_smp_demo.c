/*
 * LK SMP Demo - Shell Command
 * Demonstrates 4-core SMP with threading and synchronization
 */
#include <lk/debug.h>
#include <lk/console_cmd.h>
#include <lk/init.h>
#include <kernel/thread.h>
#include <kernel/semaphore.h>
#include <kernel/mutex.h>
#include <kernel/event.h>
#include <kernel/mp.h>
#include <platform.h>
#include <string.h>

#define WORKER_THREAD_PRIORITY DEFAULT_PRIORITY
#define COORDINATOR_PRIORITY   (DEFAULT_PRIORITY + 1)
#define WORKER_STACK_SIZE      8192

/* Synchronization primitives */
static semaphore_t work_sem;      /* Signal work availability */
static mutex_t print_mutex;        /* Protect console output */
static event_t phase_event;        /* Phase completion sync */

/* Shared state */
static volatile uint32_t shared_counter = 0;
static uint32_t core_counters[SMP_MAX_CPUS] = {0};

/* Thread handles */
static thread_t *core_threads[SMP_MAX_CPUS];
static thread_t *coordinator_thread;

static bool demo_running = true;

/* Worker thread with synchronization */
static int worker_thread_sync(void *arg) {
    int core_id = (int)(uintptr_t)arg;

    dprintf(INFO, "[Core %d] Synchronized worker started on CPU %u\n", core_id, arch_curr_cpu_num());

    while (demo_running) {
        /* Wait for work signal from coordinator */
        sem_wait(&work_sem);

        if (!demo_running) break;

        mutex_acquire(&print_mutex);
        dprintf(ALWAYS, "[Core %d] Processing task on CPU %u\n", core_id, arch_curr_cpu_num());
        mutex_release(&print_mutex);

        /* Simulate work */
        for (volatile int j = 0; j < 5000000; j++) {
            shared_counter++;
        }
        core_counters[core_id]++;

        /* Signal completion */
        event_signal(&phase_event, false);
    }

    dprintf(INFO, "[Core %d] Synchronized worker finished\n", core_id);
    return 0;
}

/* Simple independent worker - just counts and prints */
static int worker_thread(void *arg) {
    int core_id = (int)(uintptr_t)arg;

    dprintf(ALWAYS, "[Core %d] Worker thread started on CPU %u\n", core_id, arch_curr_cpu_num());

    /* Just run independently - no coordination */
    for (int i = 0; i < 10; i++) {
        dprintf(ALWAYS, "[Core %d] Iteration %d running on CPU %u\n",
                core_id, i, arch_curr_cpu_num());

        /* Busy wait to simulate work */
        for (volatile int j = 0; j < 10000000; j++);
    }

    dprintf(ALWAYS, "[Core %d] Worker thread finished on CPU %u\n", core_id, arch_curr_cpu_num());
    return 0;
}

/* Coordinator thread - manages workload distribution */
static int coordinator_thread_func(void *arg) {
    uint32_t round = 0;

    dprintf(ALWAYS, "[Coordinator] Started\n");

    while (round < 5) {  /* Run 5 rounds for demo */
        round++;

        dprintf(ALWAYS, "\n=== Round %u ===\n", round);

        /* Distribute work to all cores */
        dprintf(INFO, "[Coordinator] Distributing work to %d cores\n", SMP_MAX_CPUS);
        for (int i = 0; i < SMP_MAX_CPUS; i++) {
            sem_post(&work_sem, false);
        }

        /* Wait for all cores to complete phase */
        dprintf(INFO, "[Coordinator] Waiting for %d cores to complete\n", SMP_MAX_CPUS);
        for (int i = 0; i < SMP_MAX_CPUS; i++) {
            event_wait(&phase_event);
        }

        /* Print statistics */
        mutex_acquire(&print_mutex);
        dprintf(ALWAYS, "--- Round %u Statistics ---\n", round);
        dprintf(ALWAYS, "Shared counter: %u\n", shared_counter);
        for (int i = 0; i < SMP_MAX_CPUS; i++) {
            dprintf(ALWAYS, "Core %d: %u tasks completed\n", i, core_counters[i]);
        }
        dprintf(ALWAYS, "\n");
        mutex_release(&print_mutex);

        /* Busy wait between rounds */
        for (volatile int i = 0; i < 10000000; i++);
    }

    /* Signal workers to exit */
    demo_running = false;
    for (int i = 0; i < SMP_MAX_CPUS; i++) {
        sem_post(&work_sem, false);
    }

    dprintf(ALWAYS, "[Coordinator] Demo complete!\n");
    return 0;
}

/* Shell command to run independent demo */
static int cmd_smp_demo(int argc, const console_cmd_args *argv) {
    dprintf(ALWAYS, "\n========================================\n");
    dprintf(ALWAYS, "LK SMP Test - Independent Workers\n");
    dprintf(ALWAYS, "%d CPUs configured\n", SMP_MAX_CPUS);
    dprintf(ALWAYS, "========================================\n\n");

    /* Create independent worker threads pinned to each core */
    for (int i = 0; i < SMP_MAX_CPUS; i++) {
        char name[16];
        snprintf(name, sizeof(name), "worker%d", i);

        core_threads[i] = thread_create(
            name,
            worker_thread,
            (void *)(uintptr_t)i,
            WORKER_THREAD_PRIORITY,
            WORKER_STACK_SIZE
        );

        if (core_threads[i]) {
            /* Pin thread to specific CPU */
            thread_set_pinned_cpu(core_threads[i], i);
            thread_resume(core_threads[i]);
            dprintf(ALWAYS, "Created and started '%s' pinned to CPU %d\n", name, i);
        } else {
            dprintf(CRITICAL, "Failed to create worker thread for core %d\n", i);
        }
    }

    dprintf(ALWAYS, "\nAll %d worker threads started!\n", SMP_MAX_CPUS);
    dprintf(ALWAYS, "Each thread will run 10 iterations independently.\n\n");

    return 0;
}

/* Shell command to run synchronized demo */
static int cmd_smp_sync(int argc, const console_cmd_args *argv) {
    dprintf(ALWAYS, "\n========================================\n");
    dprintf(ALWAYS, "LK SMP Test - Synchronized Workers\n");
    dprintf(ALWAYS, "%d CPUs configured\n", SMP_MAX_CPUS);
    dprintf(ALWAYS, "========================================\n\n");

    /* Initialize synchronization primitives */
    sem_init(&work_sem, 0);
    mutex_init(&print_mutex);
    event_init(&phase_event, false, EVENT_FLAG_AUTOUNSIGNAL);

    /* Reset state */
    demo_running = true;
    shared_counter = 0;
    for (int i = 0; i < SMP_MAX_CPUS; i++) {
        core_counters[i] = 0;
    }

    /* Create synchronized worker threads pinned to each core */
    dprintf(ALWAYS, "Creating synchronized worker threads...\n");
    for (int i = 0; i < SMP_MAX_CPUS; i++) {
        char name[16];
        snprintf(name, sizeof(name), "syncwork%d", i);

        core_threads[i] = thread_create(
            name,
            worker_thread_sync,
            (void *)(uintptr_t)i,
            WORKER_THREAD_PRIORITY,
            WORKER_STACK_SIZE
        );

        if (core_threads[i]) {
            thread_set_pinned_cpu(core_threads[i], i);
            thread_resume(core_threads[i]);
            dprintf(INFO, "Created '%s' pinned to CPU %d\n", name, i);
        } else {
            dprintf(CRITICAL, "Failed to create sync worker for core %d\n", i);
        }
    }

    /* Create coordinator thread */
    dprintf(ALWAYS, "Creating coordinator thread...\n");
    coordinator_thread = thread_create(
        "coordinator",
        coordinator_thread_func,
        NULL,
        COORDINATOR_PRIORITY,
        WORKER_STACK_SIZE
    );

    if (coordinator_thread) {
        thread_resume(coordinator_thread);
        dprintf(ALWAYS, "Coordinator started - will run 5 rounds\n\n");
    } else {
        dprintf(CRITICAL, "Failed to create coordinator thread\n");
        return -1;
    }

    return 0;
}

STATIC_COMMAND_START
STATIC_COMMAND("smp_demo", "Run SMP demo with independent workers", &cmd_smp_demo)
STATIC_COMMAND("smp_sync", "Run SMP demo with synchronized workers (semaphore/mutex/event)", &cmd_smp_sync)
STATIC_COMMAND_END(smp_demo);

/* Auto-run demo at startup - DISABLED, use 'smp_demo' command instead */
#if 0
static void smp_demo_init(uint level) {
    dprintf(ALWAYS, "\n=== Auto-starting SMP demo ===\n");
    cmd_smp_demo(0, NULL);
}

LK_INIT_HOOK(smp_demo_auto, smp_demo_init, LK_INIT_LEVEL_APPS + 1);
#endif
