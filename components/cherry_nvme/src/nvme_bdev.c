/*
 * CherryNVMe - CherryVFS Block Device Bridge (/dev/nvme0n1)
 *
 * Copyright (c) 2026 ForgeStitch
 * SPDX-License-Identifier: MIT
 */

#include <stdint.h>
#include "cherry_nvme.h"
#include "cherry_vfs.h"
#include "vfs_dev.h"
#include "cherry_printk.h"

static int nvme_bdev_open(struct vfs_bdev *bdev, int flags)
{
    (void)bdev;
    (void)flags;
    return 0;
}

static int nvme_bdev_close(struct vfs_bdev *bdev)
{
    (void)bdev;
    return 0;
}

static int nvme_bdev_read(struct vfs_bdev *bdev, void *buf, uint64_t lba, size_t num_blocks)
{
    struct nvme_ns *ns = (struct nvme_ns *)bdev->dev.priv_data;
    if (!ns || !ns->dev) return -1;
    return nvme_read_blocks(ns->dev, ns->nsid, lba, (uint32_t)num_blocks, buf);
}

static int nvme_bdev_write(struct vfs_bdev *bdev, const void *buf, uint64_t lba, size_t num_blocks)
{
    struct nvme_ns *ns = (struct nvme_ns *)bdev->dev.priv_data;
    if (!ns || !ns->dev) return -1;
    return nvme_write_blocks(ns->dev, ns->nsid, lba, (uint32_t)num_blocks, buf);
}

static int nvme_bdev_sync(struct vfs_bdev *bdev)
{
    struct nvme_ns *ns = (struct nvme_ns *)bdev->dev.priv_data;
    if (!ns || !ns->dev) return -1;
    return nvme_flush(ns->dev, ns->nsid);
}

static int nvme_bdev_ioctl(struct vfs_bdev *bdev, unsigned int cmd, unsigned long arg)
{
    struct nvme_ns *ns = (struct nvme_ns *)bdev->dev.priv_data;
    if (!ns || !ns->dev) return -1;

    switch (cmd) {
    case BLOCK_IOCTL_GET_SECTOR_SIZE:
    case BLOCK_IOCTL_GET_BLOCK_SIZE:
        if (arg) *(uint32_t *)arg = ns->block_size;
        return 0;
    case BLOCK_IOCTL_GET_SECTOR_COUNT:
        if (arg) *(uint64_t *)arg = ns->block_count;
        return 0;
    case BLOCK_IOCTL_SYNC:
        return nvme_flush(ns->dev, ns->nsid);
    default:
        return -22;
    }
}

static const struct vfs_bdev_ops s_nvme_bdev_ops = {
    .open         = nvme_bdev_open,
    .close        = nvme_bdev_close,
    .read_blocks  = nvme_bdev_read,
    .write_blocks = nvme_bdev_write,
    .sync         = nvme_bdev_sync,
    .ioctl        = nvme_bdev_ioctl,
};

int nvme_bdev_register(struct nvme_dev *dev)
{
    struct nvme_ns *ns = &dev->ns;
    uint32_t blk_size = ns->block_size ? ns->block_size : 512;
    uint64_t blk_count = ns->block_count;

    ns->bdev.ops = &s_nvme_bdev_ops;
    ns->bdev.dev.priv_data = ns;
    vfs_bdev_register(&ns->bdev, "nvme0n1", blk_size, blk_count);

    ns->alias_bdev.ops = &s_nvme_bdev_ops;
    ns->alias_bdev.dev.priv_data = ns;
    vfs_bdev_register(&ns->alias_bdev, "nvme0", blk_size, blk_count);

    pr_info("cherry_nvme: registered block device /dev/nvme0n1 (%llu blocks, %u bytes/block)\n",
            (unsigned long long)blk_count, blk_size);
    return 0;
}
