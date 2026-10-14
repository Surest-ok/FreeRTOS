/*
 * CherryPrintk - Linux Kernel Standard printk Engine
 *
 * Copyright (c) 2026 ForgeStitch
 * SPDX-License-Identifier: MIT
 *
 * Features:
 * 1. Linux dmesg format: <level>[seconds.microseconds] [CPUx:task_name:tid] msg
 * 2. Multi-core concurrent log buffering without message interleaving
 * 3. Shortest interrupt-disabled window (< 100ns)
 * 4. Zero interference with hardware timer interrupts (IRQ_TIMER / PPI 14)
 */

#include "printk.h"
#include "printk_ringbuffer.h"
#include "cherry_kal.h"
#include "uart.h"
#include <string.h>

/* BSP Timer and UART dependencies */
extern uint64_t gtimer_get_counter(void);
extern uint64_t gtimer_get_frequency(void);
extern void uart_puts(const char *str);

/* FreeRTOS Scheduler & Task queries */
#define taskSCHEDULER_RUNNING 2
extern int xTaskGetSchedulerState(void);
extern char *pcTaskGetName(void *xTaskToQuery);
extern void *xTaskGetCurrentTaskHandle(void);
extern uint64_t ulPortInterruptNestings[];

static spinlock_t s_console_lock = SPIN_LOCK_UNLOCKED;
static bool s_printk_inited = false;

void cherry_printk_init(void)
{
    if (s_printk_inited) return;
    printk_rb_init();
    spin_lock_init(&s_console_lock);
    s_printk_inited = true;
}

static void console_drain(void)
{
    struct printk_record rec;
    uint64_t freq = gtimer_get_frequency();
    if (freq == 0) freq = 62500000ULL; /* QEMU virt default 62.5MHz */

    char line_buf[PRINTK_MAX_TEXT_LEN + 96];

    while (printk_rb_read(&rec)) {
        uint64_t sec = rec.timestamp_cnt / freq;
        uint64_t rem = rec.timestamp_cnt % freq;
        uint64_t usec = (rem * 1000000ULL) / freq;

        snprintf(line_buf, sizeof(line_buf),
                 "<%u>[%5lu.%06lu] [CPU%u:%s:%u] %s\n",
                 (unsigned int)rec.level,
                 (unsigned long)sec,
                 (unsigned long)usec,
                 (unsigned int)rec.cpu_id,
                 rec.thread_name,
                 (unsigned int)rec.thread_id,
                 rec.text);

        uart_puts(line_buf);
    }
}

void cherry_printk_flush(void)
{
    if (!s_printk_inited) {
        cherry_printk_init();
    }
    spin_lock(&s_console_lock);
    console_drain();
    spin_unlock(&s_console_lock);
}

int vprintk(const char *fmt, va_list args)
{
    if (!fmt) return 0;
    if (!s_printk_inited) {
        cherry_printk_init();
    }

    uint8_t level = LOGLEVEL_INFO;
    const char *p = fmt;

    /* Parse Linux log level prefix: "<0>" ~ "<7>" */
    if (p[0] == '<' && p[1] >= '0' && p[1] <= '7' && p[2] == '>') {
        level = (uint8_t)(p[1] - '0');
        p += 3;
    }

    struct printk_record rec;
    memset(&rec, 0, sizeof(rec));
    rec.level = level;

    /* 1. Format text with interrupts ENABLED (zero critical section for formatting) */
    int len = vsnprintf(rec.text, sizeof(rec.text), p, args);
    if (len < 0) return len;
    if (len >= (int)sizeof(rec.text)) {
        len = (int)sizeof(rec.text) - 1;
        rec.text[len] = '\0';
    }

    /* Strip trailing \r or \n */
    while (len > 0 && (rec.text[len - 1] == '\r' || rec.text[len - 1] == '\n')) {
        rec.text[--len] = '\0';
    }
    rec.len = (uint16_t)len;

    /* 2. Read hardware timestamp from BSP Generic Timer (1 CPU instruction) */
    rec.timestamp_cnt = gtimer_get_counter();

    /* 3. Read current CPU and thread metadata */
    uint32_t cpu = smp_processor_id();
    rec.cpu_id = cpu;

    if (xTaskGetSchedulerState() != taskSCHEDULER_RUNNING) {
        strncpy(rec.thread_name, "swapper", sizeof(rec.thread_name) - 1);
        rec.thread_id = 0;
    } else if (ulPortInterruptNestings[cpu] > 0) {
        strncpy(rec.thread_name, "irq", sizeof(rec.thread_name) - 1);
        rec.thread_id = (uint32_t)ulPortInterruptNestings[cpu];
    } else {
        const char *tname = pcTaskGetName(NULL);
        void *thandle = xTaskGetCurrentTaskHandle();
        strncpy(rec.thread_name, tname ? tname : "task", sizeof(rec.thread_name) - 1);
        rec.thread_id = (uint32_t)(((uintptr_t)thandle >> 4) & 0xFFFFU);
    }

    /* 4. Enqueue into ring buffer under ultra-short IRQ-disabled lock (< 100ns) */
    printk_rb_write(&rec);

    /* 5. Emit to console with INTERRUPTS ENABLED (non-blocking for concurrent callers) */
    if (spin_trylock(&s_console_lock)) {
        console_drain();
        spin_unlock(&s_console_lock);
    }

    return len;
}

int printk(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int ret = vprintk(fmt, ap);
    va_end(ap);
    return ret;
}
