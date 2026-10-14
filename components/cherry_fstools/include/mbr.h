/*
 * CherryFSTools - MBR (Master Boot Record) Partition Table Definitions
 *
 * Copyright (c) 2026 ForgeStitch
 * SPDX-License-Identifier: MIT
 */

#ifndef CHERRY_FSTOOLS_MBR_H
#define CHERRY_FSTOOLS_MBR_H

#include <stdint.h>
#include <stdbool.h>

#define MBR_BOOT_SIGNATURE              0xAA55
#define MBR_PARTITION_TABLE_OFFSET      446
#define MBR_PARTITION_COUNT             4

/* Common Partition Types */
#define MBR_TYPE_EMPTY                  0x00
#define MBR_TYPE_FAT12                  0x01
#define MBR_TYPE_FAT16_LESS_32M         0x04
#define MBR_TYPE_EXTENDED               0x05
#define MBR_TYPE_FAT16                  0x06
#define MBR_TYPE_NTFS_EXFAT             0x07
#define MBR_TYPE_FAT32_CHS              0x0B
#define MBR_TYPE_FAT32_LBA              0x0C
#define MBR_TYPE_FAT16_LBA              0x0E
#define MBR_TYPE_LINUX_SWAP             0x82
#define MBR_TYPE_LINUX                  0x83    /* Standard Linux native (ext2/3/4) */
#define MBR_TYPE_LINUX_EXTENDED         0x85
#define MBR_TYPE_LINUX_LVM              0x8E

#pragma pack(push, 1)

/* Standard 16-byte MBR Partition Record */
struct mbr_partition {
    uint8_t  status;                    /* 0x80 = Active / Bootable, 0x00 = Inactive */
    uint8_t  start_chs[3];              /* CHS start address */
    uint8_t  type;                      /* Partition type ID (MBR_TYPE_*) */
    uint8_t  end_chs[3];                /* CHS end address */
    uint32_t start_lba;                 /* LBA start sector */
    uint32_t sector_count;              /* Total sectors in partition */
};

/* Standard 512-byte Master Boot Record */
struct mbr_sector {
    uint8_t              bootstrap[446];/* Bootstrap code area */
    struct mbr_partition parts[4];      /* 4 Primary Partition Records */
    uint16_t             signature;     /* Boot signature: 0xAA55 (stored as 0x55, 0xAA) */
};

#pragma pack(pop)

#endif /* CHERRY_FSTOOLS_MBR_H */
