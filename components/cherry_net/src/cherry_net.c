/*
 * CherryNet - Master Subsystem Initialization
 *
 * Copyright (c) 2026 ForgeStitch
 * SPDX-License-Identifier: MIT
 */

#include "cherry_net.h"
#include "cherry_kal.h"

extern int net_loopback_init(void);

int cherry_net_init(void)
{
    pr_info("cherry_net: initializing multi-core tcp/ip network subsystem...\n");

    net_buf_pool_init();
    netif_init();
    arp_init();
    ip4_init();
    ip6_init();
    icmp4_init();
    icmp6_init();
    udp_init();
    tcp_init();

    /* Multi-core multi-queue worker engine */
    net_queue_init(2);

    /* Local Loopback Device lo0 (127.0.0.1, ::1) */
    net_loopback_init();

    /* Register BSD Socket Layer with CherryVFS */
    net_socket_init();

    /* Register VirtIO-Net PCI Driver with CherryPCI */
    virtio_net_init();

    pr_info("cherry_net: subsystem initialized successfully\n");
    return 0;
}
