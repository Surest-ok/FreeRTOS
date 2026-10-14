/*
 * CherryExt4 - Superblock & Block Group Management
 *
 * Copyright (c) 2026 ForgeStitch
 * SPDX-License-Identifier: MIT
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "cherry_kal.h"
#include "cherry_printk.h"
#include "ext4_types.h"
#include "ext4_fs.h"

int ext4_read_block(struct ext4_fs *fs, uint32_t block_nr, void *buf)
{
    if (!fs || !fs->bdev || !buf) return -1;
    uint64_t lba = (uint64_t)block_nr * fs->sectors_per_block;
    return fs->bdev->ops->read_blocks(fs->bdev, buf, lba, fs->sectors_per_block);
}

int ext4_write_block(struct ext4_fs *fs, uint32_t block_nr, const void *buf)
{
    if (!fs || !fs->bdev || !buf) return -1;
    if (fs->read_only) return -2;
    uint64_t lba = (uint64_t)block_nr * fs->sectors_per_block;
    return fs->bdev->ops->write_blocks(fs->bdev, buf, lba, fs->sectors_per_block);
}

int ext4_sync_sb_and_gdt(struct ext4_fs *fs)
{
    if (!fs || !fs->bdev) return -1;

    /* 1. Superblock is at byte 1024 */
    uint8_t *block_buf = (uint8_t *)kal_malloc(fs->block_size);
    if (!block_buf) return -12; /* ENOMEM */

    if (fs->block_size == 1024) {
        /* Superblock is block 1 */
        memcpy(block_buf, &fs->sb, sizeof(struct ext4_super_block));
        ext4_write_block(fs, 1, block_buf);
    } else {
        /* Block 0: read first, modify byte 1024..2047, write back */
        ext4_read_block(fs, 0, block_buf);
        memcpy(block_buf + 1024, &fs->sb, sizeof(struct ext4_super_block));
        ext4_write_block(fs, 0, block_buf);
    }

    /* 2. Group Descriptors starting at block after superblock */
    uint32_t gdt_block = (fs->block_size == 1024) ? 2 : 1;
    size_t gdt_bytes = fs->groups_count * sizeof(struct ext4_group_desc);
    size_t gdt_blocks = (gdt_bytes + fs->block_size - 1) / fs->block_size;

    const uint8_t *src = (const uint8_t *)fs->group_descs;
    for (uint32_t i = 0; i < gdt_blocks; i++) {
        memset(block_buf, 0, fs->block_size);
        size_t chunk = (i == gdt_blocks - 1) ? (gdt_bytes - i * fs->block_size) : fs->block_size;
        memcpy(block_buf, src + i * fs->block_size, chunk);
        ext4_write_block(fs, gdt_block + i, block_buf);
    }

    kal_free(block_buf);
    if (fs->bdev->ops->sync) {
        fs->bdev->ops->sync(fs->bdev);
    }
    return 0;
}
