/*
 * CherryExt4 - Inode Operations, Extents & Block Mapping
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

int ext4_read_inode(struct ext4_fs *fs, uint32_t ino, struct ext4_inode *out_inode)
{
    if (!fs || ino < 1 || !out_inode) return -1;

    uint32_t group = (ino - 1) / fs->sb.s_inodes_per_group;
    uint32_t index = (ino - 1) % fs->sb.s_inodes_per_group;

    if (group >= fs->groups_count) return -2;

    struct ext4_group_desc *desc = &fs->group_descs[group];
    uint32_t inode_size = fs->sb.s_inode_size ? fs->sb.s_inode_size : 128;
    uint32_t offset_in_table = index * inode_size;

    uint32_t block_nr = desc->bg_inode_table_lo + (offset_in_table / fs->block_size);
    uint32_t block_offset = offset_in_table % fs->block_size;

    uint8_t *buf = (uint8_t *)kal_malloc(fs->block_size);
    if (!buf) return -12;

    int ret = ext4_read_block(fs, block_nr, buf);
    if (ret == 0) {
        memcpy(out_inode, buf + block_offset, sizeof(struct ext4_inode));
    }
    kal_free(buf);
    return ret;
}

int ext4_write_inode(struct ext4_fs *fs, uint32_t ino, const struct ext4_inode *in_inode)
{
    if (!fs || ino < 1 || !in_inode) return -1;
    if (fs->read_only) return -2;

    uint32_t group = (ino - 1) / fs->sb.s_inodes_per_group;
    uint32_t index = (ino - 1) % fs->sb.s_inodes_per_group;

    if (group >= fs->groups_count) return -2;

    struct ext4_group_desc *desc = &fs->group_descs[group];
    uint32_t inode_size = fs->sb.s_inode_size ? fs->sb.s_inode_size : 128;
    uint32_t offset_in_table = index * inode_size;

    uint32_t block_nr = desc->bg_inode_table_lo + (offset_in_table / fs->block_size);
    uint32_t block_offset = offset_in_table % fs->block_size;

    uint8_t *buf = (uint8_t *)kal_malloc(fs->block_size);
    if (!buf) return -12;

    int ret = ext4_read_block(fs, block_nr, buf);
    if (ret == 0) {
        memcpy(buf + block_offset, in_inode, sizeof(struct ext4_inode));
        ret = ext4_write_block(fs, block_nr, buf);
    }
    kal_free(buf);
    return ret;
}

int ext4_bmap(struct ext4_fs *fs, struct ext4_inode *inode, uint32_t lblock, uint32_t *pblock, bool create)
{
    if (!fs || !inode || !pblock) return -1;

    /* 1. Extents Tree mapping */
    if (inode->i_flags & EXT4_EXTENTS_FL) {
        struct ext4_extent_header *eh = (struct ext4_extent_header *)inode->i_block;
        if (eh->eh_magic == EXT4_EXT_MAGIC && eh->eh_depth == 0) {
            struct ext4_extent *exts = (struct ext4_extent *)(eh + 1);

            /* Search existing extents */
            for (uint16_t i = 0; i < eh->eh_entries; i++) {
                if (lblock >= exts[i].ee_block && lblock < exts[i].ee_block + exts[i].ee_len) {
                    *pblock = exts[i].ee_start_lo + (lblock - exts[i].ee_block);
                    return 0;
                }
            }

            if (!create) {
                *pblock = 0;
                return 0;
            }

            /* Allocate a new physical block */
            uint32_t new_blk = 0;
            int ret = ext4_alloc_block(fs, 0, &new_blk);
            if (ret != 0) return ret;

            /* Try to extend previous extent if contiguous */
            if (eh->eh_entries > 0) {
                struct ext4_extent *last = &exts[eh->eh_entries - 1];
                if (last->ee_block + last->ee_len == lblock &&
                    last->ee_start_lo + last->ee_len == new_blk &&
                    last->ee_len < 32768) {
                    last->ee_len++;
                    inode->i_blocks_lo += fs->sectors_per_block;
                    *pblock = new_blk;
                    return 0;
                }
            }

            /* Append new extent if there is room in header (max 4 entries in 60 bytes) */
            if (eh->eh_entries < eh->eh_max) {
                struct ext4_extent *cur = &exts[eh->eh_entries];
                cur->ee_block = lblock;
                cur->ee_len = 1;
                cur->ee_start_hi = 0;
                cur->ee_start_lo = new_blk;
                eh->eh_entries++;
                inode->i_blocks_lo += fs->sectors_per_block;
                *pblock = new_blk;
                return 0;
            }

            /* For extent overflow, we fall through or could build tree. Max 4 extents in inode */
            pr_warn("cherry_ext4: inode extent leaf full (max=%d)\n", (int)eh->eh_max);
            return -27; /* EFBIG */
        }
    }

    /* 2. Direct block pointers (legacy / simple fallback) */
    if (lblock < 12) {
        if (inode->i_block[lblock] != 0) {
            *pblock = inode->i_block[lblock];
            return 0;
        }

        if (!create) {
            *pblock = 0;
            return 0;
        }

        uint32_t new_blk = 0;
        int ret = ext4_alloc_block(fs, 0, &new_blk);
        if (ret != 0) return ret;

        inode->i_block[lblock] = new_blk;
        inode->i_blocks_lo += fs->sectors_per_block;
        *pblock = new_blk;
        return 0;
    }

    /* Single indirect block (inode->i_block[12]) */
    uint32_t ptrs_per_blk = fs->block_size / 4;
    uint32_t ind_lblock = lblock - 12;
    if (ind_lblock < ptrs_per_blk) {
        uint32_t *ind_table = (uint32_t *)kal_malloc(fs->block_size);
        if (!ind_table) return -12;

        if (inode->i_block[12] == 0) {
            if (!create) {
                kal_free(ind_table);
                *pblock = 0;
                return 0;
            }
            uint32_t new_ind_blk = 0;
            int ret = ext4_alloc_block(fs, 0, &new_ind_blk);
            if (ret != 0) {
                kal_free(ind_table);
                return ret;
            }
            inode->i_block[12] = new_ind_blk;
            inode->i_blocks_lo += fs->sectors_per_block;
        }

        ext4_read_block(fs, inode->i_block[12], ind_table);
        if (ind_table[ind_lblock] != 0) {
            *pblock = ind_table[ind_lblock];
            kal_free(ind_table);
            return 0;
        }

        if (!create) {
            kal_free(ind_table);
            *pblock = 0;
            return 0;
        }

        uint32_t new_data_blk = 0;
        int ret = ext4_alloc_block(fs, 0, &new_data_blk);
        if (ret != 0) {
            kal_free(ind_table);
            return ret;
        }

        ind_table[ind_lblock] = new_data_blk;
        ext4_write_block(fs, inode->i_block[12], ind_table);
        inode->i_blocks_lo += fs->sectors_per_block;
        *pblock = new_data_blk;
        kal_free(ind_table);
        return 0;
    }

    return -27; /* EFBIG */
}

ssize_t ext4_read_file(struct ext4_fs *fs, struct ext4_inode *inode, off_t offset, void *buf, size_t count)
{
    if (!fs || !inode || !buf) return -1;
    if (offset >= (off_t)inode->i_size_lo) return 0;

    size_t to_read = count;
    if (offset + to_read > inode->i_size_lo) {
        to_read = inode->i_size_lo - offset;
    }

    uint8_t *block_buf = (uint8_t *)kal_malloc(fs->block_size);
    if (!block_buf) return -12;

    size_t bytes_read = 0;
    uint8_t *dst = (uint8_t *)buf;

    while (bytes_read < to_read) {
        off_t cur_pos = offset + bytes_read;
        uint32_t lblock = cur_pos / fs->block_size;
        uint32_t blk_off = cur_pos % fs->block_size;
        size_t chunk = fs->block_size - blk_off;
        if (chunk > to_read - bytes_read) {
            chunk = to_read - bytes_read;
        }

        uint32_t pblock = 0;
        ext4_bmap(fs, inode, lblock, &pblock, false);

        if (pblock == 0) {
            /* Sparse hole */
            memset(dst + bytes_read, 0, chunk);
        } else {
            ext4_read_block(fs, pblock, block_buf);
            memcpy(dst + bytes_read, block_buf + blk_off, chunk);
        }

        bytes_read += chunk;
    }

    kal_free(block_buf);
    return (ssize_t)bytes_read;
}

ssize_t ext4_write_file(struct ext4_fs *fs, uint32_t ino, struct ext4_inode *inode, off_t offset, const void *buf, size_t count)
{
    if (!fs || !inode || !buf || ino < 1) return -1;
    if (fs->read_only) return -2;

    uint8_t *block_buf = (uint8_t *)kal_malloc(fs->block_size);
    if (!block_buf) return -12;

    size_t bytes_written = 0;
    const uint8_t *src = (const uint8_t *)buf;

    while (bytes_written < count) {
        off_t cur_pos = offset + bytes_written;
        uint32_t lblock = cur_pos / fs->block_size;
        uint32_t blk_off = cur_pos % fs->block_size;
        size_t chunk = fs->block_size - blk_off;
        if (chunk > count - bytes_written) {
            chunk = count - bytes_written;
        }

        uint32_t pblock = 0;
        int ret = ext4_bmap(fs, inode, lblock, &pblock, true);
        if (ret != 0 || pblock == 0) {
            break;
        }

        /* Read existing block if partial write */
        if (blk_off > 0 || chunk < fs->block_size) {
            ext4_read_block(fs, pblock, block_buf);
        }

        memcpy(block_buf + blk_off, src + bytes_written, chunk);
        ext4_write_block(fs, pblock, block_buf);

        bytes_written += chunk;
    }

    kal_free(block_buf);

    if (offset + bytes_written > inode->i_size_lo) {
        inode->i_size_lo = (uint32_t)(offset + bytes_written);
    }
    ext4_write_inode(fs, ino, inode);

    return (ssize_t)bytes_written;
}

int ext4_truncate_file(struct ext4_fs *fs, uint32_t ino, struct ext4_inode *inode, off_t new_size)
{
    if (!fs || !inode || ino < 1) return -1;
    if (new_size < 0) return -22;

    inode->i_size_lo = (uint32_t)new_size;
    ext4_write_inode(fs, ino, inode);
    return 0;
}
