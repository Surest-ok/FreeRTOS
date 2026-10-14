/*
 * CherryExt4 - JBD2 Journaling Engine & Transaction Management
 *
 * Implements standard JBD2 journal creation, atomic transaction commits,
 * checkpointing, and journal recovery / replay for Ext4.
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
#include "ext4_jbd2.h"
#include "ext4_fs.h"

int jbd2_format(struct ext4_fs *fs, uint32_t start_block, uint32_t num_blocks)
{
    if (!fs || num_blocks < 8) return -1;

    uint8_t *block_buf = (uint8_t *)kal_malloc(fs->block_size);
    if (!block_buf) return -12;

    memset(block_buf, 0, fs->block_size);
    journal_superblock_t *jsb = (journal_superblock_t *)block_buf;

    jsb->s_header.h_magic = JBD2_MAGIC_NUMBER;
    jsb->s_header.h_blocktype = JBD2_SUPERBLOCK_V2;
    jsb->s_header.h_sequence = 1;

    jsb->s_blocksize = fs->block_size;
    jsb->s_maxlen = num_blocks;
    jsb->s_first = 1; /* Block 1 after journal superblock */
    jsb->s_sequence = 1;
    jsb->s_start = 0; /* 0 indicates journal is clean (no recovery needed) */
    jsb->s_feature_compat = 0;
    jsb->s_feature_incompat = 0;

    int ret = ext4_write_block(fs, start_block, block_buf);
    kal_free(block_buf);

    if (ret == 0) {
        fs->journal.fs = fs;
        fs->journal.journal_start_block = start_block;
        fs->journal.journal_total_blocks = num_blocks;
        fs->journal.cur_log_block = 1;
        fs->journal.is_active = true;
        memcpy(&fs->journal.jsb, jsb, sizeof(journal_superblock_t));
    }
    return ret;
}

int jbd2_init(struct ext4_fs *fs)
{
    if (!fs) return -1;
    kal_mutex_init(&fs->journal.lock);
    fs->journal.fs = fs;
    fs->journal.is_active = false;
    fs->journal.transactions_committed = 0;
    fs->journal.transactions_replayed = 0;

    if (!(fs->sb.s_feature_compat & EXT4_FEATURE_COMPAT_HAS_JOURNAL)) {
        return 0;
    }

    /* Locate journal inode or dedicated block */
    uint32_t j_ino = fs->sb.s_journal_inum ? fs->sb.s_journal_inum : EXT4_JOURNAL_INO;
    struct ext4_inode j_inode;
    if (ext4_read_inode(fs, j_ino, &j_inode) != 0) {
        pr_warn("cherry_ext4: failed to read journal inode %u\n", j_ino);
        return -1;
    }

    uint32_t j_start_block = 0;
    ext4_bmap(fs, &j_inode, 0, &j_start_block, false);
    if (j_start_block == 0) {
        pr_warn("cherry_ext4: journal inode has no allocated blocks\n");
        return -1;
    }

    uint8_t *block_buf = (uint8_t *)kal_malloc(fs->block_size);
    if (!block_buf) return -12;

    if (ext4_read_block(fs, j_start_block, block_buf) != 0) {
        kal_free(block_buf);
        return -5;
    }

    journal_superblock_t *jsb = (journal_superblock_t *)block_buf;
    if (jsb->s_header.h_magic != JBD2_MAGIC_NUMBER) {
        pr_warn("cherry_ext4: invalid journal superblock magic 0x%08x\n", jsb->s_header.h_magic);
        kal_free(block_buf);
        return -22;
    }

    fs->journal.journal_start_block = j_start_block;
    fs->journal.journal_total_blocks = jsb->s_maxlen;
    memcpy(&fs->journal.jsb, jsb, sizeof(journal_superblock_t));
    fs->journal.cur_log_block = (jsb->s_start > 0) ? jsb->s_start : jsb->s_first;
    fs->journal.is_active = true;

    pr_info("cherry_ext4: jbd2 journal active (start_block=%u, maxlen=%u, seq=%u, clean=%s)\n",
            j_start_block, jsb->s_maxlen, jsb->s_sequence,
            (jsb->s_start == 0) ? "yes" : "needs_replay");

    /* Check if journal needs recovery */
    if (jsb->s_start != 0) {
        pr_info("cherry_ext4: journal requires replay from block %u...\n", jsb->s_start);
        jbd2_replay(fs);
    }

    kal_free(block_buf);
    return 0;
}

struct jbd2_handle *jbd2_trans_start(struct ext4_fs *fs, int credits)
{
    if (!fs) return NULL;
    kal_mutex_lock(&fs->journal.lock);

    struct jbd2_handle *h = (struct jbd2_handle *)kal_malloc(sizeof(struct jbd2_handle));
    if (!h) {
        kal_mutex_unlock(&fs->journal.lock);
        return NULL;
    }

    memset(h, 0, sizeof(struct jbd2_handle));
    h->fs = fs;
    h->trans_id = fs->journal.jsb.s_sequence;
    (void)credits;

    return h;
}

int jbd2_trans_get_write_access(struct jbd2_handle *h, uint32_t fs_block, const void *data)
{
    if (!h || !h->fs || !data) return -1;
    if (h->num_bufs >= JBD2_MAX_TRANS_BLOCKS) {
        return -28; /* ENOSPC */
    }

    /* Check if already buffered */
    for (int i = 0; i < h->num_bufs; i++) {
        if (h->bufs[i].fs_block == fs_block) {
            memcpy(h->bufs[i].data, data, h->fs->block_size);
            h->bufs[i].is_dirty = true;
            return 0;
        }
    }

    void *buf_copy = kal_malloc(h->fs->block_size);
    if (!buf_copy) return -12;

    memcpy(buf_copy, data, h->fs->block_size);
    h->bufs[h->num_bufs].fs_block = fs_block;
    h->bufs[h->num_bufs].data = buf_copy;
    h->bufs[h->num_bufs].is_dirty = true;
    h->num_bufs++;

    return 0;
}

int jbd2_trans_commit(struct jbd2_handle *h)
{
    if (!h || !h->fs) return -1;
    struct ext4_fs *fs = h->fs;

    if (!fs->journal.is_active || h->num_bufs == 0) {
        /* Free allocated buffers */
        for (int i = 0; i < h->num_bufs; i++) {
            if (h->bufs[i].data) kal_free(h->bufs[i].data);
        }
        kal_free(h);
        kal_mutex_unlock(&fs->journal.lock);
        return 0;
    }

    uint8_t *desc_block = (uint8_t *)kal_malloc(fs->block_size);
    uint8_t *commit_block = (uint8_t *)kal_malloc(fs->block_size);
    if (!desc_block || !commit_block) {
        if (desc_block) kal_free(desc_block);
        if (commit_block) kal_free(commit_block);
        for (int i = 0; i < h->num_bufs; i++) if (h->bufs[i].data) kal_free(h->bufs[i].data);
        kal_free(h);
        kal_mutex_unlock(&fs->journal.lock);
        return -12;
    }

    memset(desc_block, 0, fs->block_size);
    memset(commit_block, 0, fs->block_size);

    /* 1. Prepare Descriptor Block */
    journal_header_t *dh = (journal_header_t *)desc_block;
    dh->h_magic = JBD2_MAGIC_NUMBER;
    dh->h_blocktype = JBD2_DESCRIPTOR_BLOCK;
    dh->h_sequence = h->trans_id;

    journal_block_tag_t *tags = (journal_block_tag_t *)(desc_block + sizeof(journal_header_t));
    for (int i = 0; i < h->num_bufs; i++) {
        tags[i].t_blocknr = h->bufs[i].fs_block;
        tags[i].t_flags = (i == h->num_bufs - 1) ? JBD2_FLAG_LAST_TAG : 0;
    }

    /* 2. Write Descriptor Block to Journal Ring */
    uint32_t j_cur = fs->journal.cur_log_block;
    uint32_t j_start = fs->journal.journal_start_block;
    uint32_t j_max = fs->journal.journal_total_blocks;

    ext4_write_block(fs, j_start + j_cur, desc_block);
    j_cur = (j_cur + 1) >= j_max ? fs->journal.jsb.s_first : (j_cur + 1);

    /* 3. Write Data Blocks to Journal Ring */
    for (int i = 0; i < h->num_bufs; i++) {
        ext4_write_block(fs, j_start + j_cur, h->bufs[i].data);
        j_cur = (j_cur + 1) >= j_max ? fs->journal.jsb.s_first : (j_cur + 1);
    }

    /* 4. Prepare & Write Commit Block */
    journal_commit_header_t *ch = (journal_commit_header_t *)commit_block;
    ch->h_header.h_magic = JBD2_MAGIC_NUMBER;
    ch->h_header.h_blocktype = JBD2_COMMIT_BLOCK;
    ch->h_header.h_sequence = h->trans_id;

    ext4_write_block(fs, j_start + j_cur, commit_block);
    j_cur = (j_cur + 1) >= j_max ? fs->journal.jsb.s_first : (j_cur + 1);

    /* Flush journal to disk before checkpointing */
    if (fs->bdev->ops->sync) fs->bdev->ops->sync(fs->bdev);

    /* 5. Checkpoint: Write modified metadata blocks to their on-disk filesystem blocks */
    for (int i = 0; i < h->num_bufs; i++) {
        ext4_write_block(fs, h->bufs[i].fs_block, h->bufs[i].data);
    }
    if (fs->bdev->ops->sync) fs->bdev->ops->sync(fs->bdev);

    /* 6. Update in-memory and on-disk journal superblock */
    fs->journal.cur_log_block = j_cur;
    fs->journal.jsb.s_sequence++;
    fs->journal.jsb.s_start = 0; /* filesystem checkpointed, state clean */
    fs->journal.transactions_committed++;

    memset(desc_block, 0, fs->block_size);
    memcpy(desc_block, &fs->journal.jsb, sizeof(journal_superblock_t));
    ext4_write_block(fs, j_start, desc_block);

    /* Cleanup */
    kal_free(desc_block);
    kal_free(commit_block);
    for (int i = 0; i < h->num_bufs; i++) {
        if (h->bufs[i].data) kal_free(h->bufs[i].data);
    }
    kal_free(h);

    kal_mutex_unlock(&fs->journal.lock);
    return 0;
}

int jbd2_replay(struct ext4_fs *fs)
{
    if (!fs || !fs->journal.is_active) return 0;

    uint32_t j_start = fs->journal.journal_start_block;
    uint32_t j_cur = fs->journal.jsb.s_start ? fs->journal.jsb.s_start : fs->journal.jsb.s_first;
    uint32_t j_max = fs->journal.journal_total_blocks;

    uint8_t *desc_block = (uint8_t *)kal_malloc(fs->block_size);
    uint8_t *data_block = (uint8_t *)kal_malloc(fs->block_size);
    uint8_t *commit_block = (uint8_t *)kal_malloc(fs->block_size);

    if (!desc_block || !data_block || !commit_block) {
        if (desc_block) kal_free(desc_block);
        if (data_block) kal_free(data_block);
        if (commit_block) kal_free(commit_block);
        return -12;
    }

    uint32_t replayed_count = 0;

    while (j_cur < j_max) {
        if (ext4_read_block(fs, j_start + j_cur, desc_block) != 0) break;

        journal_header_t *dh = (journal_header_t *)desc_block;
        if (dh->h_magic != JBD2_MAGIC_NUMBER || dh->h_blocktype != JBD2_DESCRIPTOR_BLOCK) {
            break;
        }

        uint32_t trans_seq = dh->h_sequence;
        journal_block_tag_t *tags = (journal_block_tag_t *)(desc_block + sizeof(journal_header_t));

        /* Count blocks in this transaction */
        int tag_count = 0;
        while (tag_count < 128) {
            if (tags[tag_count].t_flags & JBD2_FLAG_LAST_TAG) {
                tag_count++;
                break;
            }
            tag_count++;
        }

        /* Check if commit block follows payload blocks */
        uint32_t commit_pos = (j_cur + 1 + tag_count) % j_max;
        if (commit_pos == 0) commit_pos = fs->journal.jsb.s_first;

        if (ext4_read_block(fs, j_start + commit_pos, commit_block) != 0) break;
        journal_header_t *ch = (journal_header_t *)commit_block;

        if (ch->h_magic == JBD2_MAGIC_NUMBER && ch->h_blocktype == JBD2_COMMIT_BLOCK &&
            ch->h_sequence == trans_seq) {
            /* Valid transaction! Replay blocks */
            uint32_t blk_pos = (j_cur + 1) % j_max;
            if (blk_pos == 0) blk_pos = fs->journal.jsb.s_first;

            for (int i = 0; i < tag_count; i++) {
                ext4_read_block(fs, j_start + blk_pos, data_block);
                ext4_write_block(fs, tags[i].t_blocknr, data_block);
                blk_pos = (blk_pos + 1) % j_max;
                if (blk_pos == 0) blk_pos = fs->journal.jsb.s_first;
            }

            replayed_count++;
            j_cur = (commit_pos + 1) % j_max;
            if (j_cur == 0) j_cur = fs->journal.jsb.s_first;
        } else {
            /* Uncommitted partial transaction, stop replay */
            break;
        }
    }

    /* Mark journal clean */
    fs->journal.jsb.s_start = 0;
    fs->journal.transactions_replayed += replayed_count;

    memset(desc_block, 0, fs->block_size);
    memcpy(desc_block, &fs->journal.jsb, sizeof(journal_superblock_t));
    ext4_write_block(fs, j_start, desc_block);

    if (fs->bdev->ops->sync) fs->bdev->ops->sync(fs->bdev);

    pr_info("cherry_ext4: jbd2 recovery finished (%u transactions replayed)\n", replayed_count);

    kal_free(desc_block);
    kal_free(data_block);
    kal_free(commit_block);
    return 0;
}
