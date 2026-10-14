/*
 * CherryNet - Network Interface Management
 *
 * Copyright (c) 2026 ForgeStitch
 * SPDX-License-Identifier: MIT
 */

#ifndef _CHERRY_NET_IF_H_
#define _CHERRY_NET_IF_H_

#include "net_types.h"
#include "net_buf.h"

#ifdef __cplusplus
extern "C" {
#endif

#define NETIF_FLAG_UP         0x01U
#define NETIF_FLAG_BROADCAST  0x02U
#define NETIF_FLAG_LOOPBACK   0x04U
#define NETIF_FLAG_LINK_UP    0x08U
#define NETIF_FLAG_ETH        0x10U
#define NETIF_FLAG_RUNNING    0x20U

struct netif {
    char            name[8];        /* e.g. "eth0", "lo0" */
    uint16_t        flags;
    uint16_t        mtu;
    uint8_t         hwaddr[ETH_ALEN];
    
    /* IPv4 */
    struct ip4_addr ip4_addr;
    struct ip4_addr ip4_netmask;
    struct ip4_addr ip4_gw;

    /* IPv6 */
    struct ip6_addr ip6_ll;         /* fe80::... link-local */
    struct ip6_addr ip6_global;     /* Global unicast */
    uint8_t         ip6_prefix_len;

    /* Transmission function */
    int           (*send)(struct netif *nif, struct net_buf *buf);
    void           *priv;

    /* Traffic Statistics */
    uint32_t        rx_packets;
    uint32_t        tx_packets;
    uint64_t        rx_bytes;
    uint64_t        tx_bytes;
    uint32_t        rx_errors;
    uint32_t        tx_errors;
    uint32_t        rx_dropped;
    uint32_t        tx_dropped;

    struct netif   *next;
};

int           netif_init(void);
int           netif_register(struct netif *nif, const char *name, uint16_t mtu);
struct netif *netif_find(const char *name);
struct netif *netif_get_default(void);
void          netif_set_default(struct netif *nif);

int           netif_set_up(struct netif *nif);
int           netif_set_down(struct netif *nif);
int           netif_set_ip4(struct netif *nif, uint32_t ip, uint32_t netmask, uint32_t gw);
int           netif_set_ip6(struct netif *nif, const struct ip6_addr *ip6, uint8_t prefix_len);

struct netif *netif_route4(uint32_t dest_ip);
struct netif *netif_route6(const struct ip6_addr *dest_ip6);

int           netif_for_each(void *ctx, int (*cb)(struct netif *nif, void *ctx));

#ifdef __cplusplus
}
#endif

#endif /* _CHERRY_NET_IF_H_ */
