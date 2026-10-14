/*
 * CherryFSTools - Partition Device Bridge & Management
 *
 * Copyright (c) 2026 ForgeStitch
 * SPDX-License-Identifier: MIT
 */

#ifndef CHERRY_FSTOOLS_FDISK_H
#define CHERRY_FSTOOLS_FDISK_H

#include <stdint.h>
#include <stdbool.h>
#include "vfs_dev.h"
#include "mbr.h"

#define FSTOOLS_MAX_PARTITIONS_PER_DEV  4

/* Sub-block device representing a physical partition */
struct part_bdev {
    struct vfs_bdev      bdev;
    struct vfs_bdev     *parent;
    uint32_t             part_num;          /* 1..4 */
    uint32_t             start_lba;         /* Offset from parent sector 0 */
    uint32_t             sector_count;      /* Length in sectors */
    uint8_t              type;              /* Partition type (0x83, etc.) */
    bool                 is_registered;
    char                 part_name[32];     /* e.g. "mmcblk0p1" */
};

/* Partition Management APIs */
int part_read_mbr(struct vfs_bdev *bdev, struct mbr_sector *out_mbr);
int part_write_mbr(struct vfs_bdev *bdev, const struct mbr_sector *in_mbr);
int part_scan_and_register(struct vfs_bdev *bdev);
int part_create(struct vfs_bdev *bdev, int part_no, uint32_t start_lba, uint32_t sectors, uint8_t type);
int part_dump_table(struct vfs_bdev *bdev);

#endif /* CHERRY_FSTOOLS_FDISK_H */
