/*
 * CherryNet - ICMPv4 & ICMPv6 Protocol Engine
 *
 * Copyright (c) 2026 ForgeStitch
 * SPDX-License-Identifier: MIT
 */

#ifndef _CHERRY_NET_ICMP_H_
#define _CHERRY_NET_ICMP_H_

#include "net_types.h"
#include "net_buf.h"
#include "net_if.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ICMP4_TYPE_ECHO_REPLY   0
#define ICMP4_TYPE_DEST_UNREACH 3
#define ICMP4_TYPE_ECHO_REQUEST 8

struct icmp4_hdr {
    uint8_t  type;
    uint8_t  code;
    uint16_t chksum;
    uint16_t id;
    uint16_t seq;
} __attribute__((packed));

#define ICMP6_TYPE_DEST_UNREACH 1
#define ICMP6_TYPE_ECHO_REQUEST 128
#define ICMP6_TYPE_ECHO_REPLY   129

struct icmp6_hdr {
    uint8_t  type;
    uint8_t  code;
    uint16_t chksum;
    uint16_t id;
    uint16_t seq;
} __attribute__((packed));

int icmp4_init(void);
int icmp4_input(struct netif *nif, struct net_buf *buf, uint32_t src, uint32_t dst);
int icmp4_send_echo(struct netif *nif, uint32_t dst, uint16_t id, uint16_t seq, const void *payload, size_t len);

int icmp6_init(void);
int icmp6_input(struct netif *nif, struct net_buf *buf, const struct ip6_addr *src, const struct ip6_addr *dst);
int icmp6_send_echo(struct netif *nif, const struct ip6_addr *dst, uint16_t id, uint16_t seq, const void *payload, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* _CHERRY_NET_ICMP_H_ */
