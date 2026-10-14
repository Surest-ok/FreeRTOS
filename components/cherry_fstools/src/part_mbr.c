/*
 * CherryFSTools - MBR Partition Table Parser & Writer
 *
 * Copyright (c) 2026 ForgeStitch
 * SPDX-License-Identifier: MIT
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "cherry_kal.h"
#include "cherry_printk.h"
#include "mbr.h"
#include "fdisk.h"

int part_read_mbr(struct vfs_bdev *bdev, struct mbr_sector *out_mbr)
{
    if (!bdev || !bdev->ops || !out_mbr) return -1;

    uint8_t sector_buf[512];
    if (bdev->ops->read_blocks(bdev, sector_buf, 0, 1) != 0) {
        return -5; /* EIO */
    }

    uint16_t sig = (uint16_t)sector_buf[510] | ((uint16_t)sector_buf[511] << 8);
    if (sig != MBR_BOOT_SIGNATURE) {
        return -22; /* EINVAL: Not an MBR */
    }

    memcpy(out_mbr, sector_buf, sizeof(struct mbr_sector));
    return 0;
}

int part_write_mbr(struct vfs_bdev *bdev, const struct mbr_sector *in_mbr)
{
    if (!bdev || !bdev->ops || !in_mbr) return -1;

    uint8_t sector_buf[512];
    memcpy(sector_buf, in_mbr, sizeof(struct mbr_sector));
    sector_buf[510] = 0x55;
    sector_buf[511] = 0xAA;

    int ret = bdev->ops->write_blocks(bdev, sector_buf, 0, 1);
    if (ret == 0 && bdev->ops->sync) {
        bdev->ops->sync(bdev);
    }
    return ret;
}

static const char *part_type_to_str(uint8_t type)
{
    switch (type) {
    case MBR_TYPE_EMPTY: return "Empty";
    case MBR_TYPE_FAT12: return "FAT12";
    case MBR_TYPE_FAT16: return "FAT16";
    case MBR_TYPE_NTFS_EXFAT: return "NTFS/exFAT";
    case MBR_TYPE_FAT32_CHS:
    case MBR_TYPE_FAT32_LBA: return "FAT32";
    case MBR_TYPE_LINUX_SWAP: return "Linux swap";
    case MBR_TYPE_LINUX: return "Linux (ext2/3/4)";
    case MBR_TYPE_LINUX_LVM: return "Linux LVM";
    default: return "Unknown";
    }
}

int part_dump_table(struct vfs_bdev *bdev)
{
    if (!bdev) return -1;

    struct mbr_sector mbr;
    int ret = part_read_mbr(bdev, &mbr);
    if (ret != 0) {
        printk("fdisk: no valid mbr partition table found on '%s'\n", bdev->dev.name);
        return ret;
    }

    printk("Disk /dev/%s: %lu sectors, %lu MB\n",
           bdev->dev.name, (unsigned long)bdev->total_blocks,
           (unsigned long)(bdev->total_blocks * 512 / (1024 * 1024)));
    printk("Device         Boot   Start      End  Sectors  Size(MB)  Id  Type\n");
    printk("----------------------------------------------------------------------\n");

    for (int i = 0; i < 4; i++) {
        struct mbr_partition *p = &mbr.parts[i];
        if (p->sector_count == 0 || p->type == MBR_TYPE_EMPTY) {
            continue;
        }

        uint32_t end_lba = p->start_lba + p->sector_count - 1;
        uint32_t size_mb = (uint32_t)((uint64_t)p->sector_count * 512 / (1024 * 1024));

        printk("/dev/%sp%d    %c   %8u %8u %8u  %8u  %02x  %s\n",
               bdev->dev.name, i + 1,
               (p->status & 0x80) ? '*' : ' ',
               p->start_lba, end_lba, p->sector_count, size_mb,
               p->type, part_type_to_str(p->type));
    }
    return 0;
}
