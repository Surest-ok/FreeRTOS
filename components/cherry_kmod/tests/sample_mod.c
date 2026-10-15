/*
 * Sample Loadable Kernel Module (.ko) for CherryKmod
 *
 * Copyright (c) 2026 ForgeStitch
 * SPDX-License-Identifier: MIT
 */

#include <stdint.h>
#include <stddef.h>
#include "cherry_kmod.h"

extern void printk(const char *fmt, ...);
extern void *kmalloc(size_t size, int flags);
extern void kfree(void *ptr);

static int g_sample_counter = 100;
static const char g_sample_tag[] = "CHERRY_SAMPLE_TAG_OK";

/* Exported function callable from kernel or shell */
int sample_calc(int a, int b)
{
    return a * 10 + b + g_sample_counter;
}

const char *sample_get_tag(void)
{
    return g_sample_tag;
}

int init_module(void)
{
    g_sample_counter = 500;
    void *p = kmalloc(32, 0);
    if (p) {
        kfree(p);
    }
    printk("[sample_mod] init_module() executed successfully! counter=%d\n", g_sample_counter);
    return 0;
}

void cleanup_module(void)
{
    printk("[sample_mod] cleanup_module() executed successfully!\n");
    g_sample_counter = 0;
}

MODULE_LICENSE("GPL");
