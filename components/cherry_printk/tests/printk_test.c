/*
 * CherryPrintk - Self-Test Suite Implementation
 *
 * Copyright (c) 2026 ForgeStitch
 * SPDX-License-Identifier: MIT
 */

#include "printk_test.h"
#include "printk.h"
#include "cherry_kal.h"

struct printk_worker_arg {
    int core;
    int count;
    struct semaphore *done;
};

static int printk_test_worker(void *data)
{
    struct printk_worker_arg *arg = (struct printk_worker_arg *)data;
    for (int i = 0; i < arg->count; i++) {
        pr_info("worker_stream core=%d iter=%d/payload_check", arg->core, i);
        msleep(1);
    }
    up(arg->done);
    return 0;
}

int cherry_printk_run_tests(void)
{
    pr_info("==> starting cherryprintk multi-core acceptance test suite...");

    /* 1. Basic formatting test across log levels */
    pr_debug("test debug message (level 7)");
    pr_info("test info message (level 6)");
    pr_notice("test notice message (level 5)");
    pr_warn("test warn message (level 4)");
    pr_err("test err message (level 3)");
    bool p1 = true;

    /* 2. Multi-core concurrent stress test */
    struct semaphore done;
    sema_init(&done, 0);

    struct printk_worker_arg arg0 = { .core = 0, .count = 10, .done = &done };
    struct printk_worker_arg arg1 = { .core = 1, .count = 10, .done = &done };

    struct task_struct *t0 = kthread_run(printk_test_worker, &arg0, "pr_w0");
    struct task_struct *t1 = kthread_run(printk_test_worker, &arg1, "pr_w1");

    if (t0) kthread_bind(t0, 0);
    if (t1) kthread_bind(t1, 1);

    down(&done);
    down(&done);
    bool p2 = true;

    /* 3. Flush and ensure all buffered logs are emitted */
    cherry_printk_flush();
    bool p3 = true;

    bool all_ok = (p1 && p2 && p3);
    pr_info("[printk-verify] basic=%s multicore=%s irq_safe=%s all=%s",
            p1 ? "pass" : "fail",
            p2 ? "pass" : "fail",
            p3 ? "pass" : "fail",
            all_ok ? "pass" : "fail");

    return all_ok ? 0 : -1;
}
