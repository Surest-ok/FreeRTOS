/*
 * CherryTimer - Standard Kernel Timers (struct timer_list)
 *
 * Copyright (c) 2026 ForgeStitch
 * SPDX-License-Identifier: MIT
 */

#include "timer.h"
#include "timer_internal.h"
#include <string.h>

void timer_setup(struct timer_list *timer, void (*callback)(struct timer_list *), unsigned int flags)
{
    if (!timer) return;
    memset(timer, 0, sizeof(*timer));
    timer->function = callback;
    timer->flags = flags;
    timer->cpu = -1;
    timer->active = false;
}

static void enqueue_timer_locked(struct timer_base *base, struct timer_list *timer)
{
    struct timer_list **curr = &base->timer_list_head;
    while (*curr) {
        if ((*curr)->expires_ns > timer->expires_ns) {
            break;
        }
        curr = &(*curr)->next;
    }
    timer->next = *curr;
    *curr = timer;
    timer->active = true;
}

static void dequeue_timer_locked(struct timer_base *base, struct timer_list *timer)
{
    struct timer_list **curr = &base->timer_list_head;
    while (*curr) {
        if (*curr == timer) {
            *curr = timer->next;
            timer->next = NULL;
            timer->active = false;
            return;
        }
        curr = &(*curr)->next;
    }
}

void add_timer_on(struct timer_list *timer, int cpu)
{
    if (!timer || !timer->function) return;
    timer->cpu = cpu;

    struct timer_base *base = timer_get_base(cpu);
    unsigned long flags = 0;
    spin_lock_irqsave(&base->lock, flags);

    dequeue_timer_locked(base, timer);
    enqueue_timer_locked(base, timer);

    spin_unlock_irqrestore(&base->lock, flags);
}

void add_timer(struct timer_list *timer)
{
    if (!timer) return;
    add_timer_on(timer, timer->cpu);
}

int mod_timer(struct timer_list *timer, unsigned long expires)
{
    if (!timer || !timer->function) return -1;

    timer->expires = expires;

    /* Calculate absolute nanosecond deadline from current ktime */
    uint64_t now_ns = (uint64_t)ktime_get();
    uint64_t delta_ns = (uint64_t)expires * 1000000ULL; /* expires in ms */
    timer->expires_ns = now_ns + delta_ns;

    struct timer_base *base = timer_get_base(timer->cpu);
    unsigned long flags = 0;
    spin_lock_irqsave(&base->lock, flags);

    int was_pending = timer->active ? 1 : 0;
    dequeue_timer_locked(base, timer);
    enqueue_timer_locked(base, timer);

    spin_unlock_irqrestore(&base->lock, flags);
    return was_pending;
}

int del_timer(struct timer_list *timer)
{
    if (!timer) return 0;
    struct timer_base *base = timer_get_base(timer->cpu);
    unsigned long flags = 0;
    spin_lock_irqsave(&base->lock, flags);

    int was_pending = timer->active ? 1 : 0;
    dequeue_timer_locked(base, timer);

    spin_unlock_irqrestore(&base->lock, flags);
    return was_pending;
}

int del_timer_sync(struct timer_list *timer)
{
    return del_timer(timer);
}

int timer_pending(const struct timer_list *timer)
{
    return (timer && timer->active) ? 1 : 0;
}
