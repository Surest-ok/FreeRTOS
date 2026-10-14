/*
 * CherryVFS - Virtual File System for FreeRTOS
 *
 * Copyright (c) 2026 ForgeStitch
 * SPDX-License-Identifier: MIT
 *
 * Linux-compatible POSIX File, Directory, and Socket API
 */

#ifndef _CHERRY_VFS_POSIX_H_
#define _CHERRY_VFS_POSIX_H_

#include "vfs_types.h"
#include "vfs_inode.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Standard POSIX File I/O Calls */
int     open(const char *path, int flags, ...);
int     close(int fd);
ssize_t read(int fd, void *buf, size_t count);
ssize_t write(int fd, const void *buf, size_t count);
off_t   lseek(int fd, off_t offset, int whence);
int     ioctl(int fd, unsigned int cmd, ...);
int     stat(const char *path, struct stat *buf);
int     fstat(int fd, struct stat *buf);
int     sync(void);
int     fsync(int fd);
int     truncate(const char *path, off_t length);
int     ftruncate(int fd, off_t length);

/* Standard POSIX Directory and Path Calls */
int     mkdir(const char *path, mode_t mode);
int     rmdir(const char *path);
int     unlink(const char *path);
int     rename(const char *oldpath, const char *newpath);

/* Directory Traversal Calls */
DIR            *opendir(const char *path);
struct dirent  *readdir(DIR *dirp);
int             closedir(DIR *dirp);

/* Filesystem Mount Calls */
int     mount(const char *source, const char *target, const char *fstype,
              unsigned long flags, const void *data);
int     umount(const char *target);

/* POSIX-compatible BSD Socket Calls */
int     socket(int domain, int type, int protocol);
int     bind(int sockfd, const void *addr, socklen_t addrlen);
int     connect(int sockfd, const void *addr, socklen_t addrlen);
int     listen(int sockfd, int backlog);
int     accept(int sockfd, void *addr, socklen_t *addrlen);
ssize_t send(int sockfd, const void *buf, size_t len, int flags);
ssize_t recv(int sockfd, void *buf, size_t len, int flags);
ssize_t sendto(int sockfd, const void *buf, size_t len, int flags, const void *dest_addr, socklen_t addrlen);
ssize_t recvfrom(int sockfd, void *buf, size_t len, int flags, void *src_addr, socklen_t *addrlen);
int     setsockopt(int sockfd, int level, int optname, const void *optval, socklen_t optlen);
int     getsockopt(int sockfd, int level, int optname, void *optval, socklen_t *optlen);
int     shutdown(int sockfd, int how);

#ifdef __cplusplus
}
#endif

#endif /* _CHERRY_VFS_POSIX_H_ */
