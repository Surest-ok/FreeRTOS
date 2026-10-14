/*
 * CherryNet - Loopback Interface lo0 (127.0.0.1, ::1)
 *
 * Copyright (c) 2026 ForgeStitch
 * SPDX-License-Identifier: MIT
 */

#include "net_if.h"
#include "net_queue.h"
#include "net_eth.h"
#include <string.h>

static struct netif s_loopback_netif;

static int loopback_send(struct netif *nif, struct net_buf *buf)
{
    if (!nif || !buf) return -1;
    nif->tx_packets++;
    nif->tx_bytes += buf->len;

    /* Loopback directly to dispatch/input */
    nif->rx_packets++;
    nif->rx_bytes += buf->len;
    buf->netif = nif;

    return net_queue_dispatch(buf);
}

int net_loopback_init(void)
{
    memset(&s_loopback_netif, 0, sizeof(s_loopback_netif));
    s_loopback_netif.flags = NETIF_FLAG_UP | NETIF_FLAG_LOOPBACK | NETIF_FLAG_LINK_UP;
    s_loopback_netif.mtu = 16384;
    s_loopback_netif.send = loopback_send;

    /* 127.0.0.1 / 8 */
    s_loopback_netif.ip4_addr.addr = IP4_ADDR(127, 0, 0, 1);
    s_loopback_netif.ip4_netmask.addr = IP4_ADDR(255, 0, 0, 0);
    s_loopback_netif.ip4_gw.addr = IP4_ADDR(127, 0, 0, 1);

    /* ::1 */
    s_loopback_netif.ip6_global.u8[15] = 1;
    s_loopback_netif.ip6_ll.u8[15] = 1;
    s_loopback_netif.ip6_prefix_len = 128;

    return netif_register(&s_loopback_netif, "lo0", 16384);
}
