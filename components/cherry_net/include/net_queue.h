/*
 * CherryNet - Multi-Core Multi-Queue Subsystem
 *
 * Copyright (c) 2026 ForgeStitch
 * SPDX-License-Identifier: MIT
 */

#ifndef _CHERRY_NET_QUEUE_H_
#define _CHERRY_NET_QUEUE_H_

#include "net_types.h"
#include "net_buf.h"
#include "cherry_kal.h"

#ifdef __cplusplus
extern "C" {
#endif

#define NET_MAX_CORES         8
#define NET_QUEUE_RING_SIZE   128

struct net_core_queue {
    uint32_t            core_id;
    struct task_struct *worker_task;
    struct semaphore    sem;
    spinlock_t          lock;
    struct net_buf     *ring[NET_QUEUE_RING_SIZE];
    uint32_t            head;
    uint32_t            tail;
    uint32_t            count;

    /* Per-core metrics */
    uint64_t            processed_packets;
    uint64_t            enqueued_packets;
    uint64_t            dropped_packets;
};

int      net_queue_init(uint32_t num_cores);
void     net_queue_stop(void);
uint32_t net_flow_hash(uint32_t s_ip, uint32_t d_ip, uint16_t s_port, uint16_t d_port);
uint32_t net_flow_hash6(const struct ip6_addr *s_ip6, const struct ip6_addr *d_ip6, uint16_t s_port, uint16_t d_port);

int      net_queue_dispatch(struct net_buf *buf);
int      net_queue_enqueue(uint32_t core_id, struct net_buf *buf);
struct net_buf *net_queue_dequeue(uint32_t core_id);

uint32_t net_get_core_count(void);
struct net_core_queue *net_get_core_queue(uint32_t core_id);

#ifdef __cplusplus
}
#endif

#endif /* _CHERRY_NET_QUEUE_H_ */
