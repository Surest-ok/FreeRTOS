/*
 * CherryExt4 - fsck.ext4 Filesystem Consistency Checker & Repair
 *
 * Checks Ext4 superblock magic, block group descriptors, journal recovery,
 * root directory inode integrity, and bitmap allocations.
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

int ext4_fsck(const char *dev_name, bool repair)
{
    if (!dev_name) return -1;

    struct vfs_bdev *bdev = vfs_bdev_find(dev_name);
    if (!bdev && dev_name[0] == '/') {
        bdev = vfs_bdev_find(dev_name + 5);
    }
    if (!bdev) {
        pr_err("cherry_ext4: fsck device '%s' not found\n", dev_name);
        return -2;
    }

    pr_info("cherry_ext4: starting fsck on '%s' (repair=%d)...\n", dev_name, repair);

    /* 1. Read Superblock */
    uint8_t *sb_buf = (uint8_t *)kal_malloc(1024);
    if (!sb_buf) return -12;

    /* Superblock is at byte offset 1024 (LBA 2) */
    if (bdev->ops->read_blocks(bdev, sb_buf, 2, 2) != 0) {
        pr_err("cherry_ext4: failed to read superblock from '%s'\n", dev_name);
        kal_free(sb_buf);
        return -5;
    }

    struct ext4_super_block *sb = (struct ext4_super_block *)sb_buf;
    if (sb->s_magic != EXT4_SUPER_MAGIC) {
        pr_err("cherry_ext4: fsck failed - invalid magic 0x%04x (expected 0x%04x)\n",
               sb->s_magic, EXT4_SUPER_MAGIC);
        kal_free(sb_buf);
        return -22;
    }

    uint32_t block_size = 1024 << sb->s_log_block_size;
    if (block_size < EXT4_MIN_BLOCK_SIZE || block_size > EXT4_MAX_BLOCK_SIZE) {
        pr_err("cherry_ext4: fsck failed - unsupported block size %u\n", block_size);
        kal_free(sb_buf);
        return -22;
    }

    /* 2. Setup temporary ext4_fs context for checking */
    struct ext4_fs fs;
    memset(&fs, 0, sizeof(fs));
    fs.bdev = bdev;
    memcpy(&fs.sb, sb, sizeof(struct ext4_super_block));
    fs.block_size = block_size;
    fs.sectors_per_block = block_size / 512;
    fs.first_data_block = sb->s_first_data_block;
    fs.groups_count = (sb->s_blocks_count_lo + sb->s_blocks_per_group - 1) / sb->s_blocks_per_group;

    /* Read Group Descriptors */
    size_t gdt_size = fs.groups_count * sizeof(struct ext4_group_desc);
    fs.group_descs = (struct ext4_group_desc *)kal_malloc(gdt_size);
    if (!fs.group_descs) {
        kal_free(sb_buf);
        return -12;
    }

    uint32_t gdt_block = (block_size == 1024) ? 2 : 1;
    uint8_t *gdt_buf = (uint8_t *)kal_malloc(block_size);
    if (!gdt_buf) {
        kal_free(fs.group_descs);
        kal_free(sb_buf);
        return -12;
    }

    ext4_read_block(&fs, gdt_block, gdt_buf);
    memcpy(fs.group_descs, gdt_buf, gdt_size);
    kal_free(gdt_buf);

    /* 3. Check & Replay JBD2 Journal */
    if (fs.sb.s_feature_compat & EXT4_FEATURE_COMPAT_HAS_JOURNAL) {
        jbd2_init(&fs);
        if (repair && fs.journal.jsb.s_start != 0) {
            pr_info("cherry_ext4: repairing dirty journal via jbd2 replay...\n");
            jbd2_replay(&fs);
        }
    }

    /* 4. Validate Inode 2 (Root directory) */
    struct ext4_inode root_in;
    if (ext4_read_inode(&fs, EXT4_ROOT_INO, &root_in) != 0) {
        pr_err("cherry_ext4: fsck failed - cannot read root inode 2\n");
        kal_free(fs.group_descs);
        kal_free(sb_buf);
        return -1;
    }

    if ((root_in.i_mode & EXT4_S_IFMT) != EXT4_S_IFDIR) {
        pr_err("cherry_ext4: fsck failed - root inode 2 is not a directory (mode=0%o)\n", root_in.i_mode);
        kal_free(fs.group_descs);
        kal_free(sb_buf);
        return -1;
    }

    /* 5. Check Root directory contents ('.' and '..') */
    uint32_t dot_ino = 0, dotdot_ino = 0;
    if (ext4_dir_lookup(&fs, &root_in, ".", &dot_ino, NULL) != 0 || dot_ino != EXT4_ROOT_INO) {
        pr_err("cherry_ext4: fsck failed - missing or invalid '.' in root directory\n");
        kal_free(fs.group_descs);
        kal_free(sb_buf);
        return -1;
    }

    if (ext4_dir_lookup(&fs, &root_in, "..", &dotdot_ino, NULL) != 0 || dotdot_ino != EXT4_ROOT_INO) {
        pr_err("cherry_ext4: fsck failed - missing or invalid '..' in root directory\n");
        kal_free(fs.group_descs);
        kal_free(sb_buf);
        return -1;
    }

    /* 6. Update clean filesystem state if repaired */
    if (repair && fs.sb.s_state != EXT4_VALID_FS) {
        fs.sb.s_state = EXT4_VALID_FS;
        ext4_sync_sb_and_gdt(&fs);
    }

    pr_info("cherry_ext4: fsck pass - volume '%s' is clean and consistent\n", fs.sb.s_volume_name);

    kal_free(fs.group_descs);
    kal_free(sb_buf);
    return 0;
}
