/*
 * CherryVFS - Virtual File System for FreeRTOS
 *
 * Copyright (c) 2026 ForgeStitch
 * SPDX-License-Identifier: MIT
 *
 * Device Core: Character, Block, and Socket Device Registration & Lookup
 */

#include "cherry_vfs.h"
#include <string.h>

static struct vfs_device *g_device_list = NULL;
static dev_t g_next_dev_id = 1;

int vfs_device_register(struct vfs_device *dev)
{
    if (!dev || dev->name[0] == '\0') {
        return -EINVAL;
    }

    vfs_lock();
    struct vfs_device *cur = g_device_list;
    while (cur) {
        if (strcmp(cur->name, dev->name) == 0) {
            vfs_unlock();
            return -EEXIST;
        }
        cur = cur->next;
    }

    if (dev->dev_id == 0) {
        dev->dev_id = g_next_dev_id++;
    }
    dev->ref_count = 1;
    dev->next = g_device_list;
    g_device_list = dev;
    vfs_unlock();

    return VFS_OK;
}

int vfs_device_unregister(struct vfs_device *dev)
{
    if (!dev) return -EINVAL;

    vfs_lock();
    struct vfs_device **curr = &g_device_list;
    while (*curr) {
        if (*curr == dev) {
            *curr = dev->next;
            vfs_unlock();
            return VFS_OK;
        }
        curr = &(*curr)->next;
    }
    vfs_unlock();
    return -ENOENT;
}

struct vfs_device *vfs_device_find(const char *name)
{
    if (!name) return NULL;

    vfs_lock();
    struct vfs_device *cur = g_device_list;
    while (cur) {
        if (strcmp(cur->name, name) == 0) {
            vfs_unlock();
            return cur;
        }
        cur = cur->next;
    }
    vfs_unlock();
    return NULL;
}

int vfs_device_for_each(void *data, int (*callback)(struct vfs_device *dev, void *data))
{
    if (!callback) return -EINVAL;

    vfs_lock();
    struct vfs_device *cur = g_device_list;
    while (cur) {
        int ret = callback(cur, data);
        if (ret != 0) {
            vfs_unlock();
            return ret;
        }
        cur = cur->next;
    }
    vfs_unlock();
    return VFS_OK;
}

/* =========================================================================
 * 1. Character Device (cdev)
 * ========================================================================= */
int vfs_cdev_register(struct vfs_cdev *cdev, const char *name)
{
    if (!cdev || !name || !cdev->ops) {
        return -EINVAL;
    }
    strncpy(cdev->dev.name, name, VFS_NAME_MAX - 1);
    cdev->dev.name[VFS_NAME_MAX - 1] = '\0';
    cdev->dev.type = VFS_DEV_TYPE_CHAR;
    return vfs_device_register(&cdev->dev);
}

int vfs_cdev_unregister(struct vfs_cdev *cdev)
{
    if (!cdev) return -EINVAL;
    return vfs_device_unregister(&cdev->dev);
}

struct vfs_cdev *vfs_cdev_find(const char *name)
{
    struct vfs_device *dev = vfs_device_find(name);
    if (dev && dev->type == VFS_DEV_TYPE_CHAR) {
        return (struct vfs_cdev *)dev;
    }
    return NULL;
}

/* =========================================================================
 * 2. Block Device (bdev)
 * ========================================================================= */
int vfs_bdev_register(struct vfs_bdev *bdev, const char *name, uint32_t block_size, uint64_t total_blocks)
{
    if (!bdev || !name || !bdev->ops || block_size == 0) {
        return -EINVAL;
    }
    strncpy(bdev->dev.name, name, VFS_NAME_MAX - 1);
    bdev->dev.name[VFS_NAME_MAX - 1] = '\0';
    bdev->dev.type = VFS_DEV_TYPE_BLOCK;
    bdev->block_size = block_size;
    bdev->total_blocks = total_blocks;
    return vfs_device_register(&bdev->dev);
}

int vfs_bdev_unregister(struct vfs_bdev *bdev)
{
    if (!bdev) return -EINVAL;
    return vfs_device_unregister(&bdev->dev);
}

struct vfs_bdev *vfs_bdev_find(const char *name)
{
    struct vfs_device *dev = vfs_device_find(name);
    if (dev && dev->type == VFS_DEV_TYPE_BLOCK) {
        return (struct vfs_bdev *)dev;
    }
    return NULL;
}

/* =========================================================================
 * 3. Socket Device (sockdev)
 * ========================================================================= */
int vfs_sockdev_register(struct vfs_sockdev *sdev, const char *name)
{
    if (!sdev || !name || !sdev->ops) {
        return -EINVAL;
    }
    strncpy(sdev->dev.name, name, VFS_NAME_MAX - 1);
    sdev->dev.name[VFS_NAME_MAX - 1] = '\0';
    sdev->dev.type = VFS_DEV_TYPE_SOCKET;
    return vfs_device_register(&sdev->dev);
}

int vfs_sockdev_unregister(struct vfs_sockdev *sdev)
{
    if (!sdev) return -EINVAL;
    return vfs_device_unregister(&sdev->dev);
}

struct vfs_sockdev *vfs_sockdev_find(const char *name)
{
    struct vfs_device *dev = vfs_device_find(name);
    if (dev && dev->type == VFS_DEV_TYPE_SOCKET) {
        return (struct vfs_sockdev *)dev;
    }
    return NULL;
}
