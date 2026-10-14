/*
 * CherryPrintk - High Performance Concurrent Linux-Style Logging Engine
 *
 * Copyright (c) 2026 ForgeStitch
 * SPDX-License-Identifier: MIT
 *
 * Features:
 * - 100% Linux printk format: <level>[seconds.microseconds] [CPUx:task:tid] msg
 * - Multi-core SMP concurrent log buffering without message interleaving
 * - Ultra-short interrupt disable duration (< 100ns)
 * - Zero interference with timer ticks and high-precision timers
 */

#ifndef _LINUX_PRINTK_H_
#define _LINUX_PRINTK_H_

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =========================================================================
 * 1. Linux Standard Log Levels
 * ========================================================================= */
#define KERN_SOH     "\001"     /* ASCII Start Of Header */
#define KERN_SOH_ASCII '\001'

#define KERN_EMERG   "<0>"      /* system is unusable */
#define KERN_ALERT   "<1>"      /* action must be taken immediately */
#define KERN_CRIT    "<2>"      /* critical conditions */
#define KERN_ERR     "<3>"      /* error conditions */
#define KERN_WARNING "<4>"      /* warning conditions */
#define KERN_NOTICE  "<5>"      /* normal but significant condition */
#define KERN_INFO    "<6>"      /* informational */
#define KERN_DEBUG   "<7>"      /* debug-level messages */

#define LOGLEVEL_EMERG   0
#define LOGLEVEL_ALERT   1
#define LOGLEVEL_CRIT    2
#define LOGLEVEL_ERR     3
#define LOGLEVEL_WARNING 4
#define LOGLEVEL_NOTICE  5
#define LOGLEVEL_INFO    6
#define LOGLEVEL_DEBUG   7

/* =========================================================================
 * 2. Standard Linux printk & Helpers
 * ========================================================================= */
int printk(const char *fmt, ...);
int vprintk(const char *fmt, va_list args);

#define pr_emerg(fmt, ...)   printk(KERN_EMERG fmt, ##__VA_ARGS__)
#define pr_alert(fmt, ...)   printk(KERN_ALERT fmt, ##__VA_ARGS__)
#define pr_crit(fmt, ...)    printk(KERN_CRIT fmt, ##__VA_ARGS__)
#define pr_err(fmt, ...)     printk(KERN_ERR fmt, ##__VA_ARGS__)
#define pr_warn(fmt, ...)    printk(KERN_WARNING fmt, ##__VA_ARGS__)
#define pr_warning           pr_warn
#define pr_notice(fmt, ...)  printk(KERN_NOTICE fmt, ##__VA_ARGS__)
#define pr_info(fmt, ...)    printk(KERN_INFO fmt, ##__VA_ARGS__)
#define pr_debug(fmt, ...)   printk(KERN_DEBUG fmt, ##__VA_ARGS__)

/* =========================================================================
 * 3. CherryPrintk Subsystem Control APIs
 * ========================================================================= */
void cherry_printk_init(void);
void cherry_printk_flush(void);
int  cherry_printk_run_tests(void);

#ifdef __cplusplus
}
#endif

#endif /* _LINUX_PRINTK_H_ */
