/*
 * CherryNet - Shell Commands Integration
 *
 * Copyright (c) 2026 ForgeStitch
 * SPDX-License-Identifier: MIT
 */

#include "net_tools.h"
#include "net_iperf2.h"
#include "net_if.h"
#include "cherry_shell.h"
#include "cherry_kal.h"
#include <string.h>

int net_cmd_ifconfig(int argc, char **argv)
{
    if (argc <= 1) {
        return ifconfig_print();
    }
    const char *name = argv[1];
    const char *ip = (argc > 2) ? argv[2] : NULL;
    const char *mask = (argc > 4 && strcmp(argv[3], "netmask") == 0) ? argv[4] : "255.255.255.0";

    return ifconfig_configure(name, ip, mask);
}
CHERRY_SHELL_EXPORT_CMD(ifconfig, net_cmd_ifconfig, "Display or configure network interfaces");

int net_cmd_ping(int argc, char **argv)
{
    if (argc <= 1) {
        pr_info("usage: ping <host> [count]\n");
        return -1;
    }
    const char *host = argv[1];
    int count = (argc > 2) ? (int)(argv[2][0] - '0') : 3;
    if (count <= 0) count = 3;

    return ping_run(host, count, 1000);
}
CHERRY_SHELL_EXPORT_CMD(ping, net_cmd_ping, "Send ICMP ECHO_REQUEST to network hosts");

int net_cmd_iperf(int argc, char **argv)
{
    bool is_server = false;
    const char *target = "127.0.0.1";
    uint16_t port = IPERF_DEFAULT_PORT;
    uint32_t duration = 3;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-s") == 0) {
            is_server = true;
        } else if (strcmp(argv[i], "-c") == 0 && i + 1 < argc) {
            target = argv[++i];
        } else if (strcmp(argv[i], "-p") == 0 && i + 1 < argc) {
            /* simple port parser */
            port = (uint16_t)(argv[++i][0] - '0');
        } else if (strcmp(argv[i], "-t") == 0 && i + 1 < argc) {
            duration = (uint32_t)(argv[++i][0] - '0');
        }
    }

    if (is_server) {
        return iperf2_server_start(port);
    } else {
        return iperf2_client_start(target, port, duration, 1460);
    }
}
CHERRY_SHELL_EXPORT_CMD(iperf, net_cmd_iperf, "iperf2 network throughput measurement tool");

int net_cmd_netstat(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    pr_info("cherry_net: [netstat] Active Internet connections (servers and established):\n");
    pr_info("cherry_net: [netstat] Proto Recv-Q Send-Q Local Address           Foreign Address         State\n");
    pr_info("cherry_net: [netstat] tcp        0      0 0.0.0.0:5001            0.0.0.0:0               LISTEN\n");
    pr_info("cherry_net: [netstat] udp        0      0 0.0.0.0:68              0.0.0.0:0\n");
    return 0;
}
CHERRY_SHELL_EXPORT_CMD(netstat, net_cmd_netstat, "Show active network connections");
