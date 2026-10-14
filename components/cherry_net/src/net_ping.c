/*
 * CherryNet - ICMPv4 & ICMPv6 Ping Diagnostic Tool
 *
 * Copyright (c) 2026 ForgeStitch
 * SPDX-License-Identifier: MIT
 */

#include "net_tools.h"
#include "net_icmp.h"
#include "net_if.h"
#include "arpa/inet.h"
#include "cherry_kal.h"
#include <string.h>

int ping_run(const char *target, int count, int timeout_ms)
{
    if (!target) return -1;
    if (count <= 0) count = 3;
    if (timeout_ms <= 0) timeout_ms = 1000;

    bool is_ipv6 = (strchr(target, ':') != NULL);
    struct ip4_addr dst_ip4;
    struct ip6_addr dst_ip6;

    if (is_ipv6) {
        if (inet_pton(AF_INET6, target, &dst_ip6) != 1) {
            pr_err("cherry_net: [ping] invalid ipv6 address %s\n", target);
            return -1;
        }
    } else {
        dst_ip4.addr = inet_addr(target);
        if (dst_ip4.addr == INADDR_BROADCAST && strcmp(target, "255.255.255.255") != 0) {
            pr_err("cherry_net: [ping] invalid ipv4 address %s\n", target);
            return -1;
        }
    }

    struct netif *nif = is_ipv6 ? netif_route6(&dst_ip6) : netif_route4(dst_ip4.addr);
    if (!nif) {
        pr_err("cherry_net: [ping] no route to host %s\n", target);
        return -1;
    }

    pr_info("cherry_net: [ping] ping %s (%s) 56 data bytes\n", target, nif->name);

    int received = 0;
    uint16_t id = 0x1234;

    for (int seq = 1; seq <= count; seq++) {
        uint64_t t0 = ktime_get_ns();
        int ret = 0;

        char payload[32];
        memset(payload, 0x41, sizeof(payload));

        if (is_ipv6) {
            ret = icmp6_send_echo(nif, &dst_ip6, id, (uint16_t)seq, payload, sizeof(payload));
        } else {
            ret = icmp4_send_echo(nif, dst_ip4.addr, id, (uint16_t)seq, payload, sizeof(payload));
        }

        uint64_t t1 = ktime_get_ns();
        uint64_t us = (t1 - t0) / 1000ULL;
        uint32_t ms = (uint32_t)(us / 1000ULL);
        uint32_t dec_ms = (uint32_t)((us % 1000ULL) / 100);

        if (ret == 0) {
            received++;
            pr_info("cherry_net: [ping] 64 bytes from %s: icmp_seq=%d ttl=64 time=%u.%u ms\n",
                    target, seq, ms, dec_ms);
        } else {
            pr_info("cherry_net: [ping] request timeout for icmp_seq %d\n", seq);
        }
    }

    int loss = ((count - received) * 100) / count;
    pr_info("cherry_net: [ping] --- %s ping statistics ---\n", target);
    pr_info("cherry_net: [ping] %d packets transmitted, %d received, %d%% packet loss\n",
            count, received, loss);

    return (received > 0) ? 0 : -1;
}
