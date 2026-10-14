/*
 * CherryVFS - Virtual File System for FreeRTOS
 *
 * Copyright (c) 2026 ForgeStitch
 * SPDX-License-Identifier: MIT
 *
 * Unified Device Model: Character, Block, and Socket Devices
 */

#ifndef _CHERRY_VFS_DEV_H_
#define _CHERRY_VFS_DEV_H_

#include "vfs_types.h"
#include "vfs_inode.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Device Types */
typedef enum {
    VFS_DEV_TYPE_UNKNOWN = 0,
    VFS_DEV_TYPE_CHAR,       /* Character device (stream oriented) */
    VFS_DEV_TYPE_BLOCK,      /* Block device (sector / block oriented) */
    VFS_DEV_TYPE_SOCKET,     /* Socket network endpoint device */
    VFS_DEV_TYPE_BUS,        /* Bus device */
} vfs_dev_type_t;

struct vfs_device;
struct vfs_driver;
struct vfs_bus_type;

/* Base Device Structure */
struct vfs_device {
    char                    name[VFS_NAME_MAX]; /* Device name (e.g. "uart0", "ram0") */
    vfs_dev_type_t          type;               /* Device type */
    dev_t                   dev_id;             /* Major / Minor device number */
    struct vfs_driver      *driver;             /* Bound driver if any */
    struct vfs_bus_type    *bus;                /* Bus where device resides */
    void                   *priv_data;          /* Driver/device private context */
    uint32_t                ref_count;          /* Reference count */
    struct vfs_device      *next;               /* Global device list linkage */
    struct vfs_device      *bus_next;           /* Bus device list linkage */
};


/* =========================================================================
 * 1. Character Device (cdev) Abstraction
 * ========================================================================= */
struct vfs_cdev;

struct vfs_cdev_ops {
    int     (*open)(struct vfs_cdev *cdev, int flags);
    int     (*close)(struct vfs_cdev *cdev);
    ssize_t (*read)(struct vfs_cdev *cdev, void *buf, size_t count);
    ssize_t (*write)(struct vfs_cdev *cdev, const void *buf, size_t count);
    int     (*ioctl)(struct vfs_cdev *cdev, unsigned int cmd, unsigned long arg);
    int     (*poll)(struct vfs_cdev *cdev, void *poll_table);
};

struct vfs_cdev {
    struct vfs_device           dev;            /* Inherit base device */
    const struct vfs_cdev_ops  *ops;            /* Character device operations */
};

int vfs_cdev_register(struct vfs_cdev *cdev, const char *name);
int vfs_cdev_unregister(struct vfs_cdev *cdev);
struct vfs_cdev *vfs_cdev_find(const char *name);

/* =========================================================================
 * 2. Block Device (bdev) Abstraction
 * ========================================================================= */
struct vfs_bdev;

/* Standard Block Device IOCTL Commands */
#define BLOCK_IOCTL_GET_SECTOR_SIZE     0x1001
#define BLOCK_IOCTL_GET_SECTOR_COUNT    0x1002
#define BLOCK_IOCTL_GET_BLOCK_SIZE      0x1003
#define BLOCK_IOCTL_SYNC                0x1004
#define BLOCK_IOCTL_TRIM                0x1005

struct vfs_bdev_ops {
    int     (*open)(struct vfs_bdev *bdev, int flags);
    int     (*close)(struct vfs_bdev *bdev);
    int     (*read_blocks)(struct vfs_bdev *bdev, void *buf, uint64_t lba, size_t num_blocks);
    int     (*write_blocks)(struct vfs_bdev *bdev, const void *buf, uint64_t lba, size_t num_blocks);
    int     (*sync)(struct vfs_bdev *bdev);
    int     (*ioctl)(struct vfs_bdev *bdev, unsigned int cmd, unsigned long arg);
};

struct vfs_bdev {
    struct vfs_device           dev;            /* Inherit base device */
    uint32_t                    block_size;     /* Block/Sector size in bytes (e.g. 512, 4096) */
    uint64_t                    total_blocks;   /* Total number of blocks */
    const struct vfs_bdev_ops  *ops;            /* Block device operations */
};

int vfs_bdev_register(struct vfs_bdev *bdev, const char *name, uint32_t block_size, uint64_t total_blocks);
int vfs_bdev_unregister(struct vfs_bdev *bdev);
struct vfs_bdev *vfs_bdev_find(const char *name);

/* =========================================================================
 * 3. Socket Device (sockdev) Abstraction
 * ========================================================================= */
struct vfs_sockdev;

struct vfs_sock_ops {
    void   *(*socket)(int domain, int type, int protocol);
    int     (*bind)(void *sock, const void *addr, size_t addrlen);
    int     (*connect)(void *sock, const void *addr, size_t addrlen);
    int     (*listen)(void *sock, int backlog);
    void   *(*accept)(void *sock, void *addr, size_t *addrlen);
    ssize_t (*send)(void *sock, const void *buf, size_t len, int flags);
    ssize_t (*recv)(void *sock, void *buf, size_t len, int flags);
    int     (*shutdown)(void *sock, int how);
    int     (*close)(void *sock);
};

struct vfs_sockdev {
    struct vfs_device           dev;            /* Inherit base device */
    const struct vfs_sock_ops  *ops;            /* Socket operations */
};

int vfs_sockdev_register(struct vfs_sockdev *sdev, const char *name);
int vfs_sockdev_unregister(struct vfs_sockdev *sdev);
struct vfs_sockdev *vfs_sockdev_find(const char *name);

/* Generic Device Enumeration and Traversal */
int vfs_device_register(struct vfs_device *dev);
int vfs_device_unregister(struct vfs_device *dev);
struct vfs_device *vfs_device_find(const char *name);
int vfs_device_for_each(void *data, int (*callback)(struct vfs_device *dev, void *data));

#ifdef __cplusplus
}
#endif

#endif /* _CHERRY_VFS_DEV_H_ */
