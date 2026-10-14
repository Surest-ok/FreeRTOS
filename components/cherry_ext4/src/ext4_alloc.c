/*
 * CherryExt4 - Block and Inode Allocator (Bitmap Management)
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

int ext4_alloc_block(struct ext4_fs *fs, uint32_t goal, uint32_t *new_block)
{
    if (!fs || !new_block) return -1;
    if (fs->sb.s_free_blocks_count_lo == 0) {
        return -28; /* ENOSPC */
    }

    uint8_t *bitmap = (uint8_t *)kal_malloc(fs->block_size);
    if (!bitmap) return -12;

    uint32_t start_group = 0;
    if (goal > fs->first_data_block) {
        start_group = (goal - fs->first_data_block) / fs->sb.s_blocks_per_group;
        if (start_group >= fs->groups_count) start_group = 0;
    }

    for (uint32_t g_idx = 0; g_idx < fs->groups_count; g_idx++) {
        uint32_t group = (start_group + g_idx) % fs->groups_count;
        struct ext4_group_desc *desc = &fs->group_descs[group];
        if (desc->bg_free_blocks_count_lo == 0) {
            continue;
        }

        if (ext4_read_block(fs, desc->bg_block_bitmap_lo, bitmap) != 0) {
            continue;
        }

        uint32_t max_blocks_in_grp = fs->sb.s_blocks_per_group;
        for (uint32_t i = 0; i < max_blocks_in_grp; i++) {
            uint32_t byte_idx = i / 8;
            uint32_t bit_idx  = i % 8;
            if ((bitmap[byte_idx] & (1U << bit_idx)) == 0) {
                /* Found free block! Allocate it */
                bitmap[byte_idx] |= (1U << bit_idx);
                desc->bg_free_blocks_count_lo--;
                fs->sb.s_free_blocks_count_lo--;

                ext4_write_block(fs, desc->bg_block_bitmap_lo, bitmap);

                uint32_t blk = group * fs->sb.s_blocks_per_group + i + fs->first_data_block;
                *new_block = blk;

                /* Zero out the newly allocated block */
                memset(bitmap, 0, fs->block_size);
                ext4_write_block(fs, blk, bitmap);

                ext4_sync_sb_and_gdt(fs);
                kal_free(bitmap);
                return 0;
            }
        }
    }

    kal_free(bitmap);
    return -28; /* ENOSPC */
}

int ext4_free_block(struct ext4_fs *fs, uint32_t block_nr)
{
    if (!fs || block_nr < fs->first_data_block) return -1;

    uint32_t rel = block_nr - fs->first_data_block;
    uint32_t group = rel / fs->sb.s_blocks_per_group;
    uint32_t index = rel % fs->sb.s_blocks_per_group;

    if (group >= fs->groups_count) return -2;

    uint8_t *bitmap = (uint8_t *)kal_malloc(fs->block_size);
    if (!bitmap) return -12;

    struct ext4_group_desc *desc = &fs->group_descs[group];
    if (ext4_read_block(fs, desc->bg_block_bitmap_lo, bitmap) != 0) {
        kal_free(bitmap);
        return -5;
    }

    uint32_t byte_idx = index / 8;
    uint32_t bit_idx  = index % 8;

    if (bitmap[byte_idx] & (1U << bit_idx)) {
        bitmap[byte_idx] &= ~(1U << bit_idx);
        desc->bg_free_blocks_count_lo++;
        fs->sb.s_free_blocks_count_lo++;
        ext4_write_block(fs, desc->bg_block_bitmap_lo, bitmap);
        ext4_sync_sb_and_gdt(fs);
    }

    kal_free(bitmap);
    return 0;
}

int ext4_alloc_inode(struct ext4_fs *fs, mode_t mode, uint32_t *new_ino)
{
    if (!fs || !new_ino) return -1;
    if (fs->sb.s_free_inodes_count == 0) {
        return -28; /* ENOSPC */
    }

    uint8_t *bitmap = (uint8_t *)kal_malloc(fs->block_size);
    if (!bitmap) return -12;

    for (uint32_t group = 0; group < fs->groups_count; group++) {
        struct ext4_group_desc *desc = &fs->group_descs[group];
        if (desc->bg_free_inodes_count_lo == 0) {
            continue;
        }

        if (ext4_read_block(fs, desc->bg_inode_bitmap_lo, bitmap) != 0) {
            continue;
        }

        uint32_t start_i = (group == 0) ? (fs->sb.s_first_ino - 1) : 0;
        uint32_t max_i = fs->sb.s_inodes_per_group;

        for (uint32_t i = start_i; i < max_i; i++) {
            uint32_t byte_idx = i / 8;
            uint32_t bit_idx  = i % 8;
            if ((bitmap[byte_idx] & (1U << bit_idx)) == 0) {
                /* Allocate inode */
                bitmap[byte_idx] |= (1U << bit_idx);
                desc->bg_free_inodes_count_lo--;
                fs->sb.s_free_inodes_count--;
                if ((mode & EXT4_S_IFMT) == EXT4_S_IFDIR) {
                    desc->bg_used_dirs_count_lo++;
                }

                ext4_write_block(fs, desc->bg_inode_bitmap_lo, bitmap);

                uint32_t ino = group * fs->sb.s_inodes_per_group + i + 1;
                *new_ino = ino;

                /* Initialize inode structure on disk */
                struct ext4_inode new_in;
                memset(&new_in, 0, sizeof(new_in));
                new_in.i_mode = (uint16_t)mode;
                new_in.i_links_count = (mode & EXT4_S_IFMT) == EXT4_S_IFDIR ? 2 : 1;

                /* Initialize Extents Header if extents feature is on */
                if (fs->sb.s_feature_incompat & EXT4_FEATURE_INCOMPAT_EXTENTS) {
                    new_in.i_flags |= EXT4_EXTENTS_FL;
                    struct ext4_extent_header *eh = (struct ext4_extent_header *)new_in.i_block;
                    eh->eh_magic = EXT4_EXT_MAGIC;
                    eh->eh_entries = 0;
                    eh->eh_max = 4; /* Fits 4 extents in 60 bytes (12 bytes header + 4 * 12) */
                    eh->eh_depth = 0;
                }

                ext4_write_inode(fs, ino, &new_in);
                ext4_sync_sb_and_gdt(fs);

                kal_free(bitmap);
                return 0;
            }
        }
    }

    kal_free(bitmap);
    return -28;
}

int ext4_free_inode(struct ext4_fs *fs, uint32_t ino)
{
    if (!fs || ino < 1) return -1;

    uint32_t group = (ino - 1) / fs->sb.s_inodes_per_group;
    uint32_t index = (ino - 1) % fs->sb.s_inodes_per_group;

    if (group >= fs->groups_count) return -2;

    uint8_t *bitmap = (uint8_t *)kal_malloc(fs->block_size);
    if (!bitmap) return -12;

    struct ext4_group_desc *desc = &fs->group_descs[group];
    if (ext4_read_block(fs, desc->bg_inode_bitmap_lo, bitmap) != 0) {
        kal_free(bitmap);
        return -5;
    }

    uint32_t byte_idx = index / 8;
    uint32_t bit_idx  = index % 8;

    if (bitmap[byte_idx] & (1U << bit_idx)) {
        bitmap[byte_idx] &= ~(1U << bit_idx);
        desc->bg_free_inodes_count_lo++;
        fs->sb.s_free_inodes_count++;

        /* Check if it was directory */
        struct ext4_inode in;
        if (ext4_read_inode(fs, ino, &in) == 0) {
            if ((in.i_mode & EXT4_S_IFMT) == EXT4_S_IFDIR && desc->bg_used_dirs_count_lo > 0) {
                desc->bg_used_dirs_count_lo--;
            }
        }

        ext4_write_block(fs, desc->bg_inode_bitmap_lo, bitmap);
        ext4_sync_sb_and_gdt(fs);
    }

    kal_free(bitmap);
    return 0;
}
