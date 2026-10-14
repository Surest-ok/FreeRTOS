/*
 * CherryNet - UDP Protocol Layer
 *
 * Copyright (c) 2026 ForgeStitch
 * SPDX-License-Identifier: MIT
 */

#ifndef _CHERRY_NET_UDP_H_
#define _CHERRY_NET_UDP_H_

#include "net_types.h"
#include "net_buf.h"
#include "net_if.h"
#include "cherry_kal.h"

#ifdef __cplusplus
extern "C" {
#endif

struct udp_hdr {
    uint16_t src_port;
    uint16_t dst_port;
    uint16_t len;
    uint16_t chksum;
} __attribute__((packed));

struct udp_pcb {
    uint16_t         family;       /* AF_INET or AF_INET6 */
    uint16_t         local_port;
    uint16_t         remote_port;

    uint32_t         local_ip;     /* IPv4 */
    uint32_t         remote_ip;

    struct ip6_addr  local_ip6;    /* IPv6 */
    struct ip6_addr  remote_ip6;

    struct semaphore rx_sem;
    spinlock_t       lock;
    struct net_buf  *rx_head;
    struct net_buf  *rx_tail;
    uint32_t         rx_count;
    bool             nonblock;
    uint32_t         rcv_timeout_ms;

    struct udp_pcb  *next;
};

int             udp_init(void);
struct udp_pcb *udp_new(uint16_t family);
int             udp_bind(struct udp_pcb *pcb, const void *addr, socklen_t addrlen);
int             udp_connect(struct udp_pcb *pcb, const void *addr, socklen_t addrlen);
ssize_t         udp_sendto(struct udp_pcb *pcb, const void *data, size_t len, int flags,
                           const void *dest_addr, socklen_t addrlen);
ssize_t         udp_recvfrom(struct udp_pcb *pcb, void *buf, size_t len, int flags,
                             void *src_addr, socklen_t *addrlen);
void            udp_close(struct udp_pcb *pcb);

int             udp_input(struct netif *nif, struct net_buf *buf, uint16_t family,
                          const void *src_ip, const void *dst_ip);

#ifdef __cplusplus
}
#endif

#endif /* _CHERRY_NET_UDP_H_ */
