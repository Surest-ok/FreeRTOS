/*
 * CherryExt4 - JBD2 (Journaling Block Device 2) Definitions & Transaction Engine
 *
 * Linux JBD2 compatible journaling structures and transaction management
 * Copyright (c) 2026 ForgeStitch
 * SPDX-License-Identifier: MIT
 */

#ifndef CHERRY_EXT4_JBD2_H
#define CHERRY_EXT4_JBD2_H

#include <stdint.h>
#include <stdbool.h>
#include "cherry_kal.h"

#define JBD2_MAGIC_NUMBER               0xC03B3998U

/* Standard JBD2 Block Types */
#define JBD2_DESCRIPTOR_BLOCK           1
#define JBD2_COMMIT_BLOCK               2
#define JBD2_SUPERBLOCK_V1              3
#define JBD2_SUPERBLOCK_V2              4
#define JBD2_REVOKE_BLOCK               5

/* JBD2 Tag Flags */
#define JBD2_FLAG_ESCAPE                1       /* Escaped magic number */
#define JBD2_FLAG_SAME_UUID             2       /* Same UUID as journal */
#define JBD2_FLAG_DELETED               4       /* Block deleted */
#define JBD2_FLAG_LAST_TAG              8       /* Last tag in descriptor */

#define JBD2_MAX_TRANS_BLOCKS           32

#pragma pack(push, 1)

/* Standard 12-byte Journal Header */
typedef struct journal_header_s {
    uint32_t h_magic;                   /* Magic: 0xC03B3998 */
    uint32_t h_blocktype;               /* JBD2_DESCRIPTOR_BLOCK, etc. */
    uint32_t h_sequence;                /* Transaction sequence ID */
} journal_header_t;

/* JBD2 Block Tag (describing one payload block in the journal) */
typedef struct journal_block_tag_s {
    uint32_t t_blocknr;                 /* Target filesystem block number */
    uint32_t t_flags;                   /* JBD2_FLAG_* */
} journal_block_tag_t;

/* JBD2 Superblock (at block 0 of journal area) */
typedef struct journal_superblock_s {
    journal_header_t s_header;

    /* Static information describing the journal */
    uint32_t s_blocksize;               /* Journal device block size */
    uint32_t s_maxlen;                  /* Total blocks in journal file */
    uint32_t s_first;                   /* First block of log information */

    /* Dynamic information describing the current state of the log */
    uint32_t s_sequence;                /* First valid transaction ID */
    uint32_t s_start;                   /* Block # of first valid transaction (0=clean) */
    uint32_t s_errno;                   /* Error code */

    /* The remaining fields are for V2 superblock */
    uint32_t s_feature_compat;          /* Compatible feature set */
    uint32_t s_feature_incompat;        /* Incompatible feature set */
    uint32_t s_feature_ro_compat;       /* Readonly-compatible feature set */
    uint8_t  s_uuid[16];                /* 128-bit uuid for journal */
    uint32_t s_nr_users;                /* Nr of fs clients */
    uint32_t s_dynsuper;                /* Block # of dynamic superblock copy */
    uint32_t s_max_trans;               /* Limit of journal blocks per trans */
    uint32_t s_max_trans_data;          /* Limit of data blocks per trans */
    uint32_t s_checksum_type;           /* Checksum type */
    uint8_t  s_padding2[40];
    uint32_t s_checksum;                /* crc32c(superblock) */
    uint8_t  s_users[16*48];            /* Ids of all fs clients */
} journal_superblock_t;

/* JBD2 Commit Block */
typedef struct journal_commit_header_s {
    journal_header_t h_header;
    uint8_t  h_chksum_type;
    uint8_t  h_chksum_size;
    uint8_t  h_padding[2];
    uint32_t h_chksum[8];
    uint64_t h_commit_sec;              /* Commit timestamp (seconds) */
    uint32_t h_commit_nsec;             /* Commit timestamp (nanoseconds) */
} journal_commit_header_t;

#pragma pack(pop)

/* Forward declaration */
struct ext4_fs;

/* In-memory modified metadata buffer in a transaction */
struct jbd2_buf {
    uint32_t fs_block;                  /* Destination filesystem block */
    void    *data;                      /* Buffer data */
    bool     is_dirty;
};

/* In-memory transaction handle */
struct jbd2_handle {
    struct ext4_fs  *fs;
    uint32_t         trans_id;
    int              num_bufs;
    struct jbd2_buf  bufs[JBD2_MAX_TRANS_BLOCKS];
};

/* Journal context attached to ext4 filesystem */
struct jbd2_journal {
    struct ext4_fs         *fs;
    uint32_t                journal_start_block; /* Start block of journal on disk */
    uint32_t                journal_total_blocks;/* Total journal blocks */
    journal_superblock_t    jsb;
    uint32_t                cur_log_block;       /* Next free journal block in ring */
    kal_mutex_t             lock;
    bool                    is_active;
    uint32_t                transactions_committed;
    uint32_t                transactions_replayed;
};

/* JBD2 Operations */
int jbd2_init(struct ext4_fs *fs);
int jbd2_format(struct ext4_fs *fs, uint32_t start_block, uint32_t num_blocks);
struct jbd2_handle *jbd2_trans_start(struct ext4_fs *fs, int credits);
int jbd2_trans_get_write_access(struct jbd2_handle *h, uint32_t fs_block, const void *data);
int jbd2_trans_commit(struct jbd2_handle *h);
int jbd2_replay(struct ext4_fs *fs);

#endif /* CHERRY_EXT4_JBD2_H */
