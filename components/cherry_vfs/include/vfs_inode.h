/*
 * CherryVFS - Virtual File System for FreeRTOS
 *
 * Copyright (c) 2026 ForgeStitch
 * SPDX-License-Identifier: MIT
 *
 * Inode, Dentry, File, and Superblock Abstractions
 */

#ifndef _CHERRY_VFS_INODE_H_
#define _CHERRY_VFS_INODE_H_

#include "vfs_types.h"

#ifdef __cplusplus
extern "C" {
#endif

struct vfs_inode;
struct vfs_dentry;
struct vfs_file;
struct vfs_superblock;
struct vfs_filesystem_type;

/* Inode operations (directory and metadata operations) */
struct vfs_inode_operations {
    int (*lookup)(struct vfs_inode *dir, const char *name, struct vfs_inode **result);
    int (*create)(struct vfs_inode *dir, const char *name, mode_t mode, struct vfs_inode **result);
    int (*mkdir)(struct vfs_inode *dir, const char *name, mode_t mode);
    int (*rmdir)(struct vfs_inode *dir, const char *name);
    int (*unlink)(struct vfs_inode *dir, const char *name);
    int (*stat)(struct vfs_inode *inode, struct stat *st);
    int (*truncate)(struct vfs_inode *inode, off_t length);
};

/* File operations (I/O operations on opened files or devices) */
struct vfs_file_operations {
    int     (*open)(struct vfs_file *file);
    int     (*close)(struct vfs_file *file);
    ssize_t (*read)(struct vfs_file *file, void *buf, size_t count);
    ssize_t (*write)(struct vfs_file *file, const void *buf, size_t count);
    int     (*ioctl)(struct vfs_file *file, unsigned int cmd, unsigned long arg);
    off_t   (*lseek)(struct vfs_file *file, off_t offset, int whence);
    int     (*readdir)(struct vfs_file *file, struct dirent *dirent);
    int     (*sync)(struct vfs_file *file);
    int     (*poll)(struct vfs_file *file, void *poll_table);
};

/* Core Inode Representation */
struct vfs_inode {
    ino_t                           i_ino;      /* Unique inode number */
    mode_t                          i_mode;     /* File type and permissions (S_IF*) */
    uint32_t                        i_ref;      /* Reference count */
    off_t                           i_size;     /* File size in bytes */
    dev_t                           i_rdev;     /* Device ID for S_IFCHR or S_IFBLK */
    uint32_t                        i_flags;    /* Inode flags */
    struct vfs_superblock          *i_sb;       /* Pointer to superblock */
    const struct vfs_inode_operations *i_ops;   /* Inode operations table */
    const struct vfs_file_operations  *i_fops;  /* File operations table */
    void                           *i_private;  /* Filesystem/device specific data */
    struct vfs_inode               *i_next;     /* Inode hash/list linkage */
};

/* Directory Entry Cache (Dentry) */
struct vfs_dentry {
    char                    d_name[VFS_NAME_MAX]; /* Component name */
    struct vfs_dentry      *d_parent;             /* Parent dentry */
    struct vfs_inode       *d_inode;              /* Associated inode */
    struct vfs_dentry      *d_children;           /* Subdirectory / child list */
    struct vfs_dentry      *d_next_sibling;       /* Sibling in parent's children list */
    struct vfs_superblock  *d_mounted_sb;         /* Mounted filesystem if mountpoint */
    uint32_t                d_ref;                /* Reference count */
    uint32_t                d_flags;              /* Dentry flags */
};

/* Open File Description */
struct vfs_file {
    struct vfs_inode               *f_inode;    /* Associated inode */
    struct vfs_dentry              *f_dentry;   /* Associated dentry */
    const struct vfs_file_operations *f_ops;    /* Operations table */
    uint32_t                        f_flags;    /* O_RDONLY, O_WRONLY, O_APPEND, etc. */
    off_t                           f_pos;      /* Current file offset */
    uint32_t                        f_ref;      /* Open reference counter */
    void                           *f_private;  /* Driver/FS private state */
};

/* Filesystem Superblock */
struct vfs_superblock {
    const struct vfs_filesystem_type *s_type;    /* Filesystem type driver */
    struct vfs_dentry               *s_root;     /* Root dentry of mounted tree */
    char                             s_source[64];/* Source device or path */
    char                             s_target[VFS_PATH_MAX]; /* Mount point path */
    uint32_t                         s_flags;    /* Mount flags */
    void                            *s_fs_info;  /* Filesystem-specific private data */
    struct vfs_superblock           *s_next;     /* Mount table list linkage */
};

/* Filesystem Type Driver */
struct vfs_filesystem_type {
    const char *name;
    int (*mount)(struct vfs_superblock *sb, const char *source, unsigned long flags, const void *data);
    int (*unmount)(struct vfs_superblock *sb);
    struct vfs_filesystem_type *next;
};

/* Directory Stream (DIR) for opendir / readdir / closedir */
typedef struct {
    int             fd;
    struct dirent   cur_entry;
    off_t           offset;
} DIR;

#ifdef __cplusplus
}
#endif

#endif /* _CHERRY_VFS_INODE_H_ */
