/*
 * CherryNet - ifconfig Interface Inspection and Configuration Tool
 *
 * Copyright (c) 2026 ForgeStitch
 * SPDX-License-Identifier: MIT
 */

#include "net_tools.h"
#include "net_if.h"
#include "arpa/inet.h"
#include "cherry_kal.h"
#include <string.h>

static int print_single_if(struct netif *nif, void *ctx)
{
    (void)ctx;
    char flags_str[64];
    flags_str[0] = '\0';
    if (nif->flags & NETIF_FLAG_UP) strcat(flags_str, "UP,");
    if (nif->flags & NETIF_FLAG_BROADCAST) strcat(flags_str, "BROADCAST,");
    if (nif->flags & NETIF_FLAG_RUNNING) strcat(flags_str, "RUNNING,");
    if (nif->flags & NETIF_FLAG_LOOPBACK) strcat(flags_str, "LOOPBACK,");
    if (flags_str[0] != '\0') {
        flags_str[strlen(flags_str) - 1] = '\0'; /* Remove trailing comma */
    }

    pr_info("cherry_net: [ifconfig] %s: flags=<%s> mtu %u\n", nif->name, flags_str, nif->mtu);

    if (nif->ip4_addr.addr) {
        char ip[16], mask[16];
        struct in_addr in_ip, in_mask;
        in_ip.s_addr = nif->ip4_addr.addr;
        in_mask.s_addr = nif->ip4_netmask.addr;
        strcpy(ip, inet_ntoa(in_ip));
        strcpy(mask, inet_ntoa(in_mask));
        pr_info("cherry_net: [ifconfig]         inet %s netmask %s\n", ip, mask);
    }

    if (nif->ip6_ll.u8[0] != 0) {
        pr_info("cherry_net: [ifconfig]         inet6 fe80::%02x%02x:%02x%02x:%02x%02x%02x:%02x%02x prefixlen 64\n",
                nif->ip6_ll.u8[8], nif->ip6_ll.u8[9], nif->ip6_ll.u8[10], nif->ip6_ll.u8[11],
                nif->ip6_ll.u8[12], nif->ip6_ll.u8[13], nif->ip6_ll.u8[14], nif->ip6_ll.u8[15]);
    }

    if (nif->flags & NETIF_FLAG_ETH) {
        pr_info("cherry_net: [ifconfig]         ether %02x:%02x:%02x:%02x:%02x:%02x\n",
                nif->hwaddr[0], nif->hwaddr[1], nif->hwaddr[2],
                nif->hwaddr[3], nif->hwaddr[4], nif->hwaddr[5]);
    }

    pr_info("cherry_net: [ifconfig]         rx packets %u bytes %llu errors %u dropped %u\n",
            nif->rx_packets, (unsigned long long)nif->rx_bytes, nif->rx_errors, nif->rx_dropped);
    pr_info("cherry_net: [ifconfig]         tx packets %u bytes %llu errors %u dropped %u\n",
            nif->tx_packets, (unsigned long long)nif->tx_bytes, nif->tx_errors, nif->tx_dropped);

    return 0;
}

int ifconfig_print(void)
{
    return netif_for_each(NULL, print_single_if);
}

int ifconfig_configure(const char *name, const char *ip_str, const char *mask_str)
{
    struct netif *nif = netif_find(name);
    if (!nif) {
        pr_err("cherry_net: [ifconfig] interface %s not found\n", name);
        return -1;
    }

    uint32_t ip = ip_str ? inet_addr(ip_str) : nif->ip4_addr.addr;
    uint32_t mask = mask_str ? inet_addr(mask_str) : nif->ip4_netmask.addr;
    netif_set_ip4(nif, ip, mask, nif->ip4_gw.addr);
    netif_set_up(nif);

    pr_info("cherry_net: [ifconfig] %s configured with ip %s netmask %s\n",
            name, ip_str ? ip_str : "", mask_str ? mask_str : "");
    return 0;
}
