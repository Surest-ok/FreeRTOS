/*
 * CherryNet - UDP Protocol Layer
 *
 * Copyright (c) 2026 ForgeStitch
 * SPDX-License-Identifier: MIT
 */

#include "net_udp.h"
#include "net_ip4.h"
#include "net_ip6.h"
#include <string.h>

static struct udp_pcb *s_udp_pcbs = NULL;
static spinlock_t      s_udp_lock = SPIN_LOCK_UNLOCKED;
static uint16_t        s_ephemeral_port = 49152;

int udp_init(void)
{
    spin_lock_init(&s_udp_lock);
    s_udp_pcbs = NULL;
    return 0;
}

struct udp_pcb *udp_new(uint16_t family)
{
    struct udp_pcb *pcb = (struct udp_pcb *)kmalloc(sizeof(struct udp_pcb), GFP_KERNEL);
    if (!pcb) return NULL;
    memset(pcb, 0, sizeof(*pcb));

    pcb->family = family ? family : AF_INET;
    spin_lock_init(&pcb->lock);
    sema_init(&pcb->rx_sem, 0);

    unsigned long flags;
    spin_lock_irqsave(&s_udp_lock, flags);
    pcb->next = s_udp_pcbs;
    s_udp_pcbs = pcb;
    spin_unlock_irqrestore(&s_udp_lock, flags);

    return pcb;
}

int udp_bind(struct udp_pcb *pcb, const void *addr, socklen_t addrlen)
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
        pcb->local_port = s_ephemeral_port++;
        if (s_ephemeral_port > 65000) s_ephemeral_port = 49152;
    }

    spin_unlock_irqrestore(&pcb->lock, flags);
    return 0;
}

int udp_connect(struct udp_pcb *pcb, const void *addr, socklen_t addrlen)
{
    if (!pcb || !addr) return -1;

    unsigned long flags;
    spin_lock_irqsave(&pcb->lock, flags);

    if (pcb->family == AF_INET && addrlen >= sizeof(struct sockaddr_in)) {
        const struct sockaddr_in *sin = (const struct sockaddr_in *)addr;
        pcb->remote_port = ntohs(sin->sin_port);
        pcb->remote_ip = sin->sin_addr.s_addr;
    } else if (pcb->family == AF_INET6 && addrlen >= sizeof(struct sockaddr_in6)) {
        const struct sockaddr_in6 *sin6 = (const struct sockaddr_in6 *)addr;
        pcb->remote_port = ntohs(sin6->sin6_port);
        memcpy(&pcb->remote_ip6, &sin6->sin6_addr, sizeof(struct ip6_addr));
    }

    if (pcb->local_port == 0) {
        pcb->local_port = s_ephemeral_port++;
        if (s_ephemeral_port > 65000) s_ephemeral_port = 49152;
    }

    spin_unlock_irqrestore(&pcb->lock, flags);
    return 0;
}

ssize_t udp_sendto(struct udp_pcb *pcb, const void *data, size_t len, int flags,
                   const void *dest_addr, socklen_t addrlen)
{
    (void)flags;
    if (!pcb || !data || len == 0) return -1;

    uint16_t dst_port = 0;
    uint32_t dst_ip4 = 0;
    struct ip6_addr dst_ip6;
    memset(&dst_ip6, 0, sizeof(dst_ip6));

    if (dest_addr) {
        if (pcb->family == AF_INET && addrlen >= sizeof(struct sockaddr_in)) {
            const struct sockaddr_in *sin = (const struct sockaddr_in *)dest_addr;
            dst_port = ntohs(sin->sin_port);
            dst_ip4 = sin->sin_addr.s_addr;
        } else if (pcb->family == AF_INET6 && addrlen >= sizeof(struct sockaddr_in6)) {
            const struct sockaddr_in6 *sin6 = (const struct sockaddr_in6 *)dest_addr;
            dst_port = ntohs(sin6->sin6_port);
            memcpy(&dst_ip6, &sin6->sin6_addr, sizeof(struct ip6_addr));
        }
    } else {
        dst_port = pcb->remote_port;
        dst_ip4 = pcb->remote_ip;
        memcpy(&dst_ip6, &pcb->remote_ip6, sizeof(struct ip6_addr));
    }

    if (dst_port == 0) return -1;
    if (pcb->local_port == 0) {
        pcb->local_port = s_ephemeral_port++;
        if (s_ephemeral_port > 65000) s_ephemeral_port = 49152;
    }

    struct net_buf *buf = net_buf_alloc();
    if (!buf) return -1;

    size_t total = sizeof(struct udp_hdr) + len;
    struct udp_hdr *udp = (struct udp_hdr *)buf->data;
    buf->len = total;
    buf->tot_len = total;

    udp->src_port = htons(pcb->local_port);
    udp->dst_port = htons(dst_port);
    udp->len = htons((uint16_t)total);
    udp->chksum = 0;

    memcpy(buf->data + sizeof(struct udp_hdr), data, len);

    if (pcb->family == AF_INET) {
        udp->chksum = ip4_pseudo_checksum(pcb->local_ip, dst_ip4, IPPROTO_UDP, total, udp);
        if (udp->chksum == 0) udp->chksum = 0xFFFF;
        int ret = ip4_output(NULL, buf, pcb->local_ip, dst_ip4, IPPROTO_UDP);
        return ret == 0 ? (ssize_t)len : -1;
    } else {
        udp->chksum = ip6_pseudo_checksum(&pcb->local_ip6, &dst_ip6, IPPROTO_UDP, total, udp);
        if (udp->chksum == 0) udp->chksum = 0xFFFF;
        int ret = ip6_output(NULL, buf, &pcb->local_ip6, &dst_ip6, IPPROTO_UDP);
        return ret == 0 ? (ssize_t)len : -1;
    }
}

ssize_t udp_recvfrom(struct udp_pcb *pcb, void *buf, size_t len, int flags,
                     void *src_addr, socklen_t *addrlen)
{
    (void)flags;
    if (!pcb || !buf) return -1;

    if (pcb->nonblock) {
        if (down_trylock(&pcb->rx_sem) != 0) return -1;
    } else {
        long timeout = pcb->rcv_timeout_ms ? (long)pcb->rcv_timeout_ms : -1;
        if (down_timeout(&pcb->rx_sem, timeout) != 0) return -1;
    }

    unsigned long iflags;
    spin_lock_irqsave(&pcb->lock, iflags);

    struct net_buf *nb = pcb->rx_head;
    if (!nb) {
        spin_unlock_irqrestore(&pcb->lock, iflags);
        return 0;
    }

    pcb->rx_head = nb->next;
    if (!pcb->rx_head) pcb->rx_tail = NULL;
    pcb->rx_count--;

    spin_unlock_irqrestore(&pcb->lock, iflags);

    /* Read header metadata */
    uint16_t src_port = nb->src_port;
    size_t payload_len = nb->len > sizeof(struct udp_hdr) ? (nb->len - sizeof(struct udp_hdr)) : 0;
    size_t copy_bytes = (len < payload_len) ? len : payload_len;

    memcpy(buf, nb->data + sizeof(struct udp_hdr), copy_bytes);

    if (src_addr && addrlen) {
        if (pcb->family == AF_INET && *addrlen >= sizeof(struct sockaddr_in)) {
            struct sockaddr_in *sin = (struct sockaddr_in *)src_addr;
            sin->sin_family = AF_INET;
            sin->sin_port = htons(src_port);
            sin->sin_addr.s_addr = nb->src_ip;
            *addrlen = sizeof(struct sockaddr_in);
        } else if (pcb->family == AF_INET6 && *addrlen >= sizeof(struct sockaddr_in6)) {
            struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *)src_addr;
            sin6->sin6_family = AF_INET6;
            sin6->sin6_port = htons(src_port);
            memcpy(&sin6->sin6_addr, &nb->src_ip6, sizeof(struct ip6_addr));
            *addrlen = sizeof(struct sockaddr_in6);
        }
    }

    net_buf_free(nb);
    return (ssize_t)copy_bytes;
}

void udp_close(struct udp_pcb *pcb)
{
    if (!pcb) return;

    unsigned long flags;
    spin_lock_irqsave(&s_udp_lock, flags);

    struct udp_pcb **curr = &s_udp_pcbs;
    while (*curr) {
        if (*curr == pcb) {
            *curr = pcb->next;
            break;
        }
        curr = &(*curr)->next;
    }
    spin_unlock_irqrestore(&s_udp_lock, flags);

    /* Flush receive queue */
    struct net_buf *nb = pcb->rx_head;
    while (nb) {
        struct net_buf *next = nb->next;
        net_buf_free(nb);
        nb = next;
    }

    sema_destroy(&pcb->rx_sem);
    kfree(pcb);
}

int udp_input(struct netif *nif, struct net_buf *buf, uint16_t family,
              const void *src_ip, const void *dst_ip)
{
    (void)nif;
    (void)src_ip;
    (void)dst_ip;

    if (!buf || buf->len < sizeof(struct udp_hdr)) {
        if (buf) net_buf_free(buf);
        return -1;
    }

    struct udp_hdr *udp = (struct udp_hdr *)buf->data;
    uint16_t dst_port = ntohs(udp->dst_port);

    unsigned long flags;
    spin_lock_irqsave(&s_udp_lock, flags);

    struct udp_pcb *target = NULL;
    for (struct udp_pcb *p = s_udp_pcbs; p != NULL; p = p->next) {
        if (p->family == family && p->local_port == dst_port) {
            target = p;
            break;
        }
    }

    if (!target) {
        spin_unlock_irqrestore(&s_udp_lock, flags);
        net_buf_free(buf);
        return 0;
    }

    buf->src_port = ntohs(udp->src_port);
    buf->dst_port = dst_port;
    if (family == AF_INET && src_ip) {
        memcpy(&buf->src_ip, src_ip, 4);
    } else if (family == AF_INET6 && src_ip) {
        memcpy(&buf->src_ip6, src_ip, 16);
    }

    spin_lock(&target->lock);
    buf->next = NULL;
    if (!target->rx_head) {
        target->rx_head = buf;
        target->rx_tail = buf;
    } else {
        target->rx_tail->next = buf;
        target->rx_tail = buf;
    }
    target->rx_count++;
    spin_unlock(&target->lock);
    spin_unlock_irqrestore(&s_udp_lock, flags);

    up(&target->rx_sem);
    return 0;
}
