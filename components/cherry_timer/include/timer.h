/*
 * CherryTimer - Linux Standard Low-Resolution / Kernel Timer (struct timer_list)
 *
 * Copyright (c) 2026 ForgeStitch
 * SPDX-License-Identifier: MIT
 */

#ifndef _LINUX_TIMER_H_
#define _LINUX_TIMER_H_

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TIMER_PINNED    0x0001U
#define TIMER_IRQSAFE   0x0002U

struct timer_list {
    struct timer_list *next;
    unsigned long      expires;       /* Expire time in jiffies or ms */
    uint64_t           expires_ns;    /* Absolute expire timestamp in nanoseconds */
    void             (*function)(struct timer_list *);
    unsigned int       flags;
    int                cpu;           /* Target CPU ID (-1 for current CPU) */
    void              *data;
    volatile bool      active;
};

void timer_setup(struct timer_list *timer, void (*callback)(struct timer_list *), unsigned int flags);
void add_timer(struct timer_list *timer);
void add_timer_on(struct timer_list *timer, int cpu);
int  mod_timer(struct timer_list *timer, unsigned long expires);
int  del_timer(struct timer_list *timer);
int  del_timer_sync(struct timer_list *timer);
int  timer_pending(const struct timer_list *timer);

#ifdef __cplusplus
}
#endif

#endif /* _LINUX_TIMER_H_ */
