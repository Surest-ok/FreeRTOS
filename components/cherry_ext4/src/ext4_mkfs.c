/*
 * CherryExt4 - mkfs.ext4 Filesystem Formatter
 *
 * Formats a block device with a standard Linux-compatible Ext4 filesystem
 * including Extents, Directory Types, Inodes Table, and JBD2 Journal.
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
#include "ext4_types.h"
#include "ext4_jbd2.h"
#include "ext4_fs.h"

int ext4_mkfs(const char *dev_name, const char *vol_name)
{
    if (!dev_name) return -1;

    struct vfs_bdev *bdev = vfs_bdev_find(dev_name);
    if (!bdev && dev_name[0] == '/') {
        bdev = vfs_bdev_find(dev_name + 5); /* skip "/dev/" */
    }
    if (!bdev) {
        pr_err("cherry_ext4: mkfs target block device '%s' not found\n", dev_name);
        return -2; /* ENODEV */
    }

    uint64_t total_sectors = bdev->total_blocks;
    if (total_sectors < 2048) {
        pr_err("cherry_ext4: device too small for ext4 (sectors=%lu)\n", (unsigned long)total_sectors);
        return -28; /* ENOSPC */
    }

    /* Standard block size 1024 bytes */
    uint32_t block_size = 1024;
    uint32_t total_blocks = (uint32_t)(total_sectors / 2);

    /* Compute geometry */
    uint32_t blocks_per_group = 8192;
    if (total_blocks < blocks_per_group) {
        blocks_per_group = total_blocks;
    }
    uint32_t groups_count = (total_blocks + blocks_per_group - 1) / blocks_per_group;
    uint32_t inodes_per_group = 1024;
    uint32_t inode_size = 128;
    uint32_t itable_blocks = (inodes_per_group * inode_size) / block_size;

    /* Journal allocation: 1024 blocks (1MB) or 512 blocks for small media */
    uint32_t journal_blocks = 1024;
    if (total_blocks < 8192) {
        journal_blocks = 512;
    }

    pr_info("cherry_ext4: formatting '%s' with ext4 (size=%lu mb, blocks=%u, blksize=1024)...\n",
            dev_name, (unsigned long)(total_sectors * 512 / (1024 * 1024)), total_blocks);

    uint8_t *buf = (uint8_t *)kal_malloc(block_size);
    if (!buf) return -12;

    /* 1. Superblock definition */
    struct ext4_super_block sb;
    memset(&sb, 0, sizeof(sb));

    sb.s_inodes_count = groups_count * inodes_per_group;
    sb.s_blocks_count_lo = total_blocks;
    sb.s_r_blocks_count_lo = total_blocks / 20; /* 5% reserved */
    sb.s_first_data_block = 1;
    sb.s_log_block_size = 0; /* 1024 bytes */
    sb.s_log_cluster_size = 0;
    sb.s_blocks_per_group = blocks_per_group;
    sb.s_clusters_per_group = blocks_per_group;
    sb.s_inodes_per_group = inodes_per_group;
    sb.s_magic = EXT4_SUPER_MAGIC;
    sb.s_state = EXT4_VALID_FS;
    sb.s_errors = 1; /* continue on errors */
    sb.s_rev_level = EXT4_DYNAMIC_REV;
    sb.s_first_ino = EXT4_GOOD_OLD_FIRST_INO;
    sb.s_inode_size = (uint16_t)inode_size;
    sb.s_feature_compat = EXT4_FEATURE_COMPAT_HAS_JOURNAL;
    sb.s_feature_incompat = EXT4_FEATURE_INCOMPAT_FILETYPE | EXT4_FEATURE_INCOMPAT_EXTENTS;
    sb.s_feature_ro_compat = EXT4_FEATURE_RO_COMPAT_SPARSE_SUPER;
    sb.s_journal_inum = EXT4_JOURNAL_INO;

    if (vol_name && strlen(vol_name) > 0) {
        strncpy(sb.s_volume_name, vol_name, sizeof(sb.s_volume_name) - 1);
    } else {
        strncpy(sb.s_volume_name, "cherry_ext4", sizeof(sb.s_volume_name) - 1);
    }

    /* Layout in Group 0:
     * Block 0: Boot
     * Block 1: Superblock
     * Block 2: GDT (Group Descriptor Table)
     * Block 3: Block Bitmap
     * Block 4: Inode Bitmap
     * Block 5 .. 5 + itable_blocks - 1: Inode Table
     * Block root_dir_block = 5 + itable_blocks
     * Block journal_start_block = root_dir_block + 1
     */
    uint32_t b_bitmap_blk = 3;
    uint32_t i_bitmap_blk = 4;
    uint32_t itable_start_blk = 5;
    uint32_t root_dir_blk = itable_start_blk + itable_blocks;
    uint32_t journal_start_blk = root_dir_blk + 1;
    uint32_t overhead_blocks = journal_start_blk + journal_blocks;

    sb.s_free_blocks_count_lo = total_blocks - overhead_blocks;
    sb.s_free_inodes_count = sb.s_inodes_count - 11; /* 1..10 used, 11 is first free */

    /* 2. Group Descriptors */
    struct ext4_group_desc *gdt = (struct ext4_group_desc *)kal_malloc(groups_count * sizeof(struct ext4_group_desc));
    if (!gdt) {
        kal_free(buf);
        return -12;
    }
    memset(gdt, 0, groups_count * sizeof(struct ext4_group_desc));

    for (uint32_t g = 0; g < groups_count; g++) {
        gdt[g].bg_block_bitmap_lo = b_bitmap_blk;
        gdt[g].bg_inode_bitmap_lo = i_bitmap_blk;
        gdt[g].bg_inode_table_lo  = itable_start_blk;
        gdt[g].bg_free_blocks_count_lo = (uint16_t)(sb.s_free_blocks_count_lo / groups_count);
        gdt[g].bg_free_inodes_count_lo = (uint16_t)(inodes_per_group - 11);
        gdt[g].bg_used_dirs_count_lo = (g == 0) ? 1 : 0;
    }

    /* Helper: write block to bdev */
    #define WRITE_BLOCK(blk, p) bdev->ops->write_blocks(bdev, p, (uint64_t)(blk) * 2, 2)

    /* Write Block 0 (boot) */
    memset(buf, 0, block_size);
    WRITE_BLOCK(0, buf);

    /* Write Block 1 (Superblock) */
    memcpy(buf, &sb, sizeof(sb));
    WRITE_BLOCK(1, buf);

    /* Write Block 2 (GDT) */
    memset(buf, 0, block_size);
    memcpy(buf, gdt, groups_count * sizeof(struct ext4_group_desc));
    WRITE_BLOCK(2, buf);

    /* Write Block 3 (Block Bitmap) */
    memset(buf, 0, block_size);
    /* Mark block 0 up to overhead_blocks - 1 as allocated (1) */
    for (uint32_t b = 0; b < overhead_blocks; b++) {
        buf[b / 8] |= (1U << (b % 8));
    }
    WRITE_BLOCK(b_bitmap_blk, buf);

    /* Write Block 4 (Inode Bitmap) */
    memset(buf, 0, block_size);
    /* Inodes 1..11 are marked allocated (bits 0..10) */
    for (uint32_t i = 0; i < 11; i++) {
        buf[i / 8] |= (1U << (i % 8));
    }
    WRITE_BLOCK(i_bitmap_blk, buf);

    /* Write Inode Table blocks */
    memset(buf, 0, block_size);
    for (uint32_t b = 0; b < itable_blocks; b++) {
        WRITE_BLOCK(itable_start_blk + b, buf);
    }

    /* Populate Inode 2 (Root Directory) & Inode 8 (Journal) */
    uint32_t ino_tbl_first_blk = itable_start_blk;
    memset(buf, 0, block_size);

    /* Inode 2: offset = (2 - 1) * inode_size = 128 */
    struct ext4_inode *root_in = (struct ext4_inode *)(buf + 128);
    root_in->i_mode = EXT4_S_IFDIR | 0755;
    root_in->i_links_count = 2;
    root_in->i_size_lo = block_size;
    root_in->i_blocks_lo = 2; /* 2 sectors = 1024 bytes */
    root_in->i_flags = EXT4_EXTENTS_FL;

    struct ext4_extent_header *reh = (struct ext4_extent_header *)root_in->i_block;
    reh->eh_magic = EXT4_EXT_MAGIC;
    reh->eh_entries = 1;
    reh->eh_max = 4;
    reh->eh_depth = 0;

    struct ext4_extent *rext = (struct ext4_extent *)(reh + 1);
    rext->ee_block = 0;
    rext->ee_len = 1;
    rext->ee_start_hi = 0;
    rext->ee_start_lo = root_dir_blk;

    /* Inode 8: offset = (8 - 1) * inode_size = 7 * 128 = 896 */
    struct ext4_inode *j_in = (struct ext4_inode *)(buf + 896);
    j_in->i_mode = EXT4_S_IFREG | 0600;
    j_in->i_links_count = 1;
    j_in->i_size_lo = journal_blocks * block_size;
    j_in->i_blocks_lo = journal_blocks * 2;
    j_in->i_flags = EXT4_EXTENTS_FL;

    struct ext4_extent_header *jeh = (struct ext4_extent_header *)j_in->i_block;
    jeh->eh_magic = EXT4_EXT_MAGIC;
    jeh->eh_entries = 1;
    jeh->eh_max = 4;
    jeh->eh_depth = 0;

    struct ext4_extent *jext = (struct ext4_extent *)(jeh + 1);
    jext->ee_block = 0;
    jext->ee_len = (uint16_t)journal_blocks;
    jext->ee_start_hi = 0;
    jext->ee_start_lo = journal_start_blk;

    WRITE_BLOCK(ino_tbl_first_blk, buf);

    /* Write Root Directory Data Block (containing "." and "..") */
    memset(buf, 0, block_size);
    struct ext4_dir_entry_2 *dot = (struct ext4_dir_entry_2 *)buf;
    dot->inode = EXT4_ROOT_INO;
    dot->rec_len = 12;
    dot->name_len = 1;
    dot->file_type = EXT4_FT_DIR;
    dot->name[0] = '.';

    struct ext4_dir_entry_2 *dotdot = (struct ext4_dir_entry_2 *)(buf + 12);
    dotdot->inode = EXT4_ROOT_INO;
    dotdot->rec_len = (uint16_t)(block_size - 12);
    dotdot->name_len = 2;
    dotdot->file_type = EXT4_FT_DIR;
    dotdot->name[0] = '.';
    dotdot->name[1] = '.';

    WRITE_BLOCK(root_dir_blk, buf);

    /* Write Initial Clean JBD2 Journal Superblock */
    memset(buf, 0, block_size);
    journal_superblock_t *jsb = (journal_superblock_t *)buf;
    jsb->s_header.h_magic = JBD2_MAGIC_NUMBER;
    jsb->s_header.h_blocktype = JBD2_SUPERBLOCK_V2;
    jsb->s_header.h_sequence = 1;
    jsb->s_blocksize = block_size;
    jsb->s_maxlen = journal_blocks;
    jsb->s_first = 1;
    jsb->s_sequence = 1;
    jsb->s_start = 0; /* clean */

    WRITE_BLOCK(journal_start_blk, buf);

    if (bdev->ops->sync) {
        bdev->ops->sync(bdev);
    }

    kal_free(buf);
    kal_free(gdt);

    pr_info("cherry_ext4: mkfs.ext4 completed successfully on '%s' (vol='%s')\n",
            dev_name, sb.s_volume_name);
    return 0;
}
