/*
 * CherryNet - Ethernet II Framing and ARP Engine
 *
 * Copyright (c) 2026 ForgeStitch
 * SPDX-License-Identifier: MIT
 */

#ifndef _CHERRY_NET_ETH_H_
#define _CHERRY_NET_ETH_H_

#include "net_types.h"
#include "net_buf.h"
#include "net_if.h"

#ifdef __cplusplus
extern "C" {
#endif

struct eth_hdr {
    uint8_t  dst[ETH_ALEN];
    uint8_t  src[ETH_ALEN];
    uint16_t type;
} __attribute__((packed));

#define ARP_OP_REQUEST  1
#define ARP_OP_REPLY    2

struct arp_hdr {
    uint16_t htype;
    uint16_t ptype;
    uint8_t  hlen;
    uint8_t  plen;
    uint16_t opcode;
    uint8_t  sender_mac[ETH_ALEN];
    uint32_t sender_ip;
    uint8_t  target_mac[ETH_ALEN];
    uint32_t target_ip;
} __attribute__((packed));

int  arp_init(void);
int  arp_lookup(uint32_t ip, uint8_t *mac_out);
void arp_update(uint32_t ip, const uint8_t *mac);
int  arp_request(struct netif *nif, uint32_t target_ip);
int  arp_input(struct netif *nif, struct net_buf *buf);

int  eth_input(struct netif *nif, struct net_buf *buf);
int  eth_output(struct netif *nif, struct net_buf *buf, const uint8_t *dst_mac, uint16_t ethertype);

#ifdef __cplusplus
}
#endif

#endif /* _CHERRY_NET_ETH_H_ */
