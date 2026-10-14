/*
 * CherryNet - IPv4 Protocol Layer
 *
 * Copyright (c) 2026 ForgeStitch
 * SPDX-License-Identifier: MIT
 */

#include "net_ip4.h"
#include "net_eth.h"
#include "net_icmp.h"
#include "net_udp.h"
#include "net_tcp.h"
#include <string.h>

static uint16_t s_ip_id = 1;

uint16_t net_checksum(const void *data, size_t len)
{
    const uint8_t *b = (const uint8_t *)data;
    uint32_t sum = 0;

    while (len > 1) {
        sum += ((uint32_t)b[0] << 8) | (uint32_t)b[1];
        b += 2;
        len -= 2;
    }
    if (len == 1) {
        sum += (uint32_t)b[0] << 8;
    }

    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }
    return htons((uint16_t)(~sum));
}

struct ip4_pseudo_hdr {
    uint32_t src;
    uint32_t dst;
    uint8_t  zero;
    uint8_t  proto;
    uint16_t len;
} __attribute__((packed));

uint16_t ip4_pseudo_checksum(uint32_t src, uint32_t dst, uint8_t proto, uint16_t len, const void *data)
{
    struct ip4_pseudo_hdr pseudo;
    pseudo.src = src;
    pseudo.dst = dst;
    pseudo.zero = 0;
    pseudo.proto = proto;
    pseudo.len = htons(len);

    uint32_t sum = 0;
    const uint8_t *p = (const uint8_t *)&pseudo;
    for (size_t i = 0; i < sizeof(pseudo); i += 2) {
        sum += ((uint32_t)p[i] << 8) | (uint32_t)p[i + 1];
    }

    const uint8_t *d = (const uint8_t *)data;
    size_t dlen = len;
    while (dlen > 1) {
        sum += ((uint32_t)d[0] << 8) | (uint32_t)d[1];
        d += 2;
        dlen -= 2;
    }
    if (dlen == 1) {
        sum += (uint32_t)d[0] << 8;
    }

    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }
    return htons((uint16_t)(~sum));
}

int ip4_init(void)
{
    return 0;
}

int ip4_input(struct netif *nif, struct net_buf *buf)
{
    if (!nif || !buf || buf->len < sizeof(struct ip4_hdr)) {
        if (buf) net_buf_free(buf);
        return -1;
    }

    struct ip4_hdr *ip = (struct ip4_hdr *)buf->data;
    if (IP4_HDR_V(ip) != 4) {
        net_buf_free(buf);
        return -1;
    }

    uint32_t hlen = IP4_HDR_HL(ip) * 4;
    if (buf->len < hlen) {
        net_buf_free(buf);
        return -1;
    }

    uint16_t tot_len = ntohs(ip->len);
    if (buf->len > tot_len) {
        buf->len = tot_len;
        buf->tot_len = tot_len;
    }

    uint8_t proto = ip->proto;
    uint32_t src = ip->src;
    uint32_t dst = ip->dst;

    /* Pull IP header */
    net_buf_pull(buf, hlen);

    if (proto == IPPROTO_ICMP) {
        return icmp4_input(nif, buf, src, dst);
    } else if (proto == IPPROTO_UDP) {
        return udp_input(nif, buf, AF_INET, &src, &dst);
    } else if (proto == IPPROTO_TCP) {
        return tcp_input(nif, buf, AF_INET, &src, &dst);
    }

    net_buf_free(buf);
    return 0;
}

int ip4_output(struct netif *nif, struct net_buf *buf, uint32_t src, uint32_t dst, uint8_t proto)
{
    if (!buf) return -1;
    if (!nif) {
        nif = netif_route4(dst);
    }
    if (!nif) {
        net_buf_free(buf);
        return -1;
    }

    struct ip4_hdr *ip = (struct ip4_hdr *)net_buf_push(buf, sizeof(struct ip4_hdr));
    if (!ip) {
        net_buf_free(buf);
        return -1;
    }

    ip->v_hl = 0x45; /* IPv4, 5 words (20 bytes) */
    ip->tos = 0;
    ip->len = htons((uint16_t)buf->len);
    ip->id = htons(s_ip_id++);
    ip->offset = htons(0x4000); /* Don't fragment */
    ip->ttl = 64;
    ip->proto = proto;
    ip->chksum = 0;
    ip->src = src ? src : nif->ip4_addr.addr;
    ip->dst = dst;
    ip->chksum = net_checksum(ip, sizeof(struct ip4_hdr));

    if (nif->flags & NETIF_FLAG_LOOPBACK) {
        buf->netif = nif;
        if (nif->send) return nif->send(nif, buf);
        net_buf_free(buf);
        return 0;
    }

    /* Ethernet hardware MAC resolution */
    uint8_t dst_mac[ETH_ALEN];
    if (dst == INADDR_BROADCAST || (dst & ~nif->ip4_netmask.addr) == ~nif->ip4_netmask.addr) {
        memset(dst_mac, 0xFF, ETH_ALEN);
    } else if (arp_lookup(dst, dst_mac) != 0) {
        arp_request(nif, dst);
        /* If route is gateway */
        if (nif->ip4_gw.addr && nif->ip4_gw.addr != dst) {
            if (arp_lookup(nif->ip4_gw.addr, dst_mac) != 0) {
                arp_request(nif, nif->ip4_gw.addr);
                memset(dst_mac, 0xFF, ETH_ALEN);
            }
        } else {
            memset(dst_mac, 0xFF, ETH_ALEN);
        }
    }

    return eth_output(nif, buf, dst_mac, ETH_P_IP);
}
