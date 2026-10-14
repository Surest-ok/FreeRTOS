/*
 * CherryNet - Multi-Core Multi-Queue RSS Engine
 *
 * Copyright (c) 2026 ForgeStitch
 * SPDX-License-Identifier: MIT
 */

#include "net_queue.h"
#include "net_eth.h"
#include "net_ip4.h"
#include "net_ip6.h"
#include "cherry_kal.h"
#include <string.h>

static struct net_core_queue s_core_queues[NET_MAX_CORES];
static uint32_t              s_num_cores = 1;
static volatile bool         s_workers_running = false;

/* 4-tuple FNV-1a Hash */
uint32_t net_flow_hash(uint32_t s_ip, uint32_t d_ip, uint16_t s_port, uint16_t d_port)
{
    uint32_t hash = 2166136261U;
    const uint8_t *p;

    p = (const uint8_t *)&s_ip;
    for (int i = 0; i < 4; i++) { hash ^= p[i]; hash *= 16777619U; }

    p = (const uint8_t *)&d_ip;
    for (int i = 0; i < 4; i++) { hash ^= p[i]; hash *= 16777619U; }

    p = (const uint8_t *)&s_port;
    for (int i = 0; i < 2; i++) { hash ^= p[i]; hash *= 16777619U; }

    p = (const uint8_t *)&d_port;
    for (int i = 0; i < 2; i++) { hash ^= p[i]; hash *= 16777619U; }

    return hash;
}

uint32_t net_flow_hash6(const struct ip6_addr *s_ip6, const struct ip6_addr *d_ip6, uint16_t s_port, uint16_t d_port)
{
    uint32_t hash = 2166136261U;
    if (s_ip6) {
        for (int i = 0; i < 16; i++) { hash ^= s_ip6->u8[i]; hash *= 16777619U; }
    }
    if (d_ip6) {
        for (int i = 0; i < 16; i++) { hash ^= d_ip6->u8[i]; hash *= 16777619U; }
    }
    const uint8_t *p = (const uint8_t *)&s_port;
    for (int i = 0; i < 2; i++) { hash ^= p[i]; hash *= 16777619U; }
    p = (const uint8_t *)&d_port;
    for (int i = 0; i < 2; i++) { hash ^= p[i]; hash *= 16777619U; }
    return hash;
}

static int net_worker_fn(void *arg)
{
    uint32_t core_id = (uint32_t)(uintptr_t)arg;
    struct net_core_queue *q = &s_core_queues[core_id];

    while (s_workers_running) {
        down(&q->sem);
        if (!s_workers_running) break;

        struct net_buf *buf = NULL;
        unsigned long flags;
        spin_lock_irqsave(&q->lock, flags);

        if (q->count > 0) {
            buf = q->ring[q->head];
            q->ring[q->head] = NULL;
            q->head = (q->head + 1) % NET_QUEUE_RING_SIZE;
            q->count--;
            q->processed_packets++;
        }

        spin_unlock_irqrestore(&q->lock, flags);

        if (buf) {
            if (buf->netif && (buf->netif->flags & NETIF_FLAG_LOOPBACK)) {
                uint8_t ver = (buf->len > 0) ? ((buf->data[0] >> 4) & 0x0F) : 0;
                if (ver == 4) {
                    ip4_input(buf->netif, buf);
                } else if (ver == 6) {
                    ip6_input(buf->netif, buf);
                } else {
                    net_buf_free(buf);
                }
            } else {
                eth_input(buf->netif, buf);
            }
        }
    }
    return 0;
}

int net_queue_init(uint32_t num_cores)
{
    if (num_cores == 0) num_cores = 1;
    if (num_cores > NET_MAX_CORES) num_cores = NET_MAX_CORES;
    s_num_cores = num_cores;
    s_workers_running = true;

    for (uint32_t c = 0; c < s_num_cores; c++) {
        struct net_core_queue *q = &s_core_queues[c];
        memset(q, 0, sizeof(*q));
        q->core_id = c;
        spin_lock_init(&q->lock);
        sema_init(&q->sem, 0);

        char task_name[16];
        task_name[0] = 'n'; task_name[1] = 'e'; task_name[2] = 't';
        task_name[3] = '_'; task_name[4] = 'w'; task_name[5] = 'k';
        task_name[6] = '0' + (char)c; task_name[7] = '\0';

        q->worker_task = kthread_run(net_worker_fn, (void *)(uintptr_t)c, task_name);
        if (q->worker_task) {
            kthread_bind(q->worker_task, c);
        }
    }
    return 0;
}

void net_queue_stop(void)
{
    s_workers_running = false;
    for (uint32_t c = 0; c < s_num_cores; c++) {
        up(&s_core_queues[c].sem);
    }
}

int net_queue_enqueue(uint32_t core_id, struct net_buf *buf)
{
    if (!buf) return -1;
    if (core_id >= s_num_cores) core_id = 0;

    struct net_core_queue *q = &s_core_queues[core_id];
    unsigned long flags;
    spin_lock_irqsave(&q->lock, flags);

    if (q->count >= NET_QUEUE_RING_SIZE) {
        q->dropped_packets++;
        spin_unlock_irqrestore(&q->lock, flags);
        net_buf_free(buf);
        return -1;
    }

    q->ring[q->tail] = buf;
    q->tail = (q->tail + 1) % NET_QUEUE_RING_SIZE;
    q->count++;
    q->enqueued_packets++;

    spin_unlock_irqrestore(&q->lock, flags);
    up(&q->sem);
    return 0;
}

struct net_buf *net_queue_dequeue(uint32_t core_id)
{
    if (core_id >= s_num_cores) core_id = 0;
    struct net_core_queue *q = &s_core_queues[core_id];

    unsigned long flags;
    spin_lock_irqsave(&q->lock, flags);

    if (q->count == 0) {
        spin_unlock_irqrestore(&q->lock, flags);
        return NULL;
    }

    struct net_buf *b = q->ring[q->head];
    q->ring[q->head] = NULL;
    q->head = (q->head + 1) % NET_QUEUE_RING_SIZE;
    q->count--;

    spin_unlock_irqrestore(&q->lock, flags);
    return b;
}

int net_queue_dispatch(struct net_buf *buf)
{
    if (!buf) return -1;

    uint32_t target_core = 0;
    if (buf->flow_hash == 0) {
        if (buf->netif && (buf->netif->flags & NETIF_FLAG_LOOPBACK)) {
            uint8_t ver = (buf->len > 0) ? ((buf->data[0] >> 4) & 0x0F) : 0;
            if (ver == 4 && buf->len >= sizeof(struct ip4_hdr)) {
                struct ip4_hdr *ip = (struct ip4_hdr *)buf->data;
                uint16_t sport = 0, dport = 0;
                if (ip->proto == IPPROTO_TCP && buf->len >= 24) {
                    memcpy(&sport, buf->data + 20, 2);
                    memcpy(&dport, buf->data + 22, 2);
                }
                buf->flow_hash = net_flow_hash(ip->src, ip->dst, sport, dport);
            } else if (ver == 6 && buf->len >= 40) {
                struct ip6_hdr *ip6 = (struct ip6_hdr *)buf->data;
                uint16_t sport = 0, dport = 0;
                if (ip6->nexthdr == IPPROTO_TCP && buf->len >= 44) {
                    memcpy(&sport, buf->data + 40, 2);
                    memcpy(&dport, buf->data + 42, 2);
                }
                buf->flow_hash = net_flow_hash6(&ip6->src, &ip6->dst, sport, dport);
            }
        } else if (buf->len >= sizeof(struct eth_hdr)) {
            struct eth_hdr *eth = (struct eth_hdr *)buf->data;
            uint16_t type = ntohs(eth->type);
            if (type == ETH_P_IP && buf->len >= (sizeof(struct eth_hdr) + sizeof(struct ip4_hdr))) {
                struct ip4_hdr *ip = (struct ip4_hdr *)(buf->data + sizeof(struct eth_hdr));
                uint16_t sport = 0, dport = 0;
                if (ip->proto == IPPROTO_TCP && buf->len >= (sizeof(struct eth_hdr) + 24)) {
                    memcpy(&sport, buf->data + sizeof(struct eth_hdr) + 20, 2);
                    memcpy(&dport, buf->data + sizeof(struct eth_hdr) + 22, 2);
                }
                buf->flow_hash = net_flow_hash(ip->src, ip->dst, sport, dport);
            } else if (type == ETH_P_IPV6 && buf->len >= (sizeof(struct eth_hdr) + 40)) {
                struct ip6_hdr *ip6 = (struct ip6_hdr *)(buf->data + sizeof(struct eth_hdr));
                uint16_t sport = 0, dport = 0;
                if (ip6->nexthdr == IPPROTO_TCP && buf->len >= (sizeof(struct eth_hdr) + 44)) {
                    memcpy(&sport, buf->data + sizeof(struct eth_hdr) + 40, 2);
                    memcpy(&dport, buf->data + sizeof(struct eth_hdr) + 42, 2);
                }
                buf->flow_hash = net_flow_hash6(&ip6->src, &ip6->dst, sport, dport);
            }
        }
    }

    if (buf->flow_hash != 0) {
        target_core = buf->flow_hash % s_num_cores;
    } else {
        target_core = smp_processor_id() % s_num_cores;
    }

    buf->core_id = (uint8_t)target_core;
    return net_queue_enqueue(target_core, buf);
}

uint32_t net_get_core_count(void)
{
    return s_num_cores;
}

struct net_core_queue *net_get_core_queue(uint32_t core_id)
{
    if (core_id >= s_num_cores) return NULL;
    return &s_core_queues[core_id];
}
