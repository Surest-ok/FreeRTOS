/*
 * CherryTimer - Internal Definitions and Per-CPU Timer Bases
 *
 * Copyright (c) 2026 ForgeStitch
 * SPDX-License-Identifier: MIT
 */

#ifndef _TIMER_INTERNAL_H_
#define _TIMER_INTERNAL_H_

#include "cherry_timer.h"
#include "cherry_kal.h"

#define TIMER_MAX_CPUS 8

struct timer_base {
    spinlock_t           lock;
    struct timer_list   *timer_list_head;
    struct hrtimer      *hrtimer_head;
    uint32_t             cpu_id;
    uint64_t             expired_count;
};

extern struct timer_base g_cpu_timer_bases[TIMER_MAX_CPUS];

struct timer_base *timer_get_base(int cpu);

#endif /* _TIMER_INTERNAL_H_ */
