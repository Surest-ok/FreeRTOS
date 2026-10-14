/*
 * CherryNet - BSD Socket Layer & VFS Integration
 *
 * Copyright (c) 2026 ForgeStitch
 * SPDX-License-Identifier: MIT
 */

#include "net_socket.h"
#include "cherry_kal.h"
#include "vfs_posix.h"
#include "vfs_dev.h"
#include "arpa/inet.h"
#include <string.h>

static struct net_socket s_sockets[NET_MAX_SOCKETS];
static spinlock_t        s_sock_lock = SPIN_LOCK_UNLOCKED;

static struct net_socket *sock_alloc(void)
{
    unsigned long flags;
    spin_lock_irqsave(&s_sock_lock, flags);

    for (int i = 0; i < NET_MAX_SOCKETS; i++) {
        if (!s_sockets[i].in_use) {
            s_sockets[i].in_use = 1;
            s_sockets[i].tcp = NULL;
            s_sockets[i].flags = 0;
            spin_unlock_irqrestore(&s_sock_lock, flags);
            return &s_sockets[i];
        }
    }

    spin_unlock_irqrestore(&s_sock_lock, flags);
    return NULL;
}

static void sock_free(struct net_socket *sock)
{
    if (!sock) return;
    unsigned long flags;
    spin_lock_irqsave(&s_sock_lock, flags);
    sock->in_use = 0;
    sock->tcp = NULL;
    spin_unlock_irqrestore(&s_sock_lock, flags);
}

/* =========================================================================
 * VFS Socket Operations Bridge
 * ========================================================================= */
static void *cherry_sock_socket(int domain, int type, int protocol)
{
    struct net_socket *s = sock_alloc();
    if (!s) return NULL;

    s->domain = domain;
    s->type = type;
    s->protocol = protocol;

    if (type == SOCK_STREAM || protocol == IPPROTO_TCP) {
        s->tcp = tcp_new((uint16_t)domain);
        if (!s->tcp) {
            sock_free(s);
            return NULL;
        }
    } else if (type == SOCK_DGRAM || protocol == IPPROTO_UDP) {
        s->udp = udp_new((uint16_t)domain);
        if (!s->udp) {
            sock_free(s);
            return NULL;
        }
    }

    return s;
}

static int cherry_sock_bind(void *sock, const void *addr, socklen_t addrlen)
{
    struct net_socket *s = (struct net_socket *)sock;
    if (!s) return -1;

    if (s->type == SOCK_STREAM && s->tcp) {
        return tcp_bind(s->tcp, addr, addrlen);
    } else if (s->type == SOCK_DGRAM && s->udp) {
        return udp_bind(s->udp, addr, addrlen);
    }
    return -1;
}

static int cherry_sock_connect(void *sock, const void *addr, socklen_t addrlen)
{
    struct net_socket *s = (struct net_socket *)sock;
    if (!s) return -1;

    if (s->type == SOCK_STREAM && s->tcp) {
        return tcp_connect(s->tcp, addr, addrlen);
    } else if (s->type == SOCK_DGRAM && s->udp) {
        return udp_connect(s->udp, addr, addrlen);
    }
    return -1;
}

static int cherry_sock_listen(void *sock, int backlog)
{
    struct net_socket *s = (struct net_socket *)sock;
    if (!s || s->type != SOCK_STREAM || !s->tcp) return -1;
    return tcp_listen(s->tcp, backlog);
}

static void *cherry_sock_accept(void *sock, void *addr, socklen_t *addrlen)
{
    struct net_socket *s = (struct net_socket *)sock;
    if (!s || s->type != SOCK_STREAM || !s->tcp) return NULL;

    struct tcp_pcb *child_pcb = tcp_accept(s->tcp, addr, addrlen);
    if (!child_pcb) return NULL;

    struct net_socket *child_sock = sock_alloc();
    if (!child_sock) {
        tcp_close(child_pcb);
        return NULL;
    }

    child_sock->domain = s->domain;
    child_sock->type = s->type;
    child_sock->protocol = s->protocol;
    child_sock->tcp = child_pcb;
    return child_sock;
}

static ssize_t cherry_sock_send(void *sock, const void *buf, size_t len, int flags)
{
    struct net_socket *s = (struct net_socket *)sock;
    if (!s) return -1;

    if (s->type == SOCK_STREAM && s->tcp) {
        return tcp_send(s->tcp, buf, len, flags);
    } else if (s->type == SOCK_DGRAM && s->udp) {
        return udp_sendto(s->udp, buf, len, flags, NULL, 0);
    }
    return -1;
}

static ssize_t cherry_sock_recv(void *sock, void *buf, size_t len, int flags)
{
    struct net_socket *s = (struct net_socket *)sock;
    if (!s) return -1;

    if (s->type == SOCK_STREAM && s->tcp) {
        return tcp_recv(s->tcp, buf, len, flags);
    } else if (s->type == SOCK_DGRAM && s->udp) {
        return udp_recvfrom(s->udp, buf, len, flags, NULL, NULL);
    }
    return -1;
}

static ssize_t cherry_sock_sendto(void *sock, const void *buf, size_t len, int flags,
                                  const void *dest_addr, socklen_t addrlen)
{
    struct net_socket *s = (struct net_socket *)sock;
    if (!s) return -1;

    if (s->type == SOCK_DGRAM && s->udp) {
        return udp_sendto(s->udp, buf, len, flags, dest_addr, addrlen);
    } else if (s->type == SOCK_STREAM && s->tcp) {
        return tcp_send(s->tcp, buf, len, flags);
    }
    return -1;
}

static ssize_t cherry_sock_recvfrom(void *sock, void *buf, size_t len, int flags,
                                    void *src_addr, socklen_t *addrlen)
{
    struct net_socket *s = (struct net_socket *)sock;
    if (!s) return -1;

    if (s->type == SOCK_DGRAM && s->udp) {
        return udp_recvfrom(s->udp, buf, len, flags, src_addr, addrlen);
    } else if (s->type == SOCK_STREAM && s->tcp) {
        return tcp_recv(s->tcp, buf, len, flags);
    }
    return -1;
}

static int cherry_sock_setsockopt(void *sock, int level, int optname, const void *optval, socklen_t optlen)
{
    struct net_socket *s = (struct net_socket *)sock;
    if (!s || !optval) return -1;

    if (level == SOL_SOCKET) {
        if (optname == SO_RCVTIMEO && optlen >= sizeof(uint32_t)) {
            uint32_t timeout_ms = *(const uint32_t *)optval;
            if (s->type == SOCK_STREAM && s->tcp) s->tcp->rcv_timeout_ms = timeout_ms;
            if (s->type == SOCK_DGRAM && s->udp) s->udp->rcv_timeout_ms = timeout_ms;
            return 0;
        } else if (optname == SO_SNDTIMEO && optlen >= sizeof(uint32_t)) {
            uint32_t timeout_ms = *(const uint32_t *)optval;
            if (s->type == SOCK_STREAM && s->tcp) s->tcp->snd_timeout_ms = timeout_ms;
            return 0;
        } else if (optname == SO_NONBLOCK) {
            int nb = *(const int *)optval;
            if (s->type == SOCK_STREAM && s->tcp) s->tcp->nonblock = (nb != 0);
            if (s->type == SOCK_DGRAM && s->udp) s->udp->nonblock = (nb != 0);
            return 0;
        }
    }
    return 0;
}

static int cherry_sock_getsockopt(void *sock, int level, int optname, void *optval, socklen_t *optlen)
{
    (void)sock;
    (void)level;
    (void)optname;
    (void)optval;
    (void)optlen;
    return 0;
}

static int cherry_sock_shutdown(void *sock, int how)
{
    struct net_socket *s = (struct net_socket *)sock;
    if (!s) return -1;
    if (s->type == SOCK_STREAM && s->tcp) {
        return tcp_shutdown(s->tcp, how);
    }
    return 0;
}

static int cherry_sock_close(void *sock)
{
    struct net_socket *s = (struct net_socket *)sock;
    if (!s) return -1;

    if (s->type == SOCK_STREAM && s->tcp) {
        tcp_close(s->tcp);
        s->tcp = NULL;
    } else if (s->type == SOCK_DGRAM && s->udp) {
        udp_close(s->udp);
        s->udp = NULL;
    }
    sock_free(s);
    return 0;
}

static const struct vfs_sock_ops s_cherry_sock_ops = {
    .socket = cherry_sock_socket,
    .bind = cherry_sock_bind,
    .connect = cherry_sock_connect,
    .listen = cherry_sock_listen,
    .accept = cherry_sock_accept,
    .send = cherry_sock_send,
    .recv = cherry_sock_recv,
    .sendto = cherry_sock_sendto,
    .recvfrom = cherry_sock_recvfrom,
    .setsockopt = cherry_sock_setsockopt,
    .getsockopt = cherry_sock_getsockopt,
    .shutdown = cherry_sock_shutdown,
    .close = cherry_sock_close,
};

static struct vfs_sockdev s_cherry_sockdev = {
    .ops = &s_cherry_sock_ops,
};

int net_socket_init(void)
{
    spin_lock_init(&s_sock_lock);
    memset(s_sockets, 0, sizeof(s_sockets));

    int ret = vfs_sockdev_register(&s_cherry_sockdev, "cherry_net_sock");
    vfs_sockdev_set_default(&s_cherry_sockdev);
    return ret;
}

struct vfs_sockdev *net_sockdev_get(void)
{
    return &s_cherry_sockdev;
}

/* =========================================================================
 * POSIX inet Address Utilities
 * ========================================================================= */
in_addr_t inet_addr(const char *cp)
{
    struct in_addr val;
    if (inet_aton(cp, &val)) {
        return val.s_addr;
    }
    return INADDR_BROADCAST;
}

int inet_aton(const char *cp, struct in_addr *inp)
{
    if (!cp || !inp) return 0;
    uint32_t val = 0;
    int dots = 0;
    uint32_t part = 0;
    bool in_num = false;

    while (*cp) {
        char c = *cp++;
        if (c >= '0' && c <= '9') {
            part = part * 10 + (uint32_t)(c - '0');
            in_num = true;
            if (part > 255) return 0;
        } else if (c == '.') {
            if (!in_num || dots >= 3) return 0;
            val = (val << 8) | part;
            part = 0;
            in_num = false;
            dots++;
        } else {
            return 0;
        }
    }
    if (!in_num || dots != 3) return 0;
    val = (val << 8) | part;
    inp->s_addr = htonl(val);
    return 1;
}

char *inet_ntoa(struct in_addr in)
{
    static char buf[16];
    uint32_t ip = ntohl(in.s_addr);
    uint8_t a = (ip >> 24) & 0xFF;
    uint8_t b = (ip >> 16) & 0xFF;
    uint8_t c = (ip >> 8)  & 0xFF;
    uint8_t d = ip & 0xFF;

    int pos = 0;
    uint8_t octets[4] = {a, b, c, d};
    for (int i = 0; i < 4; i++) {
        uint8_t val = octets[i];
        if (val >= 100) {
            buf[pos++] = '0' + (val / 100);
            val %= 100;
            buf[pos++] = '0' + (val / 10);
            buf[pos++] = '0' + (val % 10);
        } else if (val >= 10) {
            buf[pos++] = '0' + (val / 10);
            buf[pos++] = '0' + (val % 10);
        } else {
            buf[pos++] = '0' + val;
        }
        if (i < 3) buf[pos++] = '.';
    }
    buf[pos] = '\0';
    return buf;
}

int inet_pton(int af, const char *src, void *dst)
{
    if (af == AF_INET) {
        return inet_aton(src, (struct in_addr *)dst);
    } else if (af == AF_INET6) {
        /* Simple IPv6 parser for ::1 and colon-hex representations */
        if (!src || !dst) return 0;
        if (strcmp(src, "::1") == 0) {
            memset(dst, 0, 16);
            ((uint8_t *)dst)[15] = 1;
            return 1;
        }
        /* Parse up to 8 16-bit hex components */
        struct ip6_addr *ip6 = (struct ip6_addr *)dst;
        memset(ip6, 0, sizeof(*ip6));
        const char *p = src;
        int idx = 0;
        while (*p && idx < 8) {
            if (*p == ':') {
                p++;
                continue;
            }
            uint32_t val = 0;
            while ((*p >= '0' && *p <= '9') || (*p >= 'a' && *p <= 'f') || (*p >= 'A' && *p <= 'F')) {
                val <<= 4;
                if (*p >= '0' && *p <= '9') val |= (*p - '0');
                else if (*p >= 'a' && *p <= 'f') val |= (*p - 'a' + 10);
                else val |= (*p - 'A' + 10);
                p++;
            }
            ip6->u16[idx++] = htons((uint16_t)val);
            if (*p == ':') p++;
        }
        return 1;
    }
    return -1;
}

const char *inet_ntop(int af, const void *src, char *dst, socklen_t size)
{
    if (!src || !dst || size == 0) return NULL;
    if (af == AF_INET) {
        char *str = inet_ntoa(*(const struct in_addr *)src);
        strncpy(dst, str, size - 1);
        dst[size - 1] = '\0';
        return dst;
    }
    return NULL;
}
