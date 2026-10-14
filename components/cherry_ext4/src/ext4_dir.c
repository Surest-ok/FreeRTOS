/*
 * CherryExt4 - Directory Engine (Lookup, Add, Remove, Traversal)
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

#define EXT4_DIR_REC_LEN(len)   ((((len) + 8 + 3) / 4) * 4)

int ext4_dir_lookup(struct ext4_fs *fs, const struct ext4_inode *dir_inode, const char *name, uint32_t *out_ino, uint8_t *out_type)
{
    if (!fs || !dir_inode || !name || !out_ino) return -1;
    size_t name_len = strlen(name);
    if (name_len == 0 || name_len > 255) return -2;

    uint32_t num_blocks = (dir_inode->i_size_lo + fs->block_size - 1) / fs->block_size;
    uint8_t *block_buf = (uint8_t *)kal_malloc(fs->block_size);
    if (!block_buf) return -12;

    for (uint32_t b = 0; b < num_blocks; b++) {
        uint32_t pblock = 0;
        ext4_bmap(fs, (struct ext4_inode *)dir_inode, b, &pblock, false);
        if (pblock == 0) continue;

        if (ext4_read_block(fs, pblock, block_buf) != 0) {
            continue;
        }

        uint32_t offset = 0;
        while (offset < fs->block_size) {
            struct ext4_dir_entry_2 *de = (struct ext4_dir_entry_2 *)(block_buf + offset);
            if (de->rec_len < 8 || offset + de->rec_len > fs->block_size) {
                break;
            }

            if (de->inode != 0 && de->name_len == name_len) {
                if (memcmp(de->name, name, name_len) == 0) {
                    *out_ino = de->inode;
                    if (out_type) *out_type = de->file_type;
                    kal_free(block_buf);
                    return 0;
                }
            }

            offset += de->rec_len;
        }
    }

    kal_free(block_buf);
    return -2; /* ENOENT */
}

int ext4_dir_add_entry(struct ext4_fs *fs, uint32_t dir_ino, struct ext4_inode *dir_inode,
                       const char *name, uint32_t child_ino, uint8_t file_type)
{
    if (!fs || !dir_inode || !name || child_ino < 1) return -1;
    size_t name_len = strlen(name);
    if (name_len == 0 || name_len > 255) return -2;

    uint16_t needed_len = (uint16_t)EXT4_DIR_REC_LEN(name_len);
    uint32_t num_blocks = (dir_inode->i_size_lo + fs->block_size - 1) / fs->block_size;

    uint8_t *block_buf = (uint8_t *)kal_malloc(fs->block_size);
    if (!block_buf) return -12;

    for (uint32_t b = 0; b < num_blocks; b++) {
        uint32_t pblock = 0;
        ext4_bmap(fs, dir_inode, b, &pblock, false);
        if (pblock == 0) continue;

        if (ext4_read_block(fs, pblock, block_buf) != 0) {
            continue;
        }

        uint32_t offset = 0;
        while (offset < fs->block_size) {
            struct ext4_dir_entry_2 *de = (struct ext4_dir_entry_2 *)(block_buf + offset);
            if (de->rec_len < 8 || offset + de->rec_len > fs->block_size) {
                break;
            }

            if (de->inode == 0 && de->rec_len >= needed_len) {
                /* Reuse inactive entry */
                de->inode = child_ino;
                de->name_len = (uint8_t)name_len;
                de->file_type = file_type;
                memcpy(de->name, name, name_len);

                ext4_write_block(fs, pblock, block_buf);
                kal_free(block_buf);
                return 0;
            }

            uint16_t actual_len = (uint16_t)EXT4_DIR_REC_LEN(de->name_len);
            if (de->rec_len >= actual_len + needed_len) {
                /* Split existing entry */
                uint16_t old_rec = de->rec_len;
                de->rec_len = actual_len;

                struct ext4_dir_entry_2 *new_de = (struct ext4_dir_entry_2 *)(block_buf + offset + actual_len);
                new_de->inode = child_ino;
                new_de->rec_len = old_rec - actual_len;
                new_de->name_len = (uint8_t)name_len;
                new_de->file_type = file_type;
                memcpy(new_de->name, name, name_len);

                ext4_write_block(fs, pblock, block_buf);
                kal_free(block_buf);
                return 0;
            }

            offset += de->rec_len;
        }
    }

    /* All existing blocks full, allocate a new block for the directory */
    uint32_t new_pblock = 0;
    int ret = ext4_bmap(fs, dir_inode, num_blocks, &new_pblock, true);
    if (ret != 0 || new_pblock == 0) {
        kal_free(block_buf);
        return -28; /* ENOSPC */
    }

    memset(block_buf, 0, fs->block_size);
    struct ext4_dir_entry_2 *first_de = (struct ext4_dir_entry_2 *)block_buf;
    first_de->inode = child_ino;
    first_de->rec_len = (uint16_t)fs->block_size;
    first_de->name_len = (uint8_t)name_len;
    first_de->file_type = file_type;
    memcpy(first_de->name, name, name_len);

    ext4_write_block(fs, new_pblock, block_buf);
    dir_inode->i_size_lo += fs->block_size;
    ext4_write_inode(fs, dir_ino, dir_inode);

    kal_free(block_buf);
    return 0;
}

int ext4_dir_remove_entry(struct ext4_fs *fs, uint32_t dir_ino, struct ext4_inode *dir_inode, const char *name)
{
    if (!fs || !dir_inode || !name) return -1;
    size_t name_len = strlen(name);
    uint32_t num_blocks = (dir_inode->i_size_lo + fs->block_size - 1) / fs->block_size;

    uint8_t *block_buf = (uint8_t *)kal_malloc(fs->block_size);
    if (!block_buf) return -12;

    for (uint32_t b = 0; b < num_blocks; b++) {
        uint32_t pblock = 0;
        ext4_bmap(fs, dir_inode, b, &pblock, false);
        if (pblock == 0) continue;

        if (ext4_read_block(fs, pblock, block_buf) != 0) {
            continue;
        }

        uint32_t offset = 0;
        struct ext4_dir_entry_2 *prev_de = NULL;

        while (offset < fs->block_size) {
            struct ext4_dir_entry_2 *de = (struct ext4_dir_entry_2 *)(block_buf + offset);
            if (de->rec_len < 8 || offset + de->rec_len > fs->block_size) {
                break;
            }

            if (de->inode != 0 && de->name_len == name_len && memcmp(de->name, name, name_len) == 0) {
                /* Found entry to remove */
                if (prev_de != NULL) {
                    prev_de->rec_len += de->rec_len;
                } else {
                    de->inode = 0;
                }

                ext4_write_block(fs, pblock, block_buf);
                (void)dir_ino;
                kal_free(block_buf);
                return 0;
            }

            prev_de = de;
            offset += de->rec_len;
        }
    }

    kal_free(block_buf);
    return -2; /* ENOENT */
}

bool ext4_dir_is_empty(struct ext4_fs *fs, const struct ext4_inode *dir_inode)
{
    if (!fs || !dir_inode) return false;
    uint32_t num_blocks = (dir_inode->i_size_lo + fs->block_size - 1) / fs->block_size;

    uint8_t *block_buf = (uint8_t *)kal_malloc(fs->block_size);
    if (!block_buf) return false;

    for (uint32_t b = 0; b < num_blocks; b++) {
        uint32_t pblock = 0;
        ext4_bmap(fs, (struct ext4_inode *)dir_inode, b, &pblock, false);
        if (pblock == 0) continue;

        if (ext4_read_block(fs, pblock, block_buf) != 0) continue;

        uint32_t offset = 0;
        while (offset < fs->block_size) {
            struct ext4_dir_entry_2 *de = (struct ext4_dir_entry_2 *)(block_buf + offset);
            if (de->rec_len < 8 || offset + de->rec_len > fs->block_size) break;

            if (de->inode != 0) {
                if (de->name_len == 1 && de->name[0] == '.') {
                    /* skip . */
                } else if (de->name_len == 2 && de->name[0] == '.' && de->name[1] == '.') {
                    /* skip .. */
                } else {
                    kal_free(block_buf);
                    return false;
                }
            }
            offset += de->rec_len;
        }
    }

    kal_free(block_buf);
    return true;
}
