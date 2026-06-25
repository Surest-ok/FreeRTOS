/*
 * Platform header for QEMU virt
 */
#pragma once

#include <platform/qemu-virt.h>
#include <sys/types.h>

/* Platform halt actions and reasons */
typedef enum {
    HALT_ACTION_HALT = 0,
    HALT_ACTION_REBOOT,
    HALT_ACTION_SHUTDOWN
} platform_halt_action;

typedef enum {
    HALT_REASON_UNKNOWN = 0,
    HALT_REASON_POR,
    HALT_REASON_HW_WATCHDOG,
    HALT_REASON_LOWVOLTAGE,
    HALT_REASON_HIGHVOLTAGE,
    HALT_REASON_THERMAL,
    HALT_REASON_OTHER_HW,
    HALT_REASON_SW_RESET,
    HALT_REASON_SW_WATCHDOG,
    HALT_REASON_SW_PANIC,
    HALT_REASON_SW_UPDATE
} platform_halt_reason;

/* Platform timer callback */
typedef enum handler_return (*platform_timer_callback)(void *arg, lk_time_t now);

void platform_early_init(void);
void platform_init(void);
void platform_halt(platform_halt_action suggested_action, platform_halt_reason reason);

/* Platform halt helper */
void platform_halt_default(platform_halt_action suggested_action,
                           platform_halt_reason reason,
                           void (*reset_func)(void),
                           void (*off_func)(void));

/* Platform MMU */
void platform_init_mmu_mappings(void);

/* Platform timer */
void platform_set_periodic_timer(platform_timer_callback callback, void *arg, lk_time_t interval);
