/*
 * CherryNet - iperf2 Wire Protocol & Performance Benchmark Engine
 *
 * Copyright (c) 2026 ForgeStitch
 * SPDX-License-Identifier: MIT
 */

#ifndef _CHERRY_NET_IPERF2_H_
#define _CHERRY_NET_IPERF2_H_

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define IPERF_DEFAULT_PORT        5001
#define IPERF_DEFAULT_BUFFER_LEN  1460
#define IPERF_DEFAULT_TIME_SEC    3

/* Standard iperf2 wire format client header */
struct iperf_client_hdr {
    int32_t flags;
    int32_t num_threads;
    int32_t m_port;
    int32_t buffer_len;
    int32_t m_win_band;
    int32_t m_amount;
} __attribute__((packed));

/* iperf server & client APIs */
int iperf2_server_start(uint16_t port);
int iperf2_client_start(const char *server_ip, uint16_t port, uint32_t duration_sec, uint32_t buf_len);

/* Self-test benchmark running across FreeRTOS SMP cores */
int iperf2_run_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* _CHERRY_NET_IPERF2_H_ */
