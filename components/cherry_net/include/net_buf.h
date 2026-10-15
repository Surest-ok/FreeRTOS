/*
 * CherryNet - High-Performance Packet Buffer Management
 *
 * Copyright (c) 2026 ForgeStitch
 * SPDX-License-Identifier: MIT
 */

#ifndef _CHERRY_NET_BUF_H_
#define _CHERRY_NET_BUF_H_

#include "net_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define NET_BUF_POOL_SIZE     128
#define NET_BUF_DATA_SIZE     2048
#define NET_BUF_HEADROOM      128

struct netif;

/**
 * High-performance network packet buffer (pbuf/sk_buff inspired)
 */
struct net_buf {
    struct net_buf *next;       /* Linked list linkage for fragmentation/queues */
    uint8_t        *head;       /* Start of allocated memory */
    uint8_t        *data;       /* Start of valid payload */
    uint8_t        *tail;       /* End of valid payload */
    uint8_t        *end;        /* End of allocated memory */
    uint32_t        len;        /* Valid payload length */
    uint32_t        tot_len;    /* Total length including next chained buffers */
    uint16_t        flags;      /* Buffer flags */
    uint16_t        ref;        /* Reference counter */
    struct netif   *netif;      /* Originating or destination interface */
    uint32_t        flow_hash;  /* 4-tuple flow hash for multi-core steering */
    uint8_t         core_id;    /* Target CPU core */
    uint8_t         pad;
    uint16_t        src_port;
    uint16_t        dst_port;
    uint16_t        pad2;
    uint32_t        src_ip;
    struct ip6_addr src_ip6;
    uint8_t         payload[NET_BUF_DATA_SIZE] __attribute__((aligned(16)));
};

int             net_buf_pool_init(void);
struct net_buf *net_buf_alloc(void);
void            net_buf_free(struct net_buf *buf);
void            net_buf_ref(struct net_buf *buf);

static inline void *net_buf_push(struct net_buf *buf, uint32_t len)
{
    if (!buf || (buf->data - buf->head) < (ptrdiff_t)len) return NULL;
    buf->data -= len;
    buf->len += len;
    buf->tot_len += len;
    return buf->data;
}

static inline void *net_buf_pull(struct net_buf *buf, uint32_t len)
{
    if (!buf || buf->len < len) return NULL;
    buf->data += len;
    buf->len -= len;
    buf->tot_len -= len;
    return buf->data;
}

static inline void net_buf_reserve(struct net_buf *buf, uint32_t len)
{
    if (buf && (buf->data + len) <= buf->end) {
        buf->data += len;
        buf->tail += len;
    }
}

#ifdef __cplusplus
}
#endif

#endif /* _CHERRY_NET_BUF_H_ */
