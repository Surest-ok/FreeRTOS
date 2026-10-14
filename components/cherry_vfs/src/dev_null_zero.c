/*
 * CherryVFS - Virtual File System for FreeRTOS
 *
 * Copyright (c) 2026 ForgeStitch
 * SPDX-License-Identifier: MIT
 *
 * Pseudo Character Devices: /dev/null and /dev/zero
 */

#include "cherry_vfs.h"
#include <string.h>

/* =========================================================================
 * 1. /dev/null Implementation
 * ========================================================================= */
static int null_open(struct vfs_cdev *cdev, int flags)
{
    (void)cdev;
    (void)flags;
    return VFS_OK;
}

static int null_close(struct vfs_cdev *cdev)
{
    (void)cdev;
    return VFS_OK;
}

static ssize_t null_read(struct vfs_cdev *cdev, void *buf, size_t count)
{
    (void)cdev;
    (void)buf;
    (void)count;
    return 0; /* EOF */
}

static ssize_t null_write(struct vfs_cdev *cdev, const void *buf, size_t count)
{
    (void)cdev;
    (void)buf;
    return (ssize_t)count; /* Discard bytes, return count written */
}

static const struct vfs_cdev_ops g_null_ops = {
    .open = null_open,
    .close = null_close,
    .read = null_read,
    .write = null_write,
    .ioctl = NULL,
    .poll = NULL,
};

static struct vfs_cdev g_null_cdev = {
    .ops = &g_null_ops,
};

/* =========================================================================
 * 2. /dev/zero Implementation
 * ========================================================================= */
static int zero_open(struct vfs_cdev *cdev, int flags)
{
    (void)cdev;
    (void)flags;
    return VFS_OK;
}

static int zero_close(struct vfs_cdev *cdev)
{
    (void)cdev;
    return VFS_OK;
}

static ssize_t zero_read(struct vfs_cdev *cdev, void *buf, size_t count)
{
    (void)cdev;
    if (buf && count > 0) {
        memset(buf, 0, count);
    }
    return (ssize_t)count;
}

static ssize_t zero_write(struct vfs_cdev *cdev, const void *buf, size_t count)
{
    (void)cdev;
    (void)buf;
    return (ssize_t)count; /* Discard bytes */
}

static const struct vfs_cdev_ops g_zero_ops = {
    .open = zero_open,
    .close = zero_close,
    .read = zero_read,
    .write = zero_write,
    .ioctl = NULL,
    .poll = NULL,
};

static struct vfs_cdev g_zero_cdev = {
    .ops = &g_zero_ops,
};

int dev_null_zero_init(void)
{
    int ret = vfs_cdev_register(&g_null_cdev, "null");
    if (ret != VFS_OK) return ret;

    return vfs_cdev_register(&g_zero_cdev, "zero");
}
