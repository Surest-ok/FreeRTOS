/*
 * CherryVFS - Virtual File System for FreeRTOS
 *
 * Copyright (c) 2026 ForgeStitch
 * SPDX-License-Identifier: MIT
 *
 * Unified Socket Device Abstraction and POSIX BSD Socket Wrappers
 */

#include "cherry_vfs.h"
#include <string.h>

#define LOOPBACK_SOCK_BUF_SIZE 512

/* Internal Context for Loopback/Test Socket */
struct loopback_socket_ctx {
    int     bound;
    int     connected;
    int     listening;
    uint8_t buffer[LOOPBACK_SOCK_BUF_SIZE];
    size_t  data_len;
    size_t  read_pos;
};

/* File Operations Bridge for Socket File Descriptors */
static int sock_file_open(struct vfs_file *file)
{
    (void)file;
    return VFS_OK;
}

static int sock_file_close(struct vfs_file *file)
{
    if (!file || !file->f_inode || !file->f_inode->i_private) {
        return -EINVAL;
    }
    struct vfs_sockdev *sdev = (struct vfs_sockdev *)file->f_inode->i_private;
    if (sdev && sdev->ops && sdev->ops->close && file->f_private) {
        return sdev->ops->close(file->f_private);
    }
    return VFS_OK;
}

static ssize_t sock_file_read(struct vfs_file *file, void *buf, size_t count)
{
    if (!file || !file->f_inode || !file->f_inode->i_private || !file->f_private) {
        return -EINVAL;
    }
    struct vfs_sockdev *sdev = (struct vfs_sockdev *)file->f_inode->i_private;
    if (sdev && sdev->ops && sdev->ops->recv) {
        return sdev->ops->recv(file->f_private, buf, count, 0);
    }
    return -ENOSYS;
}

static ssize_t sock_file_write(struct vfs_file *file, const void *buf, size_t count)
{
    if (!file || !file->f_inode || !file->f_inode->i_private || !file->f_private) {
        return -EINVAL;
    }
    struct vfs_sockdev *sdev = (struct vfs_sockdev *)file->f_inode->i_private;
    if (sdev && sdev->ops && sdev->ops->send) {
        return sdev->ops->send(file->f_private, buf, count, 0);
    }
    return -ENOSYS;
}

static const struct vfs_file_operations g_sock_file_ops = {
    .open = sock_file_open,
    .close = sock_file_close,
    .read = sock_file_read,
    .write = sock_file_write,
    .ioctl = NULL,
    .lseek = NULL,
    .readdir = NULL,
    .sync = NULL,
};

/* =========================================================================
 * Loopback Test Socket Implementation
 * ========================================================================= */
static void *loopback_socket(int domain, int type, int protocol)
{
    (void)domain;
    (void)type;
    (void)protocol;

    struct loopback_socket_ctx *ctx = (struct loopback_socket_ctx *)vfs_malloc(sizeof(struct loopback_socket_ctx));
    if (!ctx) return NULL;

    memset(ctx, 0, sizeof(struct loopback_socket_ctx));
    return ctx;
}

static int loopback_bind(void *sock, const void *addr, socklen_t addrlen)
{
    (void)addr;
    (void)addrlen;
    if (!sock) return -EINVAL;
    struct loopback_socket_ctx *ctx = (struct loopback_socket_ctx *)sock;
    ctx->bound = 1;
    return VFS_OK;
}

static int loopback_connect(void *sock, const void *addr, socklen_t addrlen)
{
    (void)addr;
    (void)addrlen;
    if (!sock) return -EINVAL;
    struct loopback_socket_ctx *ctx = (struct loopback_socket_ctx *)sock;
    ctx->connected = 1;
    return VFS_OK;
}

static int loopback_listen(void *sock, int backlog)
{
    (void)backlog;
    if (!sock) return -EINVAL;
    struct loopback_socket_ctx *ctx = (struct loopback_socket_ctx *)sock;
    ctx->listening = 1;
    return VFS_OK;
}

static void *loopback_accept(void *sock, void *addr, socklen_t *addrlen)
{
    (void)addr;
    (void)addrlen;
    if (!sock) return NULL;
    return loopback_socket(0, 0, 0);
}

static ssize_t loopback_send(void *sock, const void *buf, size_t len, int flags)
{
    (void)flags;
    if (!sock || !buf) return -EINVAL;
    struct loopback_socket_ctx *ctx = (struct loopback_socket_ctx *)sock;

    size_t space = LOOPBACK_SOCK_BUF_SIZE - ctx->data_len;
    size_t to_write = (len < space) ? len : space;

    if (to_write > 0) {
        memcpy(ctx->buffer + ctx->data_len, buf, to_write);
        ctx->data_len += to_write;
    }
    return (ssize_t)to_write;
}

static ssize_t loopback_recv(void *sock, void *buf, size_t len, int flags)
{
    (void)flags;
    if (!sock || !buf) return -EINVAL;
    struct loopback_socket_ctx *ctx = (struct loopback_socket_ctx *)sock;

    size_t avail = ctx->data_len - ctx->read_pos;
    if (avail == 0) return 0;

    size_t to_read = (len < avail) ? len : avail;
    memcpy(buf, ctx->buffer + ctx->read_pos, to_read);
    ctx->read_pos += to_read;

    if (ctx->read_pos == ctx->data_len) {
        ctx->read_pos = 0;
        ctx->data_len = 0;
    }
    return (ssize_t)to_read;
}

static int loopback_shutdown(void *sock, int how)
{
    (void)how;
    if (!sock) return -EINVAL;
    struct loopback_socket_ctx *ctx = (struct loopback_socket_ctx *)sock;
    ctx->connected = 0;
    return VFS_OK;
}

static int loopback_close(void *sock)
{
    if (sock) {
        vfs_free(sock);
    }
    return VFS_OK;
}

static const struct vfs_sock_ops g_loopback_sock_ops = {
    .socket = loopback_socket,
    .bind = loopback_bind,
    .connect = loopback_connect,
    .listen = loopback_listen,
    .accept = loopback_accept,
    .send = loopback_send,
    .recv = loopback_recv,
    .shutdown = loopback_shutdown,
    .close = loopback_close,
};

static struct vfs_sockdev g_loopback_sockdev = {
    .ops = &g_loopback_sock_ops,
};

static struct vfs_sockdev *s_default_sockdev = NULL;

void vfs_sockdev_set_default(struct vfs_sockdev *sdev)
{
    s_default_sockdev = sdev;
}

struct vfs_sockdev *vfs_sockdev_get_default(void)
{
    if (s_default_sockdev) {
        return s_default_sockdev;
    }
    struct vfs_sockdev *s = vfs_sockdev_find("cherry_net_sock");
    if (s) {
        return s;
    }
    return vfs_sockdev_find("loopback_sock");
}

int dev_socket_init(void)
{
    return vfs_sockdev_register(&g_loopback_sockdev, "loopback_sock");
}

/* =========================================================================
 * POSIX BSD Socket Wrappers
 * ========================================================================= */
int socket(int domain, int type, int protocol)
{
    struct vfs_sockdev *sdev = vfs_sockdev_get_default();
    if (!sdev || !sdev->ops || !sdev->ops->socket) {
        return -ENODEV;
    }

    void *sock_inst = sdev->ops->socket(domain, type, protocol);
    if (!sock_inst) {
        return -ENOMEM;
    }

    struct vfs_inode *inode = vfs_inode_alloc(NULL, S_IFSOCK | 0666);
    if (!inode) {
        if (sdev->ops->close) sdev->ops->close(sock_inst);
        return -ENOMEM;
    }
    inode->i_private = sdev;
    inode->i_fops = &g_sock_file_ops;

    struct vfs_file *file = (struct vfs_file *)vfs_malloc(sizeof(struct vfs_file));
    if (!file) {
        vfs_inode_put(inode);
        if (sdev->ops->close) sdev->ops->close(sock_inst);
        return -ENOMEM;
    }
    memset(file, 0, sizeof(struct vfs_file));
    file->f_inode = inode;
    file->f_ops = &g_sock_file_ops;
    file->f_flags = O_RDWR;
    file->f_private = sock_inst;
    file->f_ref = 1;

    int fd = vfs_fd_alloc(file);
    if (fd < 0) {
        vfs_file_put(file);
        return fd;
    }

    return fd;
}

int bind(int sockfd, const void *addr, socklen_t addrlen)
{
    struct vfs_file *file = vfs_file_get(sockfd);
    if (!file || !file->f_inode || !file->f_inode->i_private || !file->f_private) {
        return -EBADF;
    }
    struct vfs_sockdev *sdev = (struct vfs_sockdev *)file->f_inode->i_private;
    int ret = -ENOSYS;
    if (sdev->ops && sdev->ops->bind) {
        ret = sdev->ops->bind(file->f_private, addr, addrlen);
    }
    vfs_file_put(file);
    return ret;
}

int connect(int sockfd, const void *addr, socklen_t addrlen)
{
    struct vfs_file *file = vfs_file_get(sockfd);
    if (!file || !file->f_inode || !file->f_inode->i_private || !file->f_private) {
        return -EBADF;
    }
    struct vfs_sockdev *sdev = (struct vfs_sockdev *)file->f_inode->i_private;
    int ret = -ENOSYS;
    if (sdev->ops && sdev->ops->connect) {
        ret = sdev->ops->connect(file->f_private, addr, addrlen);
    }
    vfs_file_put(file);
    return ret;
}

int listen(int sockfd, int backlog)
{
    struct vfs_file *file = vfs_file_get(sockfd);
    if (!file || !file->f_inode || !file->f_inode->i_private || !file->f_private) {
        return -EBADF;
    }
    struct vfs_sockdev *sdev = (struct vfs_sockdev *)file->f_inode->i_private;
    int ret = -ENOSYS;
    if (sdev->ops && sdev->ops->listen) {
        ret = sdev->ops->listen(file->f_private, backlog);
    }
    vfs_file_put(file);
    return ret;
}

int accept(int sockfd, void *addr, socklen_t *addrlen)
{
    struct vfs_file *file = vfs_file_get(sockfd);
    if (!file || !file->f_inode || !file->f_inode->i_private || !file->f_private) {
        return -EBADF;
    }
    struct vfs_sockdev *sdev = (struct vfs_sockdev *)file->f_inode->i_private;
    if (!sdev->ops || !sdev->ops->accept) {
        vfs_file_put(file);
        return -ENOSYS;
    }

    void *new_sock = sdev->ops->accept(file->f_private, addr, addrlen);
    vfs_file_put(file);

    if (!new_sock) {
        return -ECONNABORTED;
    }

    struct vfs_inode *inode = vfs_inode_alloc(NULL, S_IFSOCK | 0666);
    if (!inode) {
        if (sdev->ops->close) sdev->ops->close(new_sock);
        return -ENOMEM;
    }
    inode->i_private = sdev;
    inode->i_fops = &g_sock_file_ops;

    struct vfs_file *new_file = (struct vfs_file *)vfs_malloc(sizeof(struct vfs_file));
    if (!new_file) {
        vfs_inode_put(inode);
        if (sdev->ops->close) sdev->ops->close(new_sock);
        return -ENOMEM;
    }
    memset(new_file, 0, sizeof(struct vfs_file));
    new_file->f_inode = inode;
    new_file->f_ops = &g_sock_file_ops;
    new_file->f_flags = O_RDWR;
    new_file->f_private = new_sock;
    new_file->f_ref = 1;

    int new_fd = vfs_fd_alloc(new_file);
    if (new_fd < 0) {
        vfs_file_put(new_file);
        return new_fd;
    }

    return new_fd;
}

ssize_t send(int sockfd, const void *buf, size_t len, int flags)
{
    struct vfs_file *file = vfs_file_get(sockfd);
    if (!file || !file->f_inode || !file->f_inode->i_private || !file->f_private) {
        return -EBADF;
    }
    struct vfs_sockdev *sdev = (struct vfs_sockdev *)file->f_inode->i_private;
    ssize_t ret = -ENOSYS;
    if (sdev->ops && sdev->ops->send) {
        ret = sdev->ops->send(file->f_private, buf, len, flags);
    }
    vfs_file_put(file);
    return ret;
}

ssize_t recv(int sockfd, void *buf, size_t len, int flags)
{
    struct vfs_file *file = vfs_file_get(sockfd);
    if (!file || !file->f_inode || !file->f_inode->i_private || !file->f_private) {
        return -EBADF;
    }
    struct vfs_sockdev *sdev = (struct vfs_sockdev *)file->f_inode->i_private;
    ssize_t ret = -ENOSYS;
    if (sdev->ops && sdev->ops->recv) {
        ret = sdev->ops->recv(file->f_private, buf, len, flags);
    }
    vfs_file_put(file);
    return ret;
}

int shutdown(int sockfd, int how)
{
    struct vfs_file *file = vfs_file_get(sockfd);
    if (!file || !file->f_inode || !file->f_inode->i_private || !file->f_private) {
        return -EBADF;
    }
    struct vfs_sockdev *sdev = (struct vfs_sockdev *)file->f_inode->i_private;
    int ret = -ENOSYS;
    if (sdev->ops && sdev->ops->shutdown) {
        ret = sdev->ops->shutdown(file->f_private, how);
    }
    vfs_file_put(file);
    return ret;
}

ssize_t sendto(int sockfd, const void *buf, size_t len, int flags, const void *dest_addr, socklen_t addrlen)
{
    struct vfs_file *file = vfs_file_get(sockfd);
    if (!file || !file->f_inode || !file->f_inode->i_private || !file->f_private) {
        return -EBADF;
    }
    struct vfs_sockdev *sdev = (struct vfs_sockdev *)file->f_inode->i_private;
    ssize_t ret = -ENOSYS;
    if (sdev->ops && sdev->ops->sendto) {
        ret = sdev->ops->sendto(file->f_private, buf, len, flags, dest_addr, addrlen);
    } else if (sdev->ops && sdev->ops->send) {
        ret = sdev->ops->send(file->f_private, buf, len, flags);
    }
    vfs_file_put(file);
    return ret;
}

ssize_t recvfrom(int sockfd, void *buf, size_t len, int flags, void *src_addr, socklen_t *addrlen)
{
    struct vfs_file *file = vfs_file_get(sockfd);
    if (!file || !file->f_inode || !file->f_inode->i_private || !file->f_private) {
        return -EBADF;
    }
    struct vfs_sockdev *sdev = (struct vfs_sockdev *)file->f_inode->i_private;
    ssize_t ret = -ENOSYS;
    if (sdev->ops && sdev->ops->recvfrom) {
        ret = sdev->ops->recvfrom(file->f_private, buf, len, flags, src_addr, addrlen);
    } else if (sdev->ops && sdev->ops->recv) {
        ret = sdev->ops->recv(file->f_private, buf, len, flags);
    }
    vfs_file_put(file);
    return ret;
}

int setsockopt(int sockfd, int level, int optname, const void *optval, socklen_t optlen)
{
    struct vfs_file *file = vfs_file_get(sockfd);
    if (!file || !file->f_inode || !file->f_inode->i_private || !file->f_private) {
        return -EBADF;
    }
    struct vfs_sockdev *sdev = (struct vfs_sockdev *)file->f_inode->i_private;
    int ret = -ENOSYS;
    if (sdev->ops && sdev->ops->setsockopt) {
        ret = sdev->ops->setsockopt(file->f_private, level, optname, optval, optlen);
    }
    vfs_file_put(file);
    return ret;
}

int getsockopt(int sockfd, int level, int optname, void *optval, socklen_t *optlen)
{
    struct vfs_file *file = vfs_file_get(sockfd);
    if (!file || !file->f_inode || !file->f_inode->i_private || !file->f_private) {
        return -EBADF;
    }
    struct vfs_sockdev *sdev = (struct vfs_sockdev *)file->f_inode->i_private;
    int ret = -ENOSYS;
    if (sdev->ops && sdev->ops->getsockopt) {
        ret = sdev->ops->getsockopt(file->f_private, level, optname, optval, optlen);
    }
    vfs_file_put(file);
    return ret;
}

