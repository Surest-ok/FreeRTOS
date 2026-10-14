/*
 * CherryPrintk - Ring Buffer Internal Header
 *
 * Copyright (c) 2026 ForgeStitch
 * SPDX-License-Identifier: MIT
 */

#ifndef _PRINTK_RINGBUFFER_H_
#define _PRINTK_RINGBUFFER_H_

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define PRINTK_MAX_TEXT_LEN     256
#define PRINTK_RING_ENTRIES     128

struct printk_record {
    uint64_t timestamp_cnt;
    uint32_t cpu_id;
    uint32_t thread_id;
    char     thread_name[16];
    uint8_t  level;
    uint16_t len;
    char     text[PRINTK_MAX_TEXT_LEN];
};

void printk_rb_init(void);
bool printk_rb_write(struct printk_record *rec);
bool printk_rb_read(struct printk_record *out_rec);
bool printk_rb_has_pending(void);
uint32_t printk_rb_pending_count(void);

#endif /* _PRINTK_RINGBUFFER_H_ */
