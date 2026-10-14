/*
 * CherryNet - ICMPv4 & ICMPv6 Protocol Engine
 *
 * Copyright (c) 2026 ForgeStitch
 * SPDX-License-Identifier: MIT
 */

#include "net_icmp.h"
#include "net_ip4.h"
#include "net_ip6.h"
#include "cherry_kal.h"
#include <string.h>

/* Ping Response Notification Tracker */
struct ping_reply_slot {
    bool     active;
    uint16_t id;
    uint16_t seq;
    uint32_t rtt_us;
    struct semaphore sem;
};

static struct ping_reply_slot s_ping_slots[4];
static spinlock_t             s_ping_lock = SPIN_LOCK_UNLOCKED;

int icmp4_init(void)
{
    spin_lock_init(&s_ping_lock);
    for (int i = 0; i < 4; i++) {
        s_ping_slots[i].active = false;
        sema_init(&s_ping_slots[i].sem, 0);
    }
    return 0;
}

int icmp6_init(void)
{
    return 0;
}

int icmp4_input(struct netif *nif, struct net_buf *buf, uint32_t src, uint32_t dst)
{
    if (!nif || !buf || buf->len < sizeof(struct icmp4_hdr)) {
        if (buf) net_buf_free(buf);
        return -1;
    }

    struct icmp4_hdr *icmp = (struct icmp4_hdr *)buf->data;

    if (icmp->type == ICMP4_TYPE_ECHO_REQUEST) {
        /* Echo Request -> Send Echo Reply */
        struct net_buf *reply = net_buf_alloc();
        if (reply) {
            size_t copy_len = buf->len;
            if (copy_len > NET_BUF_DATA_SIZE - NET_BUF_HEADROOM - 40) {
                copy_len = NET_BUF_DATA_SIZE - NET_BUF_HEADROOM - 40;
            }
            memcpy(reply->data, buf->data, copy_len);
            reply->len = copy_len;
            reply->tot_len = copy_len;

            struct icmp4_hdr *r = (struct icmp4_hdr *)reply->data;
            r->type = ICMP4_TYPE_ECHO_REPLY;
            r->chksum = 0;
            r->chksum = net_checksum(r, reply->len);

            ip4_output(nif, reply, dst, src, IPPROTO_ICMP);
        }
    } else if (icmp->type == ICMP4_TYPE_ECHO_REPLY) {
        uint16_t id = ntohs(icmp->id);
        uint16_t seq = ntohs(icmp->seq);

        unsigned long flags;
        spin_lock_irqsave(&s_ping_lock, flags);
        for (int i = 0; i < 4; i++) {
            if (s_ping_slots[i].active && s_ping_slots[i].id == id && s_ping_slots[i].seq == seq) {
                up(&s_ping_slots[i].sem);
                break;
            }
        }
        spin_unlock_irqrestore(&s_ping_lock, flags);
    }

    net_buf_free(buf);
    return 0;
}

int icmp4_send_echo(struct netif *nif, uint32_t dst, uint16_t id, uint16_t seq,
                    const void *payload, size_t len)
{
    struct net_buf *buf = net_buf_alloc();
    if (!buf) return -1;

    size_t total = sizeof(struct icmp4_hdr) + len;
    struct icmp4_hdr *icmp = (struct icmp4_hdr *)buf->data;
    buf->len = total;
    buf->tot_len = total;

    icmp->type = ICMP4_TYPE_ECHO_REQUEST;
    icmp->code = 0;
    icmp->id = htons(id);
    icmp->seq = htons(seq);
    icmp->chksum = 0;

    if (payload && len > 0) {
        memcpy(buf->data + sizeof(struct icmp4_hdr), payload, len);
    }

    icmp->chksum = net_checksum(icmp, total);

    /* Register ping slot for reply */
    int slot = -1;
    unsigned long flags;
    spin_lock_irqsave(&s_ping_lock, flags);
    for (int i = 0; i < 4; i++) {
        if (!s_ping_slots[i].active) {
            s_ping_slots[i].active = true;
            s_ping_slots[i].id = id;
            s_ping_slots[i].seq = seq;
            slot = i;
            break;
        }
    }
    spin_unlock_irqrestore(&s_ping_lock, flags);

    ip4_output(nif, buf, 0, dst, IPPROTO_ICMP);

    if (slot >= 0) {
        int ret = down_timeout(&s_ping_slots[slot].sem, 1000);
        spin_lock_irqsave(&s_ping_lock, flags);
        s_ping_slots[slot].active = false;
        spin_unlock_irqrestore(&s_ping_lock, flags);
        return ret == 0 ? 0 : -1;
    }
    return 0;
}

int icmp6_input(struct netif *nif, struct net_buf *buf, const struct ip6_addr *src, const struct ip6_addr *dst)
{
    if (!nif || !buf || buf->len < 4) {
        if (buf) net_buf_free(buf);
        return -1;
    }

    uint8_t type = buf->data[0];

    if (type == ICMP6_TYPE_NS) {
        if (buf->len >= sizeof(struct ndp_ns_hdr)) {
            struct ndp_ns_hdr *ns = (struct ndp_ns_hdr *)buf->data;
            struct ip6_addr target;
            memcpy(&target, &ns->target, sizeof(target));
            ndp_update(src, ns->opt_mac);
            ndp_send_na(nif, &target, src);
        }
    } else if (type == ICMP6_TYPE_NA) {
        if (buf->len >= sizeof(struct ndp_na_hdr)) {
            struct ndp_na_hdr *na = (struct ndp_na_hdr *)buf->data;
            struct ip6_addr target;
            memcpy(&target, &na->target, sizeof(target));
            ndp_update(&target, na->opt_mac);
        }
    } else if (type == ICMP6_TYPE_ECHO_REQUEST) {
        if (buf->len >= sizeof(struct icmp6_hdr)) {
            struct net_buf *reply = net_buf_alloc();
            if (reply) {
                size_t copy_len = buf->len;
                memcpy(reply->data, buf->data, copy_len);
                reply->len = copy_len;
                reply->tot_len = copy_len;

                struct icmp6_hdr *r = (struct icmp6_hdr *)reply->data;
                r->type = ICMP6_TYPE_ECHO_REPLY;
                r->chksum = 0;

                r->chksum = ip6_pseudo_checksum(dst, src, IPPROTO_ICMPV6, reply->len, r);

                ip6_output(nif, reply, dst, src, IPPROTO_ICMPV6);
            }
        }
    } else if (type == ICMP6_TYPE_ECHO_REPLY) {
        if (buf->len >= sizeof(struct icmp6_hdr)) {
            struct icmp6_hdr *icmp = (struct icmp6_hdr *)buf->data;
            uint16_t id = ntohs(icmp->id);
            uint16_t seq = ntohs(icmp->seq);

            unsigned long flags;
            spin_lock_irqsave(&s_ping_lock, flags);
            for (int i = 0; i < 4; i++) {
                if (s_ping_slots[i].active && s_ping_slots[i].id == id && s_ping_slots[i].seq == seq) {
                    up(&s_ping_slots[i].sem);
                    break;
                }
            }
            spin_unlock_irqrestore(&s_ping_lock, flags);
        }
    }

    net_buf_free(buf);
    return 0;
}

int icmp6_send_echo(struct netif *nif, const struct ip6_addr *dst, uint16_t id,
                    uint16_t seq, const void *payload, size_t len)
{
    struct net_buf *buf = net_buf_alloc();
    if (!buf) return -1;

    size_t total = sizeof(struct icmp6_hdr) + len;
    struct icmp6_hdr *icmp = (struct icmp6_hdr *)buf->data;
    buf->len = total;
    buf->tot_len = total;

    icmp->type = ICMP6_TYPE_ECHO_REQUEST;
    icmp->code = 0;
    icmp->id = htons(id);
    icmp->seq = htons(seq);
    icmp->chksum = 0;

    if (payload && len > 0) {
        memcpy(buf->data + sizeof(struct icmp6_hdr), payload, len);
    }

    struct ip6_addr src;
    if (nif) {
        if (nif->flags & NETIF_FLAG_LOOPBACK) {
            memcpy(&src, &nif->ip6_global, sizeof(src));
        } else {
            memcpy(&src, &nif->ip6_ll, sizeof(src));
        }
    } else {
        memset(&src, 0, sizeof(src));
    }
    icmp->chksum = ip6_pseudo_checksum(&src, dst, IPPROTO_ICMPV6, total, icmp);

    int slot = -1;
    unsigned long flags;
    spin_lock_irqsave(&s_ping_lock, flags);
    for (int i = 0; i < 4; i++) {
        if (!s_ping_slots[i].active) {
            s_ping_slots[i].active = true;
            s_ping_slots[i].id = id;
            s_ping_slots[i].seq = seq;
            slot = i;
            break;
        }
    }
    spin_unlock_irqrestore(&s_ping_lock, flags);

    ip6_output(nif, buf, &src, dst, IPPROTO_ICMPV6);

    if (slot >= 0) {
        int ret = down_timeout(&s_ping_slots[slot].sem, 1000);
        spin_lock_irqsave(&s_ping_lock, flags);
        s_ping_slots[slot].active = false;
        spin_unlock_irqrestore(&s_ping_lock, flags);
        return ret == 0 ? 0 : -1;
    }
    return 0;
}
