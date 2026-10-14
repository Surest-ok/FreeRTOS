/*
 * CherryNet - IPv6 Protocol & Neighbor Discovery Layer
 *
 * Copyright (c) 2026 ForgeStitch
 * SPDX-License-Identifier: MIT
 */

#include "net_ip6.h"
#include "net_eth.h"
#include "net_icmp.h"
#include "net_udp.h"
#include "net_tcp.h"
#include "cherry_kal.h"
#include <string.h>

#define NDP_TABLE_SIZE 16

struct ndp_entry {
    struct ip6_addr ip6;
    uint8_t         mac[ETH_ALEN];
    bool            valid;
};

static struct ndp_entry s_ndp_table[NDP_TABLE_SIZE];
static spinlock_t       s_ndp_lock = SPIN_LOCK_UNLOCKED;

void ip6_addr_create_linklocal(struct ip6_addr *addr, const uint8_t *mac)
{
    if (!addr || !mac) return;
    memset(addr, 0, sizeof(*addr));
    addr->u8[0] = 0xFE;
    addr->u8[1] = 0x80;
    addr->u8[8] = mac[0] ^ 0x02;
    addr->u8[9] = mac[1];
    addr->u8[10] = mac[2];
    addr->u8[11] = 0xFF;
    addr->u8[12] = 0xFE;
    addr->u8[13] = mac[3];
    addr->u8[14] = mac[4];
    addr->u8[15] = mac[5];
}

bool ip6_addr_is_multicast(const struct ip6_addr *addr)
{
    return addr && (addr->u8[0] == 0xFF);
}

bool ip6_addr_cmp(const struct ip6_addr *a, const struct ip6_addr *b)
{
    return (a && b) && (memcmp(a->u8, b->u8, 16) == 0);
}

uint16_t ip6_pseudo_checksum(const struct ip6_addr *src, const struct ip6_addr *dst,
                             uint8_t proto, uint32_t len, const void *data)
{
    uint32_t sum = 0;
    const uint8_t *s = src ? src->u8 : NULL;
    const uint8_t *d = dst ? dst->u8 : NULL;

    if (s) {
        for (int i = 0; i < 16; i += 2) {
            sum += ((uint32_t)s[i] << 8) | (uint32_t)s[i + 1];
        }
    }
    if (d) {
        for (int i = 0; i < 16; i += 2) {
            sum += ((uint32_t)d[i] << 8) | (uint32_t)d[i + 1];
        }
    }

    sum += ((len >> 16) & 0xFFFF);
    sum += (len & 0xFFFF);
    sum += (uint32_t)proto;

    const uint8_t *p = (const uint8_t *)data;
    size_t dlen = len;
    while (dlen > 1) {
        sum += ((uint32_t)p[0] << 8) | (uint32_t)p[1];
        p += 2;
        dlen -= 2;
    }
    if (dlen == 1) {
        sum += (uint32_t)p[0] << 8;
    }

    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }
    return htons((uint16_t)(~sum));
}

int ndp_init(void)
{
    spin_lock_init(&s_ndp_lock);
    memset(s_ndp_table, 0, sizeof(s_ndp_table));
    return 0;
}

int ndp_lookup(const struct ip6_addr *ip6, uint8_t *mac_out)
{
    if (!ip6 || !mac_out) return -1;
    unsigned long flags;
    spin_lock_irqsave(&s_ndp_lock, flags);

    for (int i = 0; i < NDP_TABLE_SIZE; i++) {
        if (s_ndp_table[i].valid && ip6_addr_cmp(&s_ndp_table[i].ip6, ip6)) {
            memcpy(mac_out, s_ndp_table[i].mac, ETH_ALEN);
            spin_unlock_irqrestore(&s_ndp_lock, flags);
            return 0;
        }
    }

    spin_unlock_irqrestore(&s_ndp_lock, flags);
    return -1;
}

void ndp_update(const struct ip6_addr *ip6, const uint8_t *mac)
{
    if (!ip6 || !mac) return;
    unsigned long flags;
    spin_lock_irqsave(&s_ndp_lock, flags);

    for (int i = 0; i < NDP_TABLE_SIZE; i++) {
        if (s_ndp_table[i].valid && ip6_addr_cmp(&s_ndp_table[i].ip6, ip6)) {
            memcpy(s_ndp_table[i].mac, mac, ETH_ALEN);
            spin_unlock_irqrestore(&s_ndp_lock, flags);
            return;
        }
    }

    for (int i = 0; i < NDP_TABLE_SIZE; i++) {
        if (!s_ndp_table[i].valid) {
            s_ndp_table[i].valid = true;
            memcpy(&s_ndp_table[i].ip6, ip6, sizeof(struct ip6_addr));
            memcpy(s_ndp_table[i].mac, mac, ETH_ALEN);
            spin_unlock_irqrestore(&s_ndp_lock, flags);
            return;
        }
    }

    s_ndp_table[0].valid = true;
    memcpy(&s_ndp_table[0].ip6, ip6, sizeof(struct ip6_addr));
    memcpy(s_ndp_table[0].mac, mac, ETH_ALEN);
    spin_unlock_irqrestore(&s_ndp_lock, flags);
}

int ndp_send_na(struct netif *nif, const struct ip6_addr *target, const struct ip6_addr *dst)
{
    if (!nif || !target) return -1;
    struct net_buf *buf = net_buf_alloc();
    if (!buf) return -1;

    struct ndp_na_hdr *na = (struct ndp_na_hdr *)buf->data;
    buf->len = sizeof(struct ndp_na_hdr);
    buf->tot_len = buf->len;

    na->type = ICMP6_TYPE_NA;
    na->code = 0;
    na->chksum = 0;
    na->flags = htonl(0x60000000U); /* Solicited (bit 30) + Override (bit 29) */
    memcpy(&na->target, target, sizeof(struct ip6_addr));
    na->opt_type = 2; /* Target Link-Layer Address */
    na->opt_len = 1;  /* In units of 8 octets */
    memcpy(na->opt_mac, nif->hwaddr, ETH_ALEN);

    struct ip6_addr src;
    memcpy(&src, &nif->ip6_ll, sizeof(struct ip6_addr));
    na->chksum = ip6_pseudo_checksum(&src, dst, IPPROTO_ICMPV6, buf->len, na);

    return ip6_output(nif, buf, &src, dst, IPPROTO_ICMPV6);
}

int ip6_init(void)
{
    return ndp_init();
}

int ip6_input(struct netif *nif, struct net_buf *buf)
{
    if (!nif || !buf || buf->len < IP6_HLEN) {
        if (buf) net_buf_free(buf);
        return -1;
    }

    struct ip6_hdr *ip6 = (struct ip6_hdr *)buf->data;
    uint32_t ver = (ntohl(ip6->vtc_fl) >> 28) & 0x0F;
    if (ver != 6) {
        net_buf_free(buf);
        return -1;
    }

    uint16_t plen = ntohs(ip6->payload_len);
    if (buf->len < (size_t)(IP6_HLEN + plen)) {
        net_buf_free(buf);
        return -1;
    }

    uint8_t nexthdr = ip6->nexthdr;
    struct ip6_addr src, dst;
    memcpy(&src, &ip6->src, sizeof(src));
    memcpy(&dst, &ip6->dst, sizeof(dst));

    net_buf_pull(buf, IP6_HLEN);

    if (nexthdr == IPPROTO_ICMPV6) {
        return icmp6_input(nif, buf, &src, &dst);
    } else if (nexthdr == IPPROTO_UDP) {
        return udp_input(nif, buf, AF_INET6, &src, &dst);
    } else if (nexthdr == IPPROTO_TCP) {
        return tcp_input(nif, buf, AF_INET6, &src, &dst);
    }

    net_buf_free(buf);
    return 0;
}

int ip6_output(struct netif *nif, struct net_buf *buf, const struct ip6_addr *src,
               const struct ip6_addr *dst, uint8_t nexthdr)
{
    if (!buf || !dst) return -1;
    if (!nif) {
        nif = netif_route6(dst);
    }
    if (!nif) {
        net_buf_free(buf);
        return -1;
    }

    struct ip6_hdr *ip6 = (struct ip6_hdr *)net_buf_push(buf, IP6_HLEN);
    if (!ip6) {
        net_buf_free(buf);
        return -1;
    }

    ip6->vtc_fl = htonl(0x60000000U); /* IPv6 version 6 */
    ip6->payload_len = htons((uint16_t)(buf->len - IP6_HLEN));
    ip6->nexthdr = nexthdr;
    ip6->hoplimit = 64;

    if (src) {
        memcpy(&ip6->src, src, sizeof(struct ip6_addr));
    } else {
        memcpy(&ip6->src, &nif->ip6_ll, sizeof(struct ip6_addr));
    }
    memcpy(&ip6->dst, dst, sizeof(struct ip6_addr));

    if (nif->flags & NETIF_FLAG_LOOPBACK) {
        buf->netif = nif;
        if (nif->send) return nif->send(nif, buf);
        net_buf_free(buf);
        return 0;
    }

    uint8_t dst_mac[ETH_ALEN];
    if (ip6_addr_is_multicast(dst)) {
        /* RFC 2464: Ethernet multicast mapping: 33:33:xx:xx:xx:xx */
        dst_mac[0] = 0x33;
        dst_mac[1] = 0x33;
        dst_mac[2] = dst->u8[12];
        dst_mac[3] = dst->u8[13];
        dst_mac[4] = dst->u8[14];
        dst_mac[5] = dst->u8[15];
    } else if (ndp_lookup(dst, dst_mac) != 0) {
        /* Fallback multicast until discovered */
        dst_mac[0] = 0x33;
        dst_mac[1] = 0x33;
        dst_mac[2] = dst->u8[12];
        dst_mac[3] = dst->u8[13];
        dst_mac[4] = dst->u8[14];
        dst_mac[5] = dst->u8[15];
    }

    return eth_output(nif, buf, dst_mac, ETH_P_IPV6);
}
