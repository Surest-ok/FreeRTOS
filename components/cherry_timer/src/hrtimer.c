/*
 * CherryTimer - High-Resolution Timers (struct hrtimer)
 *
 * Copyright (c) 2026 ForgeStitch
 * SPDX-License-Identifier: MIT
 */

#include "hrtimer.h"
#include "timer_internal.h"
#include <string.h>

extern uint64_t gtimer_get_counter(void);
extern uint64_t gtimer_get_frequency(void);

ktime_t ktime_get(void)
{
    uint64_t cnt = gtimer_get_counter();
    uint64_t frq = gtimer_get_frequency();
    if (frq == 0) frq = 62500000ULL; /* QEMU virt ARM Generic Timer default */
    return (ktime_t)((cnt * 1000000000ULL) / frq);
}

void hrtimer_init(struct hrtimer *timer, int clock_id, enum hrtimer_mode mode)
{
    (void)clock_id;
    (void)mode;
    if (!timer) return;
    memset(timer, 0, sizeof(*timer));
    timer->state = HRTIMER_STATE_INACTIVE;
    timer->base_cpu = -1;
    timer->active = false;
}

static void enqueue_hrtimer_locked(struct timer_base *base, struct hrtimer *timer)
{
    struct hrtimer **curr = &base->hrtimer_head;
    while (*curr) {
        if ((*curr)->expires > timer->expires) {
            break;
        }
        curr = &(*curr)->next;
    }
    timer->next = *curr;
    *curr = timer;
    timer->state = HRTIMER_STATE_ENQUEUED;
    timer->active = true;
}

static void dequeue_hrtimer_locked(struct timer_base *base, struct hrtimer *timer)
{
    struct hrtimer **curr = &base->hrtimer_head;
    while (*curr) {
        if (*curr == timer) {
            *curr = timer->next;
            timer->next = NULL;
            timer->state = HRTIMER_STATE_INACTIVE;
            timer->active = false;
            return;
        }
        curr = &(*curr)->next;
    }
}

void hrtimer_start(struct hrtimer *timer, ktime_t tim, enum hrtimer_mode mode)
{
    if (!timer || !timer->function) return;

    ktime_t now = ktime_get();
    if (mode & HRTIMER_MODE_REL) {
        timer->expires = now + tim;
    } else {
        timer->expires = tim;
    }
    timer->_softexpires = timer->expires;

    struct timer_base *base = timer_get_base(timer->base_cpu);
    unsigned long flags = 0;
    spin_lock_irqsave(&base->lock, flags);

    dequeue_hrtimer_locked(base, timer);
    enqueue_hrtimer_locked(base, timer);

    spin_unlock_irqrestore(&base->lock, flags);
}

int hrtimer_cancel(struct hrtimer *timer)
{
    if (!timer) return 0;
    struct timer_base *base = timer_get_base(timer->base_cpu);
    unsigned long flags = 0;
    spin_lock_irqsave(&base->lock, flags);

    int was_active = timer->active ? 1 : 0;
    dequeue_hrtimer_locked(base, timer);

    spin_unlock_irqrestore(&base->lock, flags);
    return was_active;
}

int hrtimer_try_to_cancel(struct hrtimer *timer)
{
    return hrtimer_cancel(timer);
}

bool hrtimer_active(const struct hrtimer *timer)
{
    return (timer && timer->active);
}
