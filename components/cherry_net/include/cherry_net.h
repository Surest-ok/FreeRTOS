/*
 * CherryNet - Master Component Header
 *
 * Copyright (c) 2026 ForgeStitch
 * SPDX-License-Identifier: MIT
 */

#ifndef _CHERRY_NET_H_
#define _CHERRY_NET_H_

#include "net_types.h"
#include "net_buf.h"
#include "net_if.h"
#include "net_queue.h"
#include "net_eth.h"
#include "net_ip4.h"
#include "net_ip6.h"
#include "net_icmp.h"
#include "net_udp.h"
#include "net_tcp.h"
#include "net_socket.h"
#include "net_virtio.h"
#include "net_iperf2.h"
#include "net_tools.h"

#ifdef __cplusplus
extern "C" {
#endif

int  cherry_net_init(void);
void cherry_net_run_tests(void);

#ifdef __cplusplus
}
#endif

#endif /* _CHERRY_NET_H_ */
