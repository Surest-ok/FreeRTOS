/*
 * CherryNet - Multi-Core TCP Protocol Engine
 *
 * Copyright (c) 2026 ForgeStitch
 * SPDX-License-Identifier: MIT
 */

#include "net_tcp.h"
#include "net_ip4.h"
#include "net_ip6.h"
#include "net_queue.h"
#include "cherry_kal.h"
#include <string.h>

static struct tcp_pcb *s_tcp_pcbs = NULL;
static spinlock_t      s_tcp_lock = SPIN_LOCK_UNLOCKED;
static uint16_t        s_ephemeral_tcp = 49152;
static uint32_t        s_initial_seq = 100000;

int tcp_init(void)
{
    spin_lock_init(&s_tcp_lock);
    s_tcp_pcbs = NULL;
    return 0;
}

struct tcp_pcb *tcp_new(uint16_t family)
{
    struct tcp_pcb *pcb = (struct tcp_pcb *)kmalloc(sizeof(struct tcp_pcb), GFP_KERNEL);
    if (!pcb) return NULL;
    memset(pcb, 0, sizeof(*pcb));

    pcb->family = family ? family : AF_INET;
    pcb->state = TCP_STATE_CLOSED;
    pcb->core_id = (uint8_t)(smp_processor_id() % net_get_core_count());

    spin_lock_init(&pcb->lock);
    sema_init(&pcb->rx_sem, 0);
    sema_init(&pcb->conn_sem, 0);

    pcb->rx_ring = (uint8_t *)kmalloc(TCP_WND_SIZE, GFP_KERNEL);
    if (!pcb->rx_ring) {
        kfree(pcb);
        return NULL;
    }
    memset(pcb->rx_ring, 0, TCP_WND_SIZE);

    pcb->rcv_wnd = TCP_WND_SIZE;
    pcb->snd_wnd = TCP_WND_SIZE;

    unsigned long flags;
    spin_lock_irqsave(&s_tcp_lock, flags);
    pcb->next = s_tcp_pcbs;
    s_tcp_pcbs = pcb;
    spin_unlock_irqrestore(&s_tcp_lock, flags);

    return pcb;
}

int tcp_bind(struct tcp_pcb *pcb, const void *addr, socklen_t addrlen)
{
    if (!pcb || !addr) return -1;

    unsigned long flags;
    spin_lock_irqsave(&pcb->lock, flags);

    if (pcb->family == AF_INET && addrlen >= sizeof(struct sockaddr_in)) {
        const struct sockaddr_in *sin = (const struct sockaddr_in *)addr;
        pcb->local_port = ntohs(sin->sin_port);
        pcb->local_ip = sin->sin_addr.s_addr;
    } else if (pcb->family == AF_INET6 && addrlen >= sizeof(struct sockaddr_in6)) {
        const struct sockaddr_in6 *sin6 = (const struct sockaddr_in6 *)addr;
        pcb->local_port = ntohs(sin6->sin6_port);
        memcpy(&pcb->local_ip6, &sin6->sin6_addr, sizeof(struct ip6_addr));
    }

    if (pcb->local_port == 0) {
        pcb->local_port = s_ephemeral_tcp++;
        if (s_ephemeral_tcp > 65000) s_ephemeral_tcp = 49152;
    }

    spin_unlock_irqrestore(&pcb->lock, flags);
    return 0;
}

int tcp_listen(struct tcp_pcb *pcb, int backlog)
{
    if (!pcb) return -1;

    unsigned long flags;
    spin_lock_irqsave(&pcb->lock, flags);
    pcb->state = TCP_STATE_LISTEN;
    pcb->backlog = backlog > 0 ? backlog : 5;
    pcb->accept_queue_head = NULL;
    pcb->accept_queue_tail = NULL;
    spin_unlock_irqrestore(&pcb->lock, flags);

    return 0;
}

static int tcp_send_packet(struct tcp_pcb *pcb, uint16_t flags, const void *payload, size_t len)
{
    struct net_buf *buf = net_buf_alloc();
    if (!buf) return -1;

    size_t total = sizeof(struct tcp_hdr) + len;
    struct tcp_hdr *tcp = (struct tcp_hdr *)buf->data;
    buf->len = total;
    buf->tot_len = total;

    tcp->src_port = htons(pcb->local_port);
    tcp->dst_port = htons(pcb->remote_port);
    tcp->seqno = htonl(pcb->snd_nxt);
    tcp->ackno = (flags & TCP_ACK) ? htonl(pcb->rcv_nxt) : 0;
    tcp->flags = htons((uint16_t)((5 << 12) | (flags & 0x003F)));
    tcp->wnd = htons(TCP_WND_SIZE);
    tcp->chksum = 0;
    tcp->urgptr = 0;

    if (payload && len > 0) {
        memcpy(buf->data + sizeof(struct tcp_hdr), payload, len);
    }

    if (pcb->family == AF_INET) {
        tcp->chksum = ip4_pseudo_checksum(pcb->local_ip, pcb->remote_ip, IPPROTO_TCP, total, tcp);
        if (tcp->chksum == 0) tcp->chksum = 0xFFFF;
        return ip4_output(NULL, buf, pcb->local_ip, pcb->remote_ip, IPPROTO_TCP);
    } else {
        tcp->chksum = ip6_pseudo_checksum(&pcb->local_ip6, &pcb->remote_ip6, IPPROTO_TCP, total, tcp);
        if (tcp->chksum == 0) tcp->chksum = 0xFFFF;
        return ip6_output(NULL, buf, &pcb->local_ip6, &pcb->remote_ip6, IPPROTO_TCP);
    }
}

int tcp_connect(struct tcp_pcb *pcb, const void *addr, socklen_t addrlen)
{
    if (!pcb || !addr) return -1;

    unsigned long flags;
    spin_lock_irqsave(&pcb->lock, flags);

    if (pcb->family == AF_INET && addrlen >= sizeof(struct sockaddr_in)) {
        const struct sockaddr_in *sin = (const struct sockaddr_in *)addr;
        pcb->remote_port = ntohs(sin->sin_port);
        pcb->remote_ip = sin->sin_addr.s_addr;
        if (pcb->local_ip == 0) {
            struct netif *rt = netif_route4(pcb->remote_ip);
            if (rt) pcb->local_ip = rt->ip4_addr.addr;
        }
    } else if (pcb->family == AF_INET6 && addrlen >= sizeof(struct sockaddr_in6)) {
        const struct sockaddr_in6 *sin6 = (const struct sockaddr_in6 *)addr;
        pcb->remote_port = ntohs(sin6->sin6_port);
        memcpy(&pcb->remote_ip6, &sin6->sin6_addr, sizeof(struct ip6_addr));
        if (pcb->local_ip6.u32[0] == 0 && pcb->local_ip6.u32[1] == 0 &&
            pcb->local_ip6.u32[2] == 0 && pcb->local_ip6.u32[3] == 0) {
            struct netif *rt = netif_route6(&pcb->remote_ip6);
            if (rt) {
                if (rt->flags & NETIF_FLAG_LOOPBACK) {
                    memcpy(&pcb->local_ip6, &rt->ip6_global, sizeof(struct ip6_addr));
                } else {
                    memcpy(&pcb->local_ip6, &rt->ip6_ll, sizeof(struct ip6_addr));
                }
            }
        }
    }

    if (pcb->local_port == 0) {
        pcb->local_port = s_ephemeral_tcp++;
        if (s_ephemeral_tcp > 65000) s_ephemeral_tcp = 49152;
    }

    pcb->snd_nxt = s_initial_seq += 10000;
    pcb->snd_una = pcb->snd_nxt;
    pcb->state = TCP_STATE_SYN_SENT;

    spin_unlock_irqrestore(&pcb->lock, flags);

    /* Send SYN */
    tcp_send_packet(pcb, TCP_SYN, NULL, 0);
    pcb->snd_nxt++;

    /* Wait for 3-way handshake completion */
    long timeout = pcb->snd_timeout_ms ? (long)pcb->snd_timeout_ms : 3000;
    if (down_timeout(&pcb->conn_sem, timeout) != 0) {
        pcb->state = TCP_STATE_CLOSED;
        return -1;
    }

    return (pcb->state == TCP_STATE_ESTABLISHED) ? 0 : -1;
}

struct tcp_pcb *tcp_accept(struct tcp_pcb *pcb, void *addr, socklen_t *addrlen)
{
    if (!pcb || pcb->state != TCP_STATE_LISTEN) return NULL;

    /* Wait for child connection in accept queue */
    while (1) {
        unsigned long flags;
        spin_lock_irqsave(&pcb->lock, flags);

        if (pcb->accept_queue_head) {
            struct tcp_pcb *child = pcb->accept_queue_head;
            pcb->accept_queue_head = child->next_pending;
            if (!pcb->accept_queue_head) {
                pcb->accept_queue_tail = NULL;
            }
            child->next_pending = NULL;
            spin_unlock_irqrestore(&pcb->lock, flags);

            if (addr && addrlen) {
                if (child->family == AF_INET && *addrlen >= sizeof(struct sockaddr_in)) {
                    struct sockaddr_in *sin = (struct sockaddr_in *)addr;
                    sin->sin_family = AF_INET;
                    sin->sin_port = htons(child->remote_port);
                    sin->sin_addr.s_addr = child->remote_ip;
                    *addrlen = sizeof(struct sockaddr_in);
                } else if (child->family == AF_INET6 && *addrlen >= sizeof(struct sockaddr_in6)) {
                    struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *)addr;
                    sin6->sin6_family = AF_INET6;
                    sin6->sin6_port = htons(child->remote_port);
                    memcpy(&sin6->sin6_addr, &child->remote_ip6, sizeof(struct ip6_addr));
                    *addrlen = sizeof(struct sockaddr_in6);
                }
            }
            return child;
        }

        spin_unlock_irqrestore(&pcb->lock, flags);

        if (pcb->nonblock) return NULL;
        long timeout = pcb->rcv_timeout_ms ? (long)pcb->rcv_timeout_ms : -1;
        if (down_timeout(&pcb->conn_sem, timeout) != 0) {
            return NULL;
        }
    }
}

ssize_t tcp_send(struct tcp_pcb *pcb, const void *data, size_t len, int flags)
{
    (void)flags;
    if (!pcb || !data || len == 0) return -1;
    if (pcb->state != TCP_STATE_ESTABLISHED && pcb->state != TCP_STATE_CLOSE_WAIT) {
        return -1;
    }

    size_t remaining = len;
    const uint8_t *ptr = (const uint8_t *)data;

    while (remaining > 0) {
        size_t chunk = (remaining > TCP_MSS) ? TCP_MSS : remaining;
        int ret = tcp_send_packet(pcb, TCP_ACK | TCP_PSH, ptr, chunk);
        if (ret != 0) break;

        pcb->snd_nxt += chunk;
        ptr += chunk;
        remaining -= chunk;
    }

    return (ssize_t)(len - remaining);
}

ssize_t tcp_recv(struct tcp_pcb *pcb, void *buf, size_t len, int flags)
{
    (void)flags;
    if (!pcb || !buf || len == 0) return -1;

    while (1) {
        unsigned long iflags;
        spin_lock_irqsave(&pcb->lock, iflags);

        if (pcb->rx_avail > 0) {
            size_t copy_bytes = (len < pcb->rx_avail) ? len : pcb->rx_avail;
            for (size_t i = 0; i < copy_bytes; i++) {
                ((uint8_t *)buf)[i] = pcb->rx_ring[pcb->rx_head];
                pcb->rx_head = (pcb->rx_head + 1) % TCP_WND_SIZE;
            }
            pcb->rx_avail -= copy_bytes;
            spin_unlock_irqrestore(&pcb->lock, iflags);
            return (ssize_t)copy_bytes;
        }

        if (pcb->state != TCP_STATE_ESTABLISHED) {
            /* Connection terminating or closed */
            spin_unlock_irqrestore(&pcb->lock, iflags);
            return 0;
        }

        spin_unlock_irqrestore(&pcb->lock, iflags);

        if (pcb->nonblock) return -1;
        long timeout = pcb->rcv_timeout_ms ? (long)pcb->rcv_timeout_ms : -1;
        if (down_timeout(&pcb->rx_sem, timeout) != 0) {
            return -1;
        }
    }
}

int tcp_shutdown(struct tcp_pcb *pcb, int how)
{
    (void)how;
    if (!pcb) return -1;

    unsigned long flags;
    spin_lock_irqsave(&pcb->lock, flags);
    if (pcb->state == TCP_STATE_ESTABLISHED) {
        pcb->state = TCP_STATE_FIN_WAIT_1;
        spin_unlock_irqrestore(&pcb->lock, flags);
        tcp_send_packet(pcb, TCP_FIN | TCP_ACK, NULL, 0);
        pcb->snd_nxt++;
        return 0;
    }
    spin_unlock_irqrestore(&pcb->lock, flags);
    return 0;
}

void tcp_close(struct tcp_pcb *pcb)
{
    if (!pcb) return;

    if (pcb->state == TCP_STATE_ESTABLISHED) {
        tcp_shutdown(pcb, 2);
    }

    unsigned long flags;
    spin_lock_irqsave(&s_tcp_lock, flags);

    struct tcp_pcb **curr = &s_tcp_pcbs;
    while (*curr) {
        if (*curr == pcb) {
            *curr = pcb->next;
            break;
        }
        curr = &(*curr)->next;
    }
    spin_unlock_irqrestore(&s_tcp_lock, flags);

    if (pcb->rx_ring) {
        kfree(pcb->rx_ring);
        pcb->rx_ring = NULL;
    }

    sema_destroy(&pcb->rx_sem);
    sema_destroy(&pcb->conn_sem);
    kfree(pcb);
}

int tcp_input(struct netif *nif, struct net_buf *buf, uint16_t family,
              const void *src_ip, const void *dst_ip)
{
    (void)nif;
    if (!buf || buf->len < sizeof(struct tcp_hdr)) {
        if (buf) net_buf_free(buf);
        return -1;
    }

    struct tcp_hdr *tcp = (struct tcp_hdr *)buf->data;
    uint16_t src_port = ntohs(tcp->src_port);
    uint16_t dst_port = ntohs(tcp->dst_port);
    uint32_t seq = ntohl(tcp->seqno);
    uint32_t ack = ntohl(tcp->ackno);
    uint16_t tcph_len = TCPH_HDRLEN(tcp);
    uint16_t flags = TCPH_FLAGS(tcp);

    if (tcph_len < sizeof(struct tcp_hdr) || buf->len < tcph_len) {
        net_buf_free(buf);
        return -1;
    }

    size_t payload_len = buf->len - tcph_len;
    const uint8_t *payload = buf->data + tcph_len;

    unsigned long gflags;
    spin_lock_irqsave(&s_tcp_lock, gflags);

    struct tcp_pcb *pcb = NULL;
    struct tcp_pcb *listen_pcb = NULL;

    for (struct tcp_pcb *p = s_tcp_pcbs; p != NULL; p = p->next) {
        if (p->family != family) continue;

        if (p->local_port == dst_port && p->remote_port == src_port) {
            pcb = p;
            break;
        }
        if (p->state == TCP_STATE_LISTEN && p->local_port == dst_port) {
            listen_pcb = p;
        }
    }

    if (!pcb && listen_pcb && (flags & TCP_SYN)) {
        /* Incoming connection on listening port */
        struct tcp_pcb *child = tcp_new(family);
        if (child) {
            child->local_port = dst_port;
            child->remote_port = src_port;
            if (family == AF_INET) {
                child->local_ip = *(const uint32_t *)dst_ip;
                child->remote_ip = *(const uint32_t *)src_ip;
            } else {
                memcpy(&child->local_ip6, dst_ip, sizeof(struct ip6_addr));
                memcpy(&child->remote_ip6, src_ip, sizeof(struct ip6_addr));
            }
            child->rcv_nxt = seq + 1;
            child->snd_nxt = s_initial_seq += 10000;
            child->snd_una = child->snd_nxt;
            child->state = TCP_STATE_SYN_RCVD;

            /* Link to listening parent's accept queue */
            if (!listen_pcb->accept_queue_head) {
                listen_pcb->accept_queue_head = child;
                listen_pcb->accept_queue_tail = child;
            } else {
                listen_pcb->accept_queue_tail->next_pending = child;
                listen_pcb->accept_queue_tail = child;
            }

            up(&listen_pcb->conn_sem);

            spin_unlock_irqrestore(&s_tcp_lock, gflags);

            /* Reply SYN-ACK */
            tcp_send_packet(child, TCP_SYN | TCP_ACK, NULL, 0);
            child->snd_nxt++;
            net_buf_free(buf);
            return 0;
        }
    }

    if (!pcb) {
        spin_unlock_irqrestore(&s_tcp_lock, gflags);
        net_buf_free(buf);
        return 0;
    }

    spin_lock(&pcb->lock);
    spin_unlock_irqrestore(&s_tcp_lock, gflags);

    if (pcb->state == TCP_STATE_SYN_SENT && (flags & TCP_SYN) && (flags & TCP_ACK)) {
        pcb->rcv_nxt = seq + 1;
        pcb->snd_una = ack;
        pcb->state = TCP_STATE_ESTABLISHED;
        tcp_send_packet(pcb, TCP_ACK, NULL, 0);
        up(&pcb->conn_sem);
    } else if (pcb->state == TCP_STATE_SYN_RCVD && (flags & TCP_ACK)) {
        pcb->state = TCP_STATE_ESTABLISHED;
        pcb->snd_una = ack;
        up(&pcb->conn_sem);
    } else if (pcb->state == TCP_STATE_ESTABLISHED || pcb->state == TCP_STATE_CLOSE_WAIT) {
        if (payload_len > 0 && pcb->rx_ring) {
            size_t space = TCP_WND_SIZE - pcb->rx_avail;
            size_t to_copy = (payload_len < space) ? payload_len : space;

            for (size_t i = 0; i < to_copy; i++) {
                pcb->rx_ring[pcb->rx_tail] = payload[i];
                pcb->rx_tail = (pcb->rx_tail + 1) % TCP_WND_SIZE;
            }
            pcb->rx_avail += to_copy;
            pcb->rcv_nxt = seq + payload_len;

            /* Send ACK */
            tcp_send_packet(pcb, TCP_ACK, NULL, 0);
            up(&pcb->rx_sem);
        }

        if (flags & TCP_FIN) {
            pcb->state = TCP_STATE_CLOSE_WAIT;
            pcb->rcv_nxt = seq + 1;
            tcp_send_packet(pcb, TCP_ACK, NULL, 0);
            up(&pcb->rx_sem);
        }
    } else if (pcb->state == TCP_STATE_FIN_WAIT_1 && (flags & TCP_ACK)) {
        pcb->state = TCP_STATE_FIN_WAIT_2;
        if (flags & TCP_FIN) {
            pcb->rcv_nxt = seq + 1;
            tcp_send_packet(pcb, TCP_ACK, NULL, 0);
            pcb->state = TCP_STATE_CLOSED;
            up(&pcb->rx_sem);
        }
    } else if (pcb->state == TCP_STATE_FIN_WAIT_2 && (flags & TCP_FIN)) {
        pcb->rcv_nxt = seq + 1;
        tcp_send_packet(pcb, TCP_ACK, NULL, 0);
        pcb->state = TCP_STATE_CLOSED;
        up(&pcb->rx_sem);
    }

    spin_unlock(&pcb->lock);
    net_buf_free(buf);
    return 0;
}
