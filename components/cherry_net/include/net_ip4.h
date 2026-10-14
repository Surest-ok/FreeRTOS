/*
 * CherryNet - IPv4 Protocol Layer
 *
 * Copyright (c) 2026 ForgeStitch
 * SPDX-License-Identifier: MIT
 */

#ifndef _CHERRY_NET_IP4_H_
#define _CHERRY_NET_IP4_H_

#include "net_types.h"
#include "net_buf.h"
#include "net_if.h"

#ifdef __cplusplus
extern "C" {
#endif

struct ip4_hdr {
    uint8_t  v_hl;       /* Version (4 bits) + Internet header length (4 bits) */
    uint8_t  tos;        /* Type of service */
    uint16_t len;        /* Total length */
    uint16_t id;         /* Identification */
    uint16_t offset;     /* Flags (3 bits) + Fragment offset (13 bits) */
    uint8_t  ttl;        /* Time to live */
    uint8_t  proto;      /* Protocol */
    uint16_t chksum;     /* Header checksum */
    uint32_t src;        /* Source address */
    uint32_t dst;        /* Destination address */
} __attribute__((packed));

#define IP4_HDR_V(hdr)      (((hdr)->v_hl >> 4) & 0x0F)
#define IP4_HDR_HL(hdr)     ((hdr)->v_hl & 0x0F)

uint16_t net_checksum(const void *data, size_t len);
uint16_t ip4_pseudo_checksum(uint32_t src, uint32_t dst, uint8_t proto, uint16_t len, const void *data);

int      ip4_init(void);
int      ip4_input(struct netif *nif, struct net_buf *buf);
int      ip4_output(struct netif *nif, struct net_buf *buf, uint32_t src, uint32_t dst, uint8_t proto);

#ifdef __cplusplus
}
#endif

#endif /* _CHERRY_NET_IP4_H_ */
