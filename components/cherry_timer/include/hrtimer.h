/*
 * CherryTimer - Linux Standard High-Resolution Timer (struct hrtimer)
 *
 * Copyright (c) 2026 ForgeStitch
 * SPDX-License-Identifier: MIT
 */

#ifndef _LINUX_HRTIMER_H_
#define _LINUX_HRTIMER_H_

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef int64_t ktime_t;

#define CLOCK_REALTIME  0
#define CLOCK_MONOTONIC 1

enum hrtimer_restart {
    HRTIMER_NORESTART = 0,
    HRTIMER_RESTART   = 1,
};

enum hrtimer_mode {
    HRTIMER_MODE_ABS    = 0x00,
    HRTIMER_MODE_REL    = 0x01,
    HRTIMER_MODE_PINNED = 0x02,
};

static inline ktime_t ktime_set(const int64_t secs, const unsigned long nsecs)
{
    return (secs * 1000000000LL) + (int64_t)nsecs;
}

static inline int64_t ktime_to_ns(const ktime_t kt) { return kt; }
static inline int64_t ktime_to_us(const ktime_t kt) { return kt / 1000LL; }
static inline int64_t ktime_to_ms(const ktime_t kt) { return kt / 1000000LL; }
static inline ktime_t ns_to_ktime(uint64_t ns)      { return (ktime_t)ns; }
static inline ktime_t ms_to_ktime(uint64_t ms)      { return (ktime_t)(ms * 1000000ULL); }

ktime_t ktime_get(void);

struct hrtimer {
    struct hrtimer         *next;
    ktime_t                 _softexpires;
    ktime_t                 expires;
    enum hrtimer_restart  (*function)(struct hrtimer *);
    uint8_t                 state;
    int                     base_cpu;     /* Target CPU ID */
    void                   *data;
    volatile bool           active;
};

#define HRTIMER_STATE_INACTIVE 0x00
#define HRTIMER_STATE_ENQUEUED 0x01
#define HRTIMER_STATE_CALLBACK 0x02

void hrtimer_init(struct hrtimer *timer, int clock_id, enum hrtimer_mode mode);
void hrtimer_start(struct hrtimer *timer, ktime_t tim, enum hrtimer_mode mode);
int  hrtimer_cancel(struct hrtimer *timer);
int  hrtimer_try_to_cancel(struct hrtimer *timer);
bool hrtimer_active(const struct hrtimer *timer);

#ifdef __cplusplus
}
#endif

#endif /* _LINUX_HRTIMER_H_ */
