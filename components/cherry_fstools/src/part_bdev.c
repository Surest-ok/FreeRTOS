/*
 * CherryFSTools - Partition Sub-Block Device Registration & Bridge
 *
 * Copyright (c) 2026 ForgeStitch
 * SPDX-License-Identifier: MIT
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "cherry_kal.h"
#include "cherry_printk.h"
#include "vfs_dev.h"
#include "mbr.h"
#include "fdisk.h"

static int part_bdev_open(struct vfs_bdev *bdev, int flags)
{
    (void)bdev;
    (void)flags;
    return 0;
}

static int part_bdev_close(struct vfs_bdev *bdev)
{
    (void)bdev;
    return 0;
}

static int part_bdev_read(struct vfs_bdev *bdev, void *buf, uint64_t lba, size_t num_blocks)
{
    struct part_bdev *part = (struct part_bdev *)bdev;
    if (!part || !part->parent || !part->parent->ops) return -1;
    if (lba + num_blocks > part->sector_count) return -28; /* Out of bounds */

    uint64_t parent_lba = (uint64_t)part->start_lba + lba;
    return part->parent->ops->read_blocks(part->parent, buf, parent_lba, num_blocks);
}

static int part_bdev_write(struct vfs_bdev *bdev, const void *buf, uint64_t lba, size_t num_blocks)
{
    struct part_bdev *part = (struct part_bdev *)bdev;
    if (!part || !part->parent || !part->parent->ops) return -1;
    if (lba + num_blocks > part->sector_count) return -28;

    uint64_t parent_lba = (uint64_t)part->start_lba + lba;
    return part->parent->ops->write_blocks(part->parent, buf, parent_lba, num_blocks);
}

static int part_bdev_sync(struct vfs_bdev *bdev)
{
    struct part_bdev *part = (struct part_bdev *)bdev;
    if (part && part->parent && part->parent->ops && part->parent->ops->sync) {
        return part->parent->ops->sync(part->parent);
    }
    return 0;
}

static int part_bdev_ioctl(struct vfs_bdev *bdev, unsigned int cmd, unsigned long arg)
{
    struct part_bdev *part = (struct part_bdev *)bdev;
    if (!part) return -1;

    switch (cmd) {
    case BLOCK_IOCTL_GET_SECTOR_SIZE:
    case BLOCK_IOCTL_GET_BLOCK_SIZE:
        if (arg) *(uint32_t *)arg = 512;
        return 0;
    case BLOCK_IOCTL_GET_SECTOR_COUNT:
        if (arg) *(uint64_t *)arg = part->sector_count;
        return 0;
    case BLOCK_IOCTL_SYNC:
        return part_bdev_sync(bdev);
    default:
        return -22;
    }
}

static const struct vfs_bdev_ops s_part_bdev_ops = {
    .open         = part_bdev_open,
    .close        = part_bdev_close,
    .read_blocks  = part_bdev_read,
    .write_blocks = part_bdev_write,
    .sync         = part_bdev_sync,
    .ioctl        = part_bdev_ioctl,
};

static struct part_bdev s_partitions[16];
static int s_part_count = 0;

int part_scan_and_register(struct vfs_bdev *bdev)
{
    if (!bdev) return -1;

    struct mbr_sector mbr;
    if (part_read_mbr(bdev, &mbr) != 0) {
        return -1;
    }

    int registered = 0;
    for (int i = 0; i < 4; i++) {
        struct mbr_partition *p = &mbr.parts[i];
        if (p->sector_count == 0 || p->type == MBR_TYPE_EMPTY) {
            continue;
        }

        /* Check if already registered */
        struct part_bdev *existing = NULL;
        for (int k = 0; k < s_part_count; k++) {
            if (s_partitions[k].parent == bdev && s_partitions[k].part_num == (uint32_t)(i + 1)) {
                existing = &s_partitions[k];
                break;
            }
        }

        if (existing) {
            existing->start_lba = p->start_lba;
            existing->sector_count = p->sector_count;
            existing->type = p->type;
            existing->bdev.total_blocks = p->sector_count;
            registered++;
            continue;
        }

        if (s_part_count >= 16) {
            pr_warn("cherry_fstools: partition table full (max 16)\n");
            break;
        }

        struct part_bdev *part = &s_partitions[s_part_count++];
        memset(part, 0, sizeof(*part));

        part->parent = bdev;
        part->part_num = (uint32_t)(i + 1);
        part->start_lba = p->start_lba;
        part->sector_count = p->sector_count;
        part->type = p->type;
        part->bdev.ops = &s_part_bdev_ops;

        /* Form name: e.g. "mmcblk0p1" or "sd0p1" */
        snprintf(part->part_name, sizeof(part->part_name), "%sp%d", bdev->dev.name, i + 1);

        vfs_bdev_register(&part->bdev, part->part_name, 512, part->sector_count);
        part->is_registered = true;
        registered++;

        pr_info("cherry_fstools: registered partition device /dev/%s (start=%u, count=%u, type=0x%02x)\n",
                part->part_name, part->start_lba, part->sector_count, part->type);
    }

    return registered;
}

int part_create(struct vfs_bdev *bdev, int part_no, uint32_t start_lba, uint32_t sectors, uint8_t type)
{
    if (!bdev || part_no < 1 || part_no > 4 || sectors == 0) return -1;

    struct mbr_sector mbr;
    int ret = part_read_mbr(bdev, &mbr);
    if (ret != 0) {
        /* Initialize new empty MBR if none exists */
        memset(&mbr, 0, sizeof(mbr));
        mbr.signature = MBR_BOOT_SIGNATURE;
    }

    int idx = part_no - 1;
    mbr.parts[idx].status = 0x80; /* Active */
    mbr.parts[idx].type = type ? type : MBR_TYPE_LINUX;
    mbr.parts[idx].start_lba = start_lba;
    mbr.parts[idx].sector_count = sectors;

    ret = part_write_mbr(bdev, &mbr);
    if (ret != 0) {
        pr_err("cherry_fstools: failed to write mbr to '%s'\n", bdev->dev.name);
        return ret;
    }

    pr_info("cherry_fstools: partition %d created on '%s' (start=%u, sectors=%u, type=0x%02x)\n",
            part_no, bdev->dev.name, start_lba, sectors, mbr.parts[idx].type);

    return part_scan_and_register(bdev);
}
