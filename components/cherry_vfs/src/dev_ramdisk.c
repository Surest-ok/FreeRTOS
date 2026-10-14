/*
 * CherryVFS - Virtual File System for FreeRTOS
 *
 * Copyright (c) 2026 ForgeStitch
 * SPDX-License-Identifier: MIT
 *
 * Ramdisk Block Device: /dev/ram0
 */

#include "cherry_vfs.h"
#include <string.h>

#define RAMDISK_SECTOR_SIZE     512
#define RAMDISK_NUM_SECTORS     64          /* 32 KB Ramdisk */
#define RAMDISK_TOTAL_BYTES     (RAMDISK_SECTOR_SIZE * RAMDISK_NUM_SECTORS)

static uint8_t g_ramdisk_storage[RAMDISK_TOTAL_BYTES];

static int ramdisk_open(struct vfs_bdev *bdev, int flags)
{
    (void)bdev;
    (void)flags;
    return VFS_OK;
}

static int ramdisk_close(struct vfs_bdev *bdev)
{
    (void)bdev;
    return VFS_OK;
}

static int ramdisk_read_blocks(struct vfs_bdev *bdev, void *buf, uint64_t lba, size_t num_blocks)
{
    (void)bdev;
    if (!buf) return -EINVAL;
    if (lba + num_blocks > RAMDISK_NUM_SECTORS) {
        return -EINVAL;
    }

    uint8_t *src = g_ramdisk_storage + (lba * RAMDISK_SECTOR_SIZE);
    memcpy(buf, src, num_blocks * RAMDISK_SECTOR_SIZE);
    return VFS_OK;
}

static int ramdisk_write_blocks(struct vfs_bdev *bdev, const void *buf, uint64_t lba, size_t num_blocks)
{
    (void)bdev;
    if (!buf) return -EINVAL;
    if (lba + num_blocks > RAMDISK_NUM_SECTORS) {
        return -EINVAL;
    }

    uint8_t *dst = g_ramdisk_storage + (lba * RAMDISK_SECTOR_SIZE);
    memcpy(dst, buf, num_blocks * RAMDISK_SECTOR_SIZE);
    return VFS_OK;
}

static int ramdisk_sync(struct vfs_bdev *bdev)
{
    (void)bdev;
    return VFS_OK;
}

static int ramdisk_ioctl(struct vfs_bdev *bdev, unsigned int cmd, unsigned long arg)
{
    (void)bdev;
    switch (cmd) {
    case BLOCK_IOCTL_GET_SECTOR_SIZE:
    case BLOCK_IOCTL_GET_BLOCK_SIZE:
        if (arg) {
            *(uint32_t *)arg = RAMDISK_SECTOR_SIZE;
            return VFS_OK;
        }
        return -EINVAL;
    case BLOCK_IOCTL_GET_SECTOR_COUNT:
        if (arg) {
            *(uint64_t *)arg = RAMDISK_NUM_SECTORS;
            return VFS_OK;
        }
        return -EINVAL;
    case BLOCK_IOCTL_SYNC:
        return VFS_OK;
    default:
        return -ENOTTY;
    }
}

static const struct vfs_bdev_ops g_ramdisk_ops = {
    .open = ramdisk_open,
    .close = ramdisk_close,
    .read_blocks = ramdisk_read_blocks,
    .write_blocks = ramdisk_write_blocks,
    .sync = ramdisk_sync,
    .ioctl = ramdisk_ioctl,
};

static struct vfs_bdev g_ramdisk_bdev = {
    .ops = &g_ramdisk_ops,
};

int dev_ramdisk_init(void)
{
    memset(g_ramdisk_storage, 0, sizeof(g_ramdisk_storage));
    return vfs_bdev_register(&g_ramdisk_bdev, "ram0", RAMDISK_SECTOR_SIZE, RAMDISK_NUM_SECTORS);
}
