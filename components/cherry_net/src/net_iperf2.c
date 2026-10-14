/*
 * CherryNet - iperf2 TCP/UDP Performance Benchmark Engine
 *
 * Copyright (c) 2026 ForgeStitch
 * SPDX-License-Identifier: MIT
 */

#include "net_iperf2.h"
#include "net_types.h"
#include "cherry_kal.h"
#include "sys/socket.h"
#include "netinet/in.h"
#include "arpa/inet.h"
#include <string.h>

struct iperf_server_param {
    uint16_t port;
    volatile bool ready;
    volatile bool done;
    uint64_t total_bytes;
    uint64_t elapsed_us;
};

struct iperf_client_param {
    char target[32];
    uint16_t port;
    uint32_t duration_sec;
    uint32_t buf_len;
    volatile bool done;
    uint64_t total_bytes;
    uint64_t elapsed_us;
};

static void print_iperf_report(int id, uint64_t bytes, uint64_t elapsed_us)
{
    uint64_t msec = elapsed_us / 1000ULL;
    if (msec == 0) msec = 1;

    /* MB = bytes / (1024 * 1024) */
    uint32_t mbytes = (uint32_t)(bytes / (1024ULL * 1024ULL));
    uint32_t mbytes_dec = (uint32_t)(((bytes % (1024ULL * 1024ULL)) * 10ULL) / (1024ULL * 1024ULL));

    /* Throughput in Mbits/sec: (bytes * 8 * 1000) / (msec * 1000000) = (bytes * 8) / (msec * 1000) */
    uint64_t mbps = (bytes * 8ULL) / (msec * 1000ULL);

    pr_info("cherry_net: [iperf] [%3d]  0.0-%2u.%u sec  %u.%u MBytes  %llu Mbits/sec\n",
            id,
            (unsigned int)(msec / 1000), (unsigned int)((msec % 1000) / 100),
            (unsigned int)mbytes, (unsigned int)mbytes_dec,
            (unsigned long long)mbps);
}

int iperf2_server_start(uint16_t port)
{
    if (port == 0) port = IPERF_DEFAULT_PORT;

    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) {
        pr_err("cherry_net: iperf server socket failed\n");
        return -1;
    }

    struct sockaddr_in sin;
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons(port);
    sin.sin_addr.s_addr = INADDR_ANY;

    if (bind(listen_fd, (struct sockaddr *)&sin, sizeof(sin)) != 0) {
        pr_err("cherry_net: iperf server bind failed\n");
        return -1;
    }

    if (listen(listen_fd, 5) != 0) {
        pr_err("cherry_net: iperf server listen failed\n");
        return -1;
    }

    pr_info("cherry_net: [iperf] server listening on tcp port %u\n", port);

    socklen_t client_len = sizeof(sin);
    int client_fd = accept(listen_fd, (struct sockaddr *)&sin, &client_len);
    if (client_fd < 0) {
        pr_err("cherry_net: iperf server accept failed\n");
        return -1;
    }

    uint8_t *rx_buf = (uint8_t *)kmalloc(4096, GFP_KERNEL);
    if (!rx_buf) return -1;

    uint64_t t_start = ktime_get_ns();
    uint64_t total_bytes = 0;

    while (1) {
        ssize_t n = recv(client_fd, rx_buf, 4096, 0);
        if (n <= 0) break;
        total_bytes += n;
    }

    uint64_t t_end = ktime_get_ns();
    uint64_t elapsed_us = (t_end - t_start) / 1000ULL;

    print_iperf_report(3, total_bytes, elapsed_us);
    kfree(rx_buf);
    return 0;
}

int iperf2_client_start(const char *server_ip, uint16_t port, uint32_t duration_sec, uint32_t buf_len)
{
    if (!server_ip) server_ip = "127.0.0.1";
    if (port == 0) port = IPERF_DEFAULT_PORT;
    if (duration_sec == 0) duration_sec = IPERF_DEFAULT_TIME_SEC;
    if (buf_len == 0) buf_len = IPERF_DEFAULT_BUFFER_LEN;

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        pr_err("cherry_net: iperf client socket failed\n");
        return -1;
    }

    struct sockaddr_in sin;
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons(port);
    sin.sin_addr.s_addr = inet_addr(server_ip);

    if (connect(fd, (struct sockaddr *)&sin, sizeof(sin)) != 0) {
        pr_err("cherry_net: iperf client connect to %s:%u failed\n", server_ip, port);
        return -1;
    }

    /* Send standard iperf2 client header */
    struct iperf_client_hdr hdr;
    memset(&hdr, 0, sizeof(hdr));
    hdr.flags = 0;
    hdr.num_threads = htonl(1);
    hdr.m_port = htonl(port);
    hdr.buffer_len = htonl((int32_t)buf_len);
    hdr.m_amount = htonl(-((int32_t)(duration_sec * 100))); /* negative = time in 10ms */
    send(fd, &hdr, sizeof(hdr), 0);

    uint8_t *tx_buf = (uint8_t *)kmalloc(buf_len, GFP_KERNEL);
    if (!tx_buf) return -1;
    memset(tx_buf, 0x5A, buf_len);

    pr_info("cherry_net: [iperf] client sending to %s:%u (%u sec)...\n",
            server_ip, port, duration_sec);

    uint64_t t_start = ktime_get_ns();
    uint64_t target_ns = t_start + (uint64_t)duration_sec * 1000000000ULL;
    uint64_t total_bytes = 0;

    while (ktime_get_ns() < target_ns) {
        ssize_t n = send(fd, tx_buf, buf_len, 0);
        if (n <= 0) break;
        total_bytes += n;
    }

    uint64_t t_end = ktime_get_ns();
    uint64_t elapsed_us = (t_end - t_start) / 1000ULL;

    print_iperf_report(4, total_bytes, elapsed_us);
    kfree(tx_buf);
    return 0;
}

static int iperf_srv_task(void *arg)
{
    struct iperf_server_param *sp = (struct iperf_server_param *)arg;
    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) {
        sp->done = true;
        return -1;
    }

    struct sockaddr_in sin;
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons(sp->port);
    sin.sin_addr.s_addr = INADDR_ANY;

    bind(listen_fd, (struct sockaddr *)&sin, sizeof(sin));
    listen(listen_fd, 5);

    sp->ready = true;

    socklen_t client_len = sizeof(sin);
    int client_fd = accept(listen_fd, (struct sockaddr *)&sin, &client_len);
    if (client_fd >= 0) {
        uint8_t *rx_buf = (uint8_t *)kmalloc(2048, GFP_KERNEL);
        if (rx_buf) {
            uint64_t t0 = ktime_get_ns();
            uint64_t total = 0;
            while (1) {
                ssize_t n = recv(client_fd, rx_buf, 2048, 0);
                if (n <= 0) break;
                total += n;
            }
            uint64_t t1 = ktime_get_ns();
            sp->total_bytes = total;
            sp->elapsed_us = (t1 - t0) / 1000ULL;
            kfree(rx_buf);
        }
    }
    sp->done = true;
    return 0;
}

int iperf2_run_self_test(void)
{
    pr_info("cherry_net: starting iperf2 multi-core throughput benchmark...\n");

    struct iperf_server_param srv_param;
    memset(&srv_param, 0, sizeof(srv_param));
    srv_param.port = 5001;
    srv_param.ready = false;
    srv_param.done = false;

    /* Run server pinned to Core 1 */
    struct task_struct *srv_task = kthread_run(iperf_srv_task, &srv_param, "iperf_srv");
    if (srv_task) {
        kthread_bind(srv_task, 1);
    }

    /* Wait for server ready */
    while (!srv_param.ready) {
        msleep(2);
    }

    /* Client on Core 0 */
    int client_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (client_fd < 0) return -1;

    struct sockaddr_in sin;
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons(5001);
    sin.sin_addr.s_addr = IP4_ADDR(127, 0, 0, 1);

    if (connect(client_fd, (struct sockaddr *)&sin, sizeof(sin)) != 0) {
        pr_err("cherry_net: self-test iperf connect failed\n");
        return -1;
    }

    /* Stream 64 KB of test traffic across cores */
    uint32_t chunk_sz = 1460;
    uint8_t *tx = (uint8_t *)kmalloc(chunk_sz, GFP_KERNEL);
    if (!tx) return -1;
    memset(tx, 0xAA, chunk_sz);

    uint64_t bytes_to_send = 64 * 1024;
    uint64_t sent = 0;
    uint64_t t0 = ktime_get_ns();
    int retries = 0;

    while (sent < bytes_to_send && retries < 1000) {
        size_t to_write = (bytes_to_send - sent > chunk_sz) ? chunk_sz : (size_t)(bytes_to_send - sent);
        ssize_t n = send(client_fd, tx, to_write, 0);
        if (n <= 0) {
            msleep(1);
            retries++;
            continue;
        }
        sent += n;
        retries = 0;
    }

    uint64_t t1 = ktime_get_ns();
    uint64_t elapsed_us = (t1 - t0) / 1000ULL;

    /* Signal EOF */
    shutdown(client_fd, 2);

    /* Wait for server task to finish report */
    while (!srv_param.done) {
        msleep(2);
    }

    kfree(tx);

    print_iperf_report(3, sent, elapsed_us);
    return (sent >= bytes_to_send) ? 0 : -1;
}
