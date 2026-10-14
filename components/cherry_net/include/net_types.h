/*
 * CherryNet - High-Performance Multi-Core Multi-Queue TCP/IP Stack
 *
 * Copyright (c) 2026 ForgeStitch
 * SPDX-License-Identifier: MIT
 *
 * Common Types, Endian Conversions, and Network Addressing
 */

#ifndef _CHERRY_NET_TYPES_H_
#define _CHERRY_NET_TYPES_H_

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifndef _SSIZE_T_DECLARED
#define _SSIZE_T_DECLARED
typedef long ssize_t;
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* Byte Order Conversions */
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
static inline uint16_t htons(uint16_t x) { return (uint16_t)((x << 8) | (x >> 8)); }
static inline uint16_t ntohs(uint16_t x) { return htons(x); }
static inline uint32_t htonl(uint32_t x) {
    return ((x << 24) & 0xff000000U) |
           ((x << 8)  & 0x00ff0000U) |
           ((x >> 8)  & 0x0000ff00U) |
           ((x >> 24) & 0x000000ffU);
}
static inline uint32_t ntohl(uint32_t x) { return htonl(x); }
#else
static inline uint16_t htons(uint16_t x) { return x; }
static inline uint16_t ntohs(uint16_t x) { return x; }
static inline uint32_t htonl(uint32_t x) { return x; }
static inline uint32_t ntohl(uint32_t x) { return x; }
#endif

/* Standard EtherTypes */
#define ETH_P_IP        0x0800U
#define ETH_P_ARP       0x0806U
#define ETH_P_IPV6      0x86DDU

/* Standard IP Protocols */
#define IPPROTO_IP      0
#define IPPROTO_ICMP    1
#define IPPROTO_TCP     6
#define IPPROTO_UDP     17
#define IPPROTO_IPV6    41
#define IPPROTO_ICMPV6  58
#define IPPROTO_RAW     255

/* Hardware and Network Lengths */
#define ETH_ALEN        6
#define ETH_HLEN        14
#define ETH_DATA_LEN    1500
#define ETH_FRAME_LEN   1514

/* Ethernet Hardware Address */
struct eth_addr {
    uint8_t addr[ETH_ALEN];
} __attribute__((packed));

/* IPv4 Address */
struct ip4_addr {
    uint32_t addr; /* Network byte order */
};

#define IP4_ADDR(a, b, c, d) \
    htonl(((uint32_t)((a) & 0xff) << 24) | \
          ((uint32_t)((b) & 0xff) << 16) | \
          ((uint32_t)((c) & 0xff) << 8)  | \
          ((uint32_t)((d) & 0xff)))

#define INADDR_ANY              0x00000000U
#define INADDR_LOOPBACK         0x7F000001U
#define INADDR_BROADCAST        0xFFFFFFFFU

/* IPv6 Address */
struct ip6_addr {
    union {
        uint8_t  u8[16];
        uint16_t u16[8];
        uint32_t u32[4];
    };
};

/* BSD Socket Address Families */
#define AF_UNSPEC       0
#define AF_INET         2
#define AF_INET6        10

/* BSD Socket Types */
#define SOCK_STREAM     1
#define SOCK_DGRAM      2
#define SOCK_RAW        3

/* Socket Options */
#define SOL_SOCKET      1
#define SO_DEBUG        1
#define SO_REUSEADDR    2
#define SO_TYPE         3
#define SO_ERROR        4
#define SO_DONTROUTE    5
#define SO_BROADCAST    6
#define SO_SNDBUF       7
#define SO_RCVBUF       8
#define SO_KEEPALIVE    9
#define SO_OOBINLINE    10
#define SO_NO_CHECK     11
#define SO_PRIORITY     12
#define SO_LINGER       13
#define SO_BSDCOMPAT    14
#define SO_REUSEPORT    15
#define SO_RCVTIMEO     20
#define SO_SNDTIMEO     21
#define SO_NONBLOCK     0x1000

#define IPPROTO_TCP_NODELAY 1

/* Standard POSIX Types */
typedef uint32_t socklen_t;
typedef uint16_t in_port_t;
typedef uint32_t in_addr_t;

struct in_addr {
    in_addr_t s_addr;
};

struct in6_addr {
    union {
        uint8_t  s6_addr[16];
        uint16_t s6_addr16[8];
        uint32_t s6_addr32[4];
    };
};

struct sockaddr {
    uint16_t sa_family;
    char     sa_data[14];
};

struct sockaddr_in {
    uint16_t       sin_family;
    in_port_t      sin_port;
    struct in_addr sin_addr;
    char           sin_zero[8];
};

struct sockaddr_in6 {
    uint16_t        sin6_family;
    in_port_t       sin6_port;
    uint32_t        sin6_flowinfo;
    struct in6_addr sin6_addr;
    uint32_t        sin6_scope_id;
};

struct sockaddr_storage {
    uint16_t ss_family;
    char     __ss_pad[26];
};

#ifdef __cplusplus
}
#endif

#endif /* _CHERRY_NET_TYPES_H_ */
