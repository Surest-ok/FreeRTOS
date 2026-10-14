/*
 * CherryNet - Multi-Core TCP Protocol Engine
 *
 * Copyright (c) 2026 ForgeStitch
 * SPDX-License-Identifier: MIT
 */

#ifndef _CHERRY_NET_TCP_H_
#define _CHERRY_NET_TCP_H_

#include "net_types.h"
#include "net_buf.h"
#include "net_if.h"
#include "cherry_kal.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TCP_HLEN            20
#define TCP_WND_SIZE        32768
#define TCP_MSS             1460

/* TCP Flags */
#define TCP_FIN             0x01U
#define TCP_SYN             0x02U
#define TCP_RST             0x04U
#define TCP_PSH             0x08U
#define TCP_ACK             0x10U
#define TCP_URG             0x20U

struct tcp_hdr {
    uint16_t src_port;
    uint16_t dst_port;
    uint32_t seqno;
    uint32_t ackno;
    uint16_t flags;         /* Data offset (4b), reserved (6b), flags (6b) */
    uint16_t wnd;
    uint16_t chksum;
    uint16_t urgptr;
} __attribute__((packed));

#define TCPH_HDRLEN(hdr)    ((ntohs((hdr)->flags) >> 12) * 4)
#define TCPH_FLAGS(hdr)     (ntohs((hdr)->flags) & 0x003FU)

enum tcp_state {
    TCP_STATE_CLOSED = 0,
    TCP_STATE_LISTEN,
    TCP_STATE_SYN_SENT,
    TCP_STATE_SYN_RCVD,
    TCP_STATE_ESTABLISHED,
    TCP_STATE_FIN_WAIT_1,
    TCP_STATE_FIN_WAIT_2,
    TCP_STATE_CLOSE_WAIT,
    TCP_STATE_CLOSING,
    TCP_STATE_LAST_ACK,
    TCP_STATE_TIME_WAIT,
};

struct tcp_pcb {
    uint16_t          family;       /* AF_INET or AF_INET6 */
    enum tcp_state    state;
    uint8_t           core_id;      /* Bound worker CPU core */
    bool              nonblock;
    bool              nodelay;
    uint32_t          rcv_timeout_ms;
    uint32_t          snd_timeout_ms;

    /* IPv4 */
    uint32_t          local_ip;
    uint32_t          remote_ip;

    /* IPv6 */
    struct ip6_addr   local_ip6;
    struct ip6_addr   remote_ip6;

    uint16_t          local_port;
    uint16_t          remote_port;

    /* Sequence Tracking */
    uint32_t          snd_una;
    uint32_t          snd_nxt;
    uint32_t          snd_wnd;
    uint32_t          rcv_nxt;
    uint32_t          rcv_wnd;

    /* Receive Stream Ring Buffer */
    uint8_t          *rx_ring;
    uint32_t          rx_head;
    uint32_t          rx_tail;
    uint32_t          rx_avail;

    /* Synchronization */
    struct semaphore  rx_sem;
    struct semaphore  conn_sem;
    spinlock_t        lock;

    /* Listen backlog queue */
    int               backlog;
    struct tcp_pcb   *accept_queue_head;
    struct tcp_pcb   *accept_queue_tail;
    struct tcp_pcb   *next_pending;

    struct tcp_pcb   *next;
};

int             tcp_init(void);
struct tcp_pcb *tcp_new(uint16_t family);
int             tcp_bind(struct tcp_pcb *pcb, const void *addr, socklen_t addrlen);
int             tcp_listen(struct tcp_pcb *pcb, int backlog);
struct tcp_pcb *tcp_accept(struct tcp_pcb *pcb, void *addr, socklen_t *addrlen);
int             tcp_connect(struct tcp_pcb *pcb, const void *addr, socklen_t addrlen);
ssize_t         tcp_send(struct tcp_pcb *pcb, const void *data, size_t len, int flags);
ssize_t         tcp_recv(struct tcp_pcb *pcb, void *buf, size_t len, int flags);
int             tcp_shutdown(struct tcp_pcb *pcb, int how);
void            tcp_close(struct tcp_pcb *pcb);

int             tcp_input(struct netif *nif, struct net_buf *buf, uint16_t family,
                          const void *src_ip, const void *dst_ip);

#ifdef __cplusplus
}
#endif

#endif /* _CHERRY_NET_TCP_H_ */
