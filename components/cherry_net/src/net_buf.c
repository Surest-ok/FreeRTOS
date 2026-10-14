/*
 * CherryNet - High-Performance Packet Buffer Management
 *
 * Copyright (c) 2026 ForgeStitch
 * SPDX-License-Identifier: MIT
 */

#include "net_buf.h"
#include "cherry_kal.h"
#include <string.h>

static struct net_buf  s_buf_pool[NET_BUF_POOL_SIZE];
static struct net_buf *s_free_list = NULL;
static spinlock_t      s_pool_lock = SPIN_LOCK_UNLOCKED;

int net_buf_pool_init(void)
{
    spin_lock_init(&s_pool_lock);

    s_free_list = NULL;
    for (int i = 0; i < NET_BUF_POOL_SIZE; i++) {
        struct net_buf *b = &s_buf_pool[i];
        memset(b, 0, sizeof(*b));
        b->head = b->payload;
        b->end = b->payload + NET_BUF_DATA_SIZE;
        b->data = b->head + NET_BUF_HEADROOM;
        b->tail = b->data;
        b->len = 0;
        b->tot_len = 0;
        b->ref = 0;

        b->next = s_free_list;
        s_free_list = b;
    }
    return 0;
}

struct net_buf *net_buf_alloc(void)
{
    unsigned long flags;
    spin_lock_irqsave(&s_pool_lock, flags);

    struct net_buf *b = s_free_list;
    if (b) {
        s_free_list = b->next;
        b->next = NULL;
        b->ref = 1;
        b->data = b->head + NET_BUF_HEADROOM;
        b->tail = b->data;
        b->len = 0;
        b->tot_len = 0;
        b->flags = 0;
        b->netif = NULL;
        b->flow_hash = 0;
        b->core_id = 0;
        b->src_port = 0;
        b->dst_port = 0;
        b->src_ip = 0;
        memset(&b->src_ip6, 0, sizeof(b->src_ip6));
    }

    spin_unlock_irqrestore(&s_pool_lock, flags);
    return b;
}

void net_buf_ref(struct net_buf *buf)
{
    if (!buf) return;
    unsigned long flags;
    spin_lock_irqsave(&s_pool_lock, flags);
    buf->ref++;
    spin_unlock_irqrestore(&s_pool_lock, flags);
}

void net_buf_free(struct net_buf *buf)
{
    while (buf) {
        struct net_buf *next = buf->next;
        unsigned long flags;
        spin_lock_irqsave(&s_pool_lock, flags);

        if (buf->ref > 1) {
            buf->ref--;
            spin_unlock_irqrestore(&s_pool_lock, flags);
            return;
        }

        buf->ref = 0;
        buf->next = s_free_list;
        s_free_list = buf;

        spin_unlock_irqrestore(&s_pool_lock, flags);
        buf = next;
    }
}
