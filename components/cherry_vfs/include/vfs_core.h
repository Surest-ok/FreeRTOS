/*
 * CherryVFS - Virtual File System for FreeRTOS
 *
 * Copyright (c) 2026 ForgeStitch
 * SPDX-License-Identifier: MIT
 *
 * VFS Core: Path Resolution, Mount Table, FD Management
 */

#ifndef _CHERRY_VFS_CORE_H_
#define _CHERRY_VFS_CORE_H_

#include "vfs_types.h"
#include "vfs_inode.h"

#ifdef __cplusplus
extern "C" {
#endif

/* VFS Initialization */
int vfs_init(void);

/* Synchronization Lock for VFS Structures */
void vfs_lock(void);
void vfs_unlock(void);

/* Dynamic Memory Allocators */
void *vfs_malloc(size_t size);
void vfs_free(void *ptr);

/* Filesystem Registration */
int vfs_register_fs(struct vfs_filesystem_type *fs);
int vfs_unregister_fs(struct vfs_filesystem_type *fs);
struct vfs_filesystem_type *vfs_find_fs(const char *name);

/* Mount and Unmount Management */
int vfs_mount(const char *source, const char *target, const char *fstype, unsigned long flags, const void *data);
int vfs_umount(const char *target);
struct vfs_superblock *vfs_get_root_sb(void);

/* Inode and Dentry Lifecycle */
struct vfs_inode *vfs_inode_alloc(struct vfs_superblock *sb, mode_t mode);
void vfs_inode_ref(struct vfs_inode *inode);
void vfs_inode_put(struct vfs_inode *inode);

struct vfs_dentry *vfs_dentry_alloc(const char *name, struct vfs_dentry *parent);
void vfs_dentry_ref(struct vfs_dentry *dentry);
void vfs_dentry_put(struct vfs_dentry *dentry);

/* Path Resolution (Look up dentry corresponding to absolute or relative path) */
int vfs_path_lookup(const char *path, int flags, struct vfs_dentry **res_dentry);

/* Path Resolution Helper for Parent Directory (for create / mkdir / unlink) */
int vfs_path_lookup_parent(const char *path, struct vfs_dentry **res_parent, char *last_comp);

/* File Descriptor Table Management */
int vfs_fd_alloc(struct vfs_file *file);
void vfs_fd_free(int fd);
struct vfs_file *vfs_file_get(int fd);
void vfs_file_put(struct vfs_file *file);

/* Inode Numbers Generator */
ino_t vfs_next_ino(void);

#ifdef __cplusplus
}
#endif

#endif /* _CHERRY_VFS_CORE_H_ */
