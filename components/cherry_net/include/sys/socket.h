/*
 * CherryNet - POSIX sys/socket.h Compatibility Header
 *
 * Copyright (c) 2026 ForgeStitch
 * SPDX-License-Identifier: MIT
 */

#ifndef _SYS_SOCKET_H_
#define _SYS_SOCKET_H_

#include "net_types.h"

#ifdef __cplusplus
extern "C" {
#endif

int     socket(int domain, int type, int protocol);
int     bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
int     connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
int     listen(int sockfd, int backlog);
int     accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen);
ssize_t send(int sockfd, const void *buf, size_t len, int flags);
ssize_t recv(int sockfd, void *buf, size_t len, int flags);
ssize_t sendto(int sockfd, const void *buf, size_t len, int flags, const struct sockaddr *dest_addr, socklen_t addrlen);
ssize_t recvfrom(int sockfd, void *buf, size_t len, int flags, struct sockaddr *src_addr, socklen_t *addrlen);
int     shutdown(int sockfd, int how);
int     setsockopt(int sockfd, int level, int optname, const void *optval, socklen_t optlen);
int     getsockopt(int sockfd, int level, int optname, void *optval, socklen_t *optlen);

#ifdef __cplusplus
}
#endif

#endif /* _SYS_SOCKET_H_ */
