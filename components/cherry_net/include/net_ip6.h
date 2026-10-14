/*
 * CherryNet - IPv6 Protocol & Neighbor Discovery Layer
 *
 * Copyright (c) 2026 ForgeStitch
 * SPDX-License-Identifier: MIT
 */

#ifndef _CHERRY_NET_IP6_H_
#define _CHERRY_NET_IP6_H_

#include "net_types.h"
#include "net_buf.h"
#include "net_if.h"

#ifdef __cplusplus
extern "C" {
#endif

#define IP6_HLEN 40

struct ip6_hdr {
    uint32_t vtc_fl;       /* Version (4b), Traffic Class (8b), Flow Label (20b) */
    uint16_t payload_len;  /* Payload length */
    uint8_t  nexthdr;      /* Next Header protocol */
    uint8_t  hoplimit;     /* Hop Limit (TTL) */
    struct ip6_addr src;   /* 128-bit Source Address */
    struct ip6_addr dst;   /* 128-bit Destination Address */
} __attribute__((packed));

/* Neighbor Discovery Messages (RFC 4861) */
#define ICMP6_TYPE_NS      135  /* Neighbor Solicitation */
#define ICMP6_TYPE_NA      136  /* Neighbor Advertisement */

struct ndp_ns_hdr {
    uint8_t         type;
    uint8_t         code;
    uint16_t        chksum;
    uint32_t        reserved;
    struct ip6_addr target;
    uint8_t         opt_type;
    uint8_t         opt_len;
    uint8_t         opt_mac[ETH_ALEN];
} __attribute__((packed));

struct ndp_na_hdr {
    uint8_t         type;
    uint8_t         code;
    uint16_t        chksum;
    uint32_t        flags; /* Router (bit 31), Solicited (bit 30), Override (bit 29) */
    struct ip6_addr target;
    uint8_t         opt_type;
    uint8_t         opt_len;
    uint8_t         opt_mac[ETH_ALEN];
} __attribute__((packed));

void     ip6_addr_create_linklocal(struct ip6_addr *addr, const uint8_t *mac);
bool     ip6_addr_is_multicast(const struct ip6_addr *addr);
bool     ip6_addr_cmp(const struct ip6_addr *a, const struct ip6_addr *b);
uint16_t ip6_pseudo_checksum(const struct ip6_addr *src, const struct ip6_addr *dst,
                             uint8_t proto, uint32_t len, const void *data);

int      ndp_init(void);
int      ndp_lookup(const struct ip6_addr *ip6, uint8_t *mac_out);
void     ndp_update(const struct ip6_addr *ip6, const uint8_t *mac);
int      ndp_send_na(struct netif *nif, const struct ip6_addr *target, const struct ip6_addr *dst);

int      ip6_init(void);
int      ip6_input(struct netif *nif, struct net_buf *buf);
int      ip6_output(struct netif *nif, struct net_buf *buf, const struct ip6_addr *src,
                    const struct ip6_addr *dst, uint8_t nexthdr);

#ifdef __cplusplus
}
#endif

#endif /* _CHERRY_NET_IP6_H_ */
