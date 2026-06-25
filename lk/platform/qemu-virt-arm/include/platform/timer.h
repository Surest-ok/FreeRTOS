/*
 * Platform timer definitions for QEMU virt
 */
#pragma once

#include <sys/types.h>

/* Platform timer functions implemented by arm_generic_timer driver */
lk_time_t current_time(void);
lk_bigtime_t current_time_hires(void);
