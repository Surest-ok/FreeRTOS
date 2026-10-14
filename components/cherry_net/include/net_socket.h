/*
 * CherryNet - BSD Socket Table & VFS Provider
 *
 * Copyright (c) 2026 ForgeStitch
 * SPDX-License-Identifier: MIT
 */

#ifndef _CHERRY_NET_SOCKET_H_
#define _CHERRY_NET_SOCKET_H_

#include "net_types.h"
#include "net_tcp.h"
#include "net_udp.h"
#include "vfs_dev.h"

#ifdef __cplusplus
extern "C" {
#endif

#define NET_MAX_SOCKETS 64

struct net_socket {
    int      in_use;
    int      domain;
    int      type;
    int      protocol;
    union {
        struct tcp_pcb *tcp;
        struct udp_pcb *udp;
    };
    int      flags;
};

int                 net_socket_init(void);
struct vfs_sockdev *net_sockdev_get(void);

#ifdef __cplusplus
}
#endif

#endif /* _CHERRY_NET_SOCKET_H_ */
