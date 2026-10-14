/*
 * CherryTimer - Top-level include
 *
 * Copyright (c) 2026 ForgeStitch
 * SPDX-License-Identifier: MIT
 */

#ifndef _CHERRY_TIMER_H_
#define _CHERRY_TIMER_H_

#include "timer.h"
#include "hrtimer.h"

#ifdef __cplusplus
extern "C" {
#endif

void cherry_timer_init(void);
void cherry_timer_run_queues(void);
void cherry_timer_poll(void);
int  cherry_timer_run_tests(void);

#ifdef __cplusplus
}
#endif

#endif /* _CHERRY_TIMER_H_ */
