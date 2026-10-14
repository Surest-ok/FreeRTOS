/*
 * CherryTimer - Master Subsystem, Per-CPU Timer Bases & Queue Dispatcher
 *
 * Copyright (c) 2026 ForgeStitch
 * SPDX-License-Identifier: MIT
 */

#include "cherry_timer.h"
#include "timer_internal.h"
#include "cherry_kal.h"
#include <string.h>

struct timer_base g_cpu_timer_bases[TIMER_MAX_CPUS];
static bool s_timer_subsystem_inited = false;
static struct task_struct *s_timer_daemon = NULL;

struct timer_base *timer_get_base(int cpu)
{
    if (cpu < 0 || cpu >= TIMER_MAX_CPUS) {
        cpu = (int)smp_processor_id();
    }
    if (cpu < 0 || cpu >= TIMER_MAX_CPUS) {
        cpu = 0;
    }
    return &g_cpu_timer_bases[cpu];
}

void cherry_timer_run_queues(void)
{
    int current_cpu = (int)smp_processor_id();
    ktime_t now = ktime_get();

    for (int idx = 0; idx < TIMER_MAX_CPUS; idx++) {
        int cpu = (idx == 0) ? current_cpu : (idx <= current_cpu ? idx - 1 : idx);
        struct timer_base *base = &g_cpu_timer_bases[cpu];

        /* 1. Dispatch High-Resolution Timers (struct hrtimer) */
        for (;;) {
            struct hrtimer *target = NULL;
            unsigned long flags = 0;
            spin_lock_irqsave(&base->lock, flags);

            if (base->hrtimer_head && base->hrtimer_head->expires <= now) {
                target = base->hrtimer_head;
                base->hrtimer_head = target->next;
                target->next = NULL;
                target->state = HRTIMER_STATE_CALLBACK;
                target->active = false;
                base->expired_count++;
            }

            spin_unlock_irqrestore(&base->lock, flags);

            if (!target) break;

            /* Execute callback with interrupts ENABLED */
            enum hrtimer_restart restart = HRTIMER_NORESTART;
            if (target->function) {
                restart = target->function(target);
            }

            /* If callback requested restart, re-enqueue timer */
            if (restart == HRTIMER_RESTART) {
                spin_lock_irqsave(&base->lock, flags);
                struct hrtimer **curr = &base->hrtimer_head;
                while (*curr) {
                    if ((*curr)->expires > target->expires) break;
                    curr = &(*curr)->next;
                }
                target->next = *curr;
                *curr = target;
                target->state = HRTIMER_STATE_ENQUEUED;
                target->active = true;
                spin_unlock_irqrestore(&base->lock, flags);
            } else {
                target->state = HRTIMER_STATE_INACTIVE;
            }
        }

        /* 2. Dispatch Standard Timers (struct timer_list) */
        for (;;) {
            struct timer_list *target = NULL;
            unsigned long flags = 0;
            spin_lock_irqsave(&base->lock, flags);

            if (base->timer_list_head && base->timer_list_head->expires_ns <= (uint64_t)now) {
                target = base->timer_list_head;
                base->timer_list_head = target->next;
                target->next = NULL;
                target->active = false;
                base->expired_count++;
            }

            spin_unlock_irqrestore(&base->lock, flags);

            if (!target) break;

            /* Execute callback with interrupts ENABLED */
            if (target->function) {
                target->function(target);
            }
        }
    }
}

void cherry_timer_poll(void)
{
    cherry_timer_run_queues();
}

static int cherry_timer_daemon_fn(void *data)
{
    (void)data;
    for (;;) {
        cherry_timer_run_queues();
        msleep(1);
    }
    return 0;
}

void cherry_timer_init(void)
{
    if (s_timer_subsystem_inited) return;

    for (int i = 0; i < TIMER_MAX_CPUS; i++) {
        spin_lock_init(&g_cpu_timer_bases[i].lock);
        g_cpu_timer_bases[i].timer_list_head = NULL;
        g_cpu_timer_bases[i].hrtimer_head = NULL;
        g_cpu_timer_bases[i].cpu_id = (uint32_t)i;
        g_cpu_timer_bases[i].expired_count = 0;
    }

    s_timer_subsystem_inited = true;

    /* Launch background timer engine thread */
    if (s_timer_daemon == NULL) {
        s_timer_daemon = kthread_run(cherry_timer_daemon_fn, NULL, "ktimerd");
    }
}
