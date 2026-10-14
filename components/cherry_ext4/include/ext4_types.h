/*
 * CherryExt4 - Standard Ext4 On-Disk Structures & Constants
 *
 * Linux-compatible Ext4 (Fourth Extended Filesystem) definitions
 * Copyright (c) 2026 ForgeStitch
 * SPDX-License-Identifier: MIT
 */

#ifndef CHERRY_EXT4_TYPES_H
#define CHERRY_EXT4_TYPES_H

#include <stdint.h>
#include <stdbool.h>

#define EXT4_SUPER_MAGIC                0xEF53
#define EXT4_MIN_BLOCK_SIZE             1024
#define EXT4_MAX_BLOCK_SIZE             4096

/* Revision levels */
#define EXT4_GOOD_OLD_REV               0
#define EXT4_DYNAMIC_REV                1

/* Special Inode Numbers */
#define EXT4_BAD_INO                    1       /* Bad blocks inode */
#define EXT4_ROOT_INO                   2       /* Root directory inode */
#define EXT4_USR_QUOTA_INO              3       /* User quota inode */
#define EXT4_GRP_QUOTA_INO              4       /* Group quota inode */
#define EXT4_BOOT_LOADER_INO            5       /* Boot loader inode */
#define EXT4_UNDEL_DIR_INO              6       /* Undelete directory inode */
#define EXT4_RESIZE_INO                 7       /* Reserved group descriptors inode */
#define EXT4_JOURNAL_INO                8       /* Journal inode */
#define EXT4_EXCLUDE_INO                9       /* The exclude inode */
#define EXT4_REPLICA_INO                10      /* Replica inode */
#define EXT4_GOOD_OLD_FIRST_INO         11      /* First non-reserved inode */

/* Inode File Types / Mode */
#define EXT4_S_IFMT                     0xF000
#define EXT4_S_IFSOCK                   0xC000
#define EXT4_S_IFLNK                    0xA000
#define EXT4_S_IFREG                    0x8000
#define EXT4_S_IFBLK                    0x6000
#define EXT4_S_IFDIR                    0x4000
#define EXT4_S_IFCHR                    0x2000
#define EXT4_S_IFIFO                    0x1000

/* Inode Flags */
#define EXT4_SECRM_FL                   0x00000001 /* Secure deletion */
#define EXT4_UNRM_FL                    0x00000002 /* Undelete */
#define EXT4_COMPR_FL                   0x00000004 /* Compress file */
#define EXT4_SYNC_FL                    0x00000008 /* Synchronous updates */
#define EXT4_IMMUTABLE_FL               0x00000010 /* Immutable file */
#define EXT4_APPEND_FL                  0x00000020 /* writes to file may only append */
#define EXT4_NODUMP_FL                  0x00000040 /* do not dump file */
#define EXT4_NOATIME_FL                 0x00000080 /* do not update atime */
#define EXT4_INDEX_FL                   0x00001000 /* hash-indexed directory */
#define EXT4_JOURNAL_DATA_FL            0x00004000 /* file data should be journaled */
#define EXT4_EXTENTS_FL                 0x00080000 /* Inode uses extents */

/* Directory Entry File Types */
#define EXT4_FT_UNKNOWN                 0
#define EXT4_FT_REG_FILE                1
#define EXT4_FT_DIR                     2
#define EXT4_FT_CHRDEV                  3
#define EXT4_FT_BLKDEV                  4
#define EXT4_FT_FIFO                    5
#define EXT4_FT_SOCK                    6
#define EXT4_FT_SYMLINK                 7

/* Superblock Features */
#define EXT4_FEATURE_COMPAT_DIR_PREALLOC   0x0001
#define EXT4_FEATURE_COMPAT_IMAGIC_INODES  0x0002
#define EXT4_FEATURE_COMPAT_HAS_JOURNAL    0x0004
#define EXT4_FEATURE_COMPAT_EXT_ATTR       0x0008
#define EXT4_FEATURE_COMPAT_RESIZE_INODE   0x0010
#define EXT4_FEATURE_COMPAT_DIR_INDEX      0x0020

#define EXT4_FEATURE_RO_COMPAT_SPARSE_SUPER 0x0001
#define EXT4_FEATURE_RO_COMPAT_LARGE_FILE   0x0002
#define EXT4_FEATURE_RO_COMPAT_BTREE_DIR    0x0004
#define EXT4_FEATURE_RO_COMPAT_HUGE_FILE    0x0008
#define EXT4_FEATURE_RO_COMPAT_GDT_CSUM     0x0010
#define EXT4_FEATURE_RO_COMPAT_DIR_NLINK    0x0020
#define EXT4_FEATURE_RO_COMPAT_EXTRA_ISIZE  0x0040

#define EXT4_FEATURE_INCOMPAT_COMPRESSION  0x0001
#define EXT4_FEATURE_INCOMPAT_FILETYPE     0x0002
#define EXT4_FEATURE_INCOMPAT_RECOVER      0x0004 /* Needs recovery */
#define EXT4_FEATURE_INCOMPAT_JOURNAL_DEV  0x0008
#define EXT4_FEATURE_INCOMPAT_META_BG      0x0010
#define EXT4_FEATURE_INCOMPAT_EXTENTS      0x0040 /* Extents used */
#define EXT4_FEATURE_INCOMPAT_64BIT        0x0080
#define EXT4_FEATURE_INCOMPAT_MMP          0x0100
#define EXT4_FEATURE_INCOMPAT_FLEX_BG      0x0200

#define EXT4_VALID_FS                   0x0001
#define EXT4_ERROR_FS                   0x0002

#pragma pack(push, 1)

/* Ext4 Superblock (1024 bytes) */
struct ext4_super_block {
    uint32_t s_inodes_count;            /* Inodes count */
    uint32_t s_blocks_count_lo;         /* Blocks count */
    uint32_t s_r_blocks_count_lo;       /* Reserved blocks count */
    uint32_t s_free_blocks_count_lo;    /* Free blocks count */
    uint32_t s_free_inodes_count;       /* Free inodes count */
    uint32_t s_first_data_block;        /* First Data Block (1 for 1KB, 0 for >1KB) */
    uint32_t s_log_block_size;          /* Block size: 1024 << s_log_block_size */
    uint32_t s_log_cluster_size;        /* Allocation cluster size */
    uint32_t s_blocks_per_group;        /* # Blocks per group */
    uint32_t s_clusters_per_group;      /* # Clusters per group */
    uint32_t s_inodes_per_group;        /* # Inodes per group */
    uint32_t s_mtime;                   /* Mount time */
    uint32_t s_wtime;                   /* Write time */
    uint16_t s_mnt_count;               /* Mount count */
    uint16_t s_max_mnt_count;           /* Maximal mount count */
    uint16_t s_magic;                   /* Magic signature: 0xEF53 */
    uint16_t s_state;                   /* File system state */
    uint16_t s_errors;                  /* Behaviour when detecting errors */
    uint16_t s_minor_rev_level;         /* minor revision level */
    uint32_t s_lastcheck;               /* time of last check */
    uint32_t s_checkinterval;           /* max. time between checks */
    uint32_t s_creator_os;              /* OS */
    uint32_t s_rev_level;               /* Revision level (0: good old, 1: dynamic) */
    uint16_t s_def_resuid;              /* Default uid for reserved blocks */
    uint16_t s_def_resgid;              /* Default gid for reserved blocks */

    /* DYNAMIC_REV Specific */
    uint32_t s_first_ino;               /* First non-reserved inode (11) */
    uint16_t s_inode_size;              /* size of inode structure (128 or 256) */
    uint16_t s_block_group_nr;          /* block group # of this superblock */
    uint32_t s_feature_compat;          /* compatible feature set */
    uint32_t s_feature_incompat;        /* incompatible feature set */
    uint32_t s_feature_ro_compat;       /* readonly-compatible feature set */
    uint8_t  s_uuid[16];                /* 128-bit uuid for volume */
    char     s_volume_name[16];         /* volume name */
    char     s_last_mounted[64];        /* directory where last mounted */
    uint32_t s_algorithm_usage_bitmap;  /* For compression */
    uint8_t  s_prealloc_blocks;         /* Nr of blocks to try to preallocate*/
    uint8_t  s_prealloc_dir_blocks;     /* Nr to preallocate for dirs */
    uint16_t s_reserved_gdt_blocks;     /* Per group desc for online growth */

    /* Journaling Support */
    uint8_t  s_journal_uuid[16];        /* uuid of journal superblock */
    uint32_t s_journal_inum;            /* inode number of journal file (8) */
    uint32_t s_journal_dev;             /* device number of journal file */
    uint32_t s_last_orphan;             /* start of list of inodes to delete */
    uint32_t s_hash_seed[4];            /* HTREE hash seed */
    uint8_t  s_def_hash_version;        /* Default hash version to use */
    uint8_t  s_jnl_backup_type;
    uint16_t s_desc_size;               /* size of group descriptor (32 or 64) */
    uint32_t s_default_mount_opts;
    uint32_t s_first_meta_bg;           /* First metablock group */
    uint32_t s_mkfs_time;               /* When the filesystem was created */
    uint32_t s_jnl_blocks[17];          /* Backup of the journal inode */

    /* 64-bit support */
    uint32_t s_blocks_count_hi;         /* Blocks count */
    uint32_t s_r_blocks_count_hi;       /* Reserved blocks count */
    uint32_t s_free_blocks_count_hi;    /* Free blocks count */
    uint16_t s_min_extra_isize;         /* All inodes have at least # bytes */
    uint16_t s_want_extra_isize;        /* New inodes should reserve # bytes */
    uint32_t s_flags;                   /* Miscellaneous flags */
    uint16_t s_raid_stride;             /* RAID stride */
    uint16_t s_mmp_interval;            /* # seconds to wait in MMP checking */
    uint64_t s_mmp_block;               /* Block for multi-mount protection */
    uint32_t s_raid_stripe_width;       /* blocks on all data disks (N*stride)*/
    uint8_t  s_log_groups_per_flex;     /* FLEX_BG group size */
    uint8_t  s_checksum_type;
    uint16_t s_reserved_pad;
    uint64_t s_kbytes_written;          /* nr of lifetime kilobytes written */
    uint32_t s_snapshot_inum;           /* I-node number of active snapshot */
    uint32_t s_snapshot_id;             /* Sequential ID of active snapshot */
    uint64_t s_snapshot_r_blocks_count; /* reserved blocks for active snapshot's future use */
    uint32_t s_snapshot_list;           /* inode number of the head of the on-disk snapshot list */
    uint32_t s_error_count;             /* number of fs errors */
    uint32_t s_first_error_time;        /* first time an error happened */
    uint32_t s_first_error_ino;         /* inode involved in first error */
    uint64_t s_first_error_block;       /* block involved of first error */
    uint8_t  s_first_error_func[32];    /* function where the error happened */
    uint32_t s_first_error_line;        /* line number where error happened */
    uint32_t s_last_error_time;         /* most recent time of an error */
    uint32_t s_last_error_ino;          /* inode involved in last error */
    uint32_t s_last_error_line;         /* line number where error happened */
    uint64_t s_last_error_block;        /* block involved of last error */
    uint8_t  s_last_error_func[32];     /* function where the error happened */
    uint8_t  s_mount_opts[64];
    uint32_t s_usr_quota_inum;          /* inode for tracking user quota */
    uint32_t s_grp_quota_inum;          /* inode for tracking group quota */
    uint32_t s_overhead_clusters;       /* overhead blocks/clusters in fs */
    uint32_t s_backup_bgs[2];           /* groups with sparse_super2 SBs */
    uint8_t  s_encrypt_algos[4];        /* Encryption algorithms in use */
    uint8_t  s_encrypt_pw_salt[16];     /* Salt for string2key algorithm */
    uint32_t s_lpf_ino;                 /* Location of the lost+found inode */
    uint32_t s_prj_quota_inum;          /* inode for tracking project quota */
    uint32_t s_checksum_seed;           /* crc32c(uuid) if csum_seed set */
    uint32_t s_reserved[98];            /* Padding to the end of the block */
    uint32_t s_checksum;                /* crc32c(superblock) */
};

/* Block Group Descriptor (32 bytes or 64 bytes) */
struct ext4_group_desc {
    uint32_t bg_block_bitmap_lo;        /* Blocks bitmap block */
    uint32_t bg_inode_bitmap_lo;        /* Inodes bitmap block */
    uint32_t bg_inode_table_lo;         /* Inodes table block */
    uint16_t bg_free_blocks_count_lo;   /* Free blocks count */
    uint16_t bg_free_inodes_count_lo;   /* Free inodes count */
    uint16_t bg_used_dirs_count_lo;     /* Directories count */
    uint16_t bg_flags;                  /* EXT4_BG_flags (INODE_UNINIT, etc) */
    uint32_t bg_exclude_bitmap_lo;      /* Exclude bitmap for snapshots */
    uint16_t bg_block_bitmap_csum_lo;   /* crc32c(s_uuid+grp_num+bitmap) LSB */
    uint16_t bg_inode_bitmap_csum_lo;   /* crc32c(s_uuid+grp_num+bitmap) LSB */
    uint16_t bg_itable_unused_lo;       /* Unused inodes count */
    uint16_t bg_checksum;               /* crc16(sb_uuid+group+desc) */
    /* 64-bit descriptor additions */
    uint32_t bg_block_bitmap_hi;        /* Blocks bitmap block MSB */
    uint32_t bg_inode_bitmap_hi;        /* Inodes bitmap block MSB */
    uint32_t bg_inode_table_hi;         /* Inodes table block MSB */
    uint16_t bg_free_blocks_count_hi;   /* Free blocks count MSB */
    uint16_t bg_free_inodes_count_hi;   /* Free inodes count MSB */
    uint16_t bg_used_dirs_count_hi;     /* Directories count MSB */
    uint16_t bg_itable_unused_hi;       /* Unused inodes count MSB */
    uint32_t bg_exclude_bitmap_hi;      /* Exclude bitmap block MSB */
    uint16_t bg_block_bitmap_csum_hi;   /* crc32c(s_uuid+grp_num+bitmap) MSB */
    uint16_t bg_inode_bitmap_csum_hi;   /* crc32c(s_uuid+grp_num+bitmap) MSB */
    uint32_t bg_reserved;
};

/* Ext4 Extent Header (12 bytes) */
#define EXT4_EXT_MAGIC                  0xF30A

struct ext4_extent_header {
    uint16_t eh_magic;                  /* Magic: 0xF30A */
    uint16_t eh_entries;                /* Number of valid entries */
    uint16_t eh_max;                    /* Capacity of store in entries */
    uint16_t eh_depth;                  /* Has tree real underlying blocks? (0 = leaf) */
    uint32_t eh_generation;             /* generation of the tree */
};

/* Ext4 Extent Leaf (12 bytes) */
struct ext4_extent {
    uint32_t ee_block;                  /* First logical block extent covers */
    uint16_t ee_len;                    /* Number of blocks covered by extent */
    uint16_t ee_start_hi;               /* High 16 bits of physical block */
    uint32_t ee_start_lo;               /* Low 32 bits of physical block */
};

/* Ext4 Extent Index (12 bytes) */
struct ext4_extent_idx {
    uint32_t ei_block;                  /* Index covers logical blocks from 'ei_block' */
    uint32_t ei_leaf_lo;                /* Pointer to the physical block of the next level */
    uint16_t ei_leaf_hi;                /* High 16 bits of physical block */
    uint16_t ei_unused;
};

/* Ext4 Inode (128 or 256 bytes) */
struct ext4_inode {
    uint16_t i_mode;                    /* File mode */
    uint16_t i_uid;                     /* Low 16 bits of Owner Uid */
    uint32_t i_size_lo;                 /* Size in bytes */
    uint32_t i_atime;                   /* Access time */
    uint32_t i_ctime;                   /* Inode Change time */
    uint32_t i_mtime;                   /* Modification time */
    uint32_t i_dtime;                   /* Deletion Time */
    uint16_t i_gid;                     /* Low 16 bits of Group Id */
    uint16_t i_links_count;             /* Links count */
    uint32_t i_blocks_lo;               /* Blocks count (512-byte sectors) */
    uint32_t i_flags;                   /* File flags */
    union {
        struct {
            uint32_t l_i_version;
        } linux1;
        struct {
            uint32_t h_i_translator;
        } hurd1;
        struct {
            uint32_t m_i_reserved;
        } masix1;
    } osd1;                             /* OS dependent 1 */
    uint32_t i_block[15];               /* Pointers to blocks or Extent Tree Header (60 bytes) */
    uint32_t i_generation;              /* File version (for NFS) */
    uint32_t i_file_acl_lo;             /* File ACL */
    uint32_t i_size_high;               /* High 32 bits of size */
    uint32_t i_obso_faddr;              /* Obsoleted fragment address */
    union {
        struct {
            uint16_t l_i_blocks_high;   /* were l_i_reserved1 */
            uint16_t l_i_file_acl_high;
            uint16_t l_i_uid_high;      /* these 2 fields */
            uint16_t l_i_gid_high;      /* were reserved2[0] */
            uint16_t l_i_checksum_lo;   /* crc32c(uuid+inum+inode) LE */
            uint16_t l_i_reserved;
        } linux2;
        struct {
            uint16_t h_i_reserved1;     /* Obsoleted fragment number/size which are removed in ext4 */
            uint16_t h_i_mode_high;
            uint16_t h_i_uid_high;
            uint16_t h_i_gid_high;
            uint32_t h_i_author;
        } hurd2;
        struct {
            uint16_t m_i_reserved1;
            uint16_t m_i_file_acl_high;
            uint32_t m_i_reserved2[2];
        } masix2;
    } osd2;                             /* OS dependent 2 */
    uint16_t i_extra_isize;
    uint16_t i_checksum_hi;             /* crc32c(uuid+inum+inode) BE */
    uint32_t i_ctime_extra;             /* extra Change time (nsec << 2 | epoch) */
    uint32_t i_mtime_extra;             /* extra Modification time (nsec << 2 | epoch) */
    uint32_t i_atime_extra;             /* extra Access time (nsec << 2 | epoch) */
    uint32_t i_crtime;                  /* File creation time */
    uint32_t i_crtime_extra;            /* extra File creation time (nsec << 2 | epoch) */
    uint32_t i_version_hi;              /* high 32 bits for 64-bit version */
    uint32_t i_projid;                  /* Project ID */
};

/* Ext4 Directory Entry (Rev 2: with file_type) */
struct ext4_dir_entry_2 {
    uint32_t inode;                     /* Inode number */
    uint16_t rec_len;                   /* Directory entry length */
    uint8_t  name_len;                  /* Name length */
    uint8_t  file_type;                 /* File type (EXT4_FT_*) */
    char     name[255];                 /* File name */
};

#pragma pack(pop)

#endif /* CHERRY_EXT4_TYPES_H */
