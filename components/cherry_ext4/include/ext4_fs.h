/*
 * CherryExt4 - Internal Filesystem Representation & Core Interfaces
 *
 * Copyright (c) 2026 ForgeStitch
 * SPDX-License-Identifier: MIT
 */

#ifndef CHERRY_EXT4_FS_H
#define CHERRY_EXT4_FS_H

#include <stdint.h>
#include <stdbool.h>
#include "cherry_kal.h"
#include "vfs_dev.h"
#include "vfs_types.h"
#include "ext4_types.h"
#include "ext4_jbd2.h"

/* In-Memory Ext4 Mount Instance */
struct ext4_fs {
    struct vfs_bdev            *bdev;               /* Underlying block device */
    struct ext4_super_block     sb;                 /* Superblock cache */
    struct ext4_group_desc     *group_descs;        /* Group descriptors array */
    uint32_t                    block_size;         /* e.g. 1024 or 4096 */
    uint32_t                    sectors_per_block;  /* block_size / 512 */
    uint32_t                    groups_count;       /* Total block groups */
    uint32_t                    first_data_block;   /* 1 for 1KB, 0 for >1KB */
    kal_mutex_t                 lock;               /* Metadata lock */
    struct jbd2_journal         journal;            /* Journal context */
    bool                        is_mounted;
    bool                        read_only;
};

/* Block I/O */
int ext4_read_block(struct ext4_fs *fs, uint32_t block_nr, void *buf);
int ext4_write_block(struct ext4_fs *fs, uint32_t block_nr, const void *buf);
int ext4_sync_sb_and_gdt(struct ext4_fs *fs);

/* Allocation Engine */
int ext4_alloc_block(struct ext4_fs *fs, uint32_t goal, uint32_t *new_block);
int ext4_free_block(struct ext4_fs *fs, uint32_t block_nr);
int ext4_alloc_inode(struct ext4_fs *fs, mode_t mode, uint32_t *new_ino);
int ext4_free_inode(struct ext4_fs *fs, uint32_t ino);

/* Inode Management */
int ext4_read_inode(struct ext4_fs *fs, uint32_t ino, struct ext4_inode *out_inode);
int ext4_write_inode(struct ext4_fs *fs, uint32_t ino, const struct ext4_inode *in_inode);
int ext4_bmap(struct ext4_fs *fs, struct ext4_inode *inode, uint32_t lblock, uint32_t *pblock, bool create);
ssize_t ext4_read_file(struct ext4_fs *fs, struct ext4_inode *inode, off_t offset, void *buf, size_t count);
ssize_t ext4_write_file(struct ext4_fs *fs, uint32_t ino, struct ext4_inode *inode, off_t offset, const void *buf, size_t count);
int ext4_truncate_file(struct ext4_fs *fs, uint32_t ino, struct ext4_inode *inode, off_t new_size);

/* Directory Engine */
int ext4_dir_lookup(struct ext4_fs *fs, const struct ext4_inode *dir_inode, const char *name, uint32_t *out_ino, uint8_t *out_type);
int ext4_dir_add_entry(struct ext4_fs *fs, uint32_t dir_ino, struct ext4_inode *dir_inode, const char *name, uint32_t child_ino, uint8_t file_type);
int ext4_dir_remove_entry(struct ext4_fs *fs, uint32_t dir_ino, struct ext4_inode *dir_inode, const char *name);
bool ext4_dir_is_empty(struct ext4_fs *fs, const struct ext4_inode *dir_inode);

#endif /* CHERRY_EXT4_FS_H */
