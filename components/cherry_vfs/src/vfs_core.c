/*
 * CherryVFS - Virtual File System for FreeRTOS
 *
 * Copyright (c) 2026 ForgeStitch
 * SPDX-License-Identifier: MIT
 *
 * Core VFS: Inode/Dentry Management, Path Resolution, Mount Table, FD Table
 */

#include "cherry_vfs.h"
#include <string.h>

/* Kernel Abstraction Layer (KAL) for memory and synchronization */
#include "cherry_kal.h"

/* Static state */
static struct vfs_filesystem_type *g_fs_list = NULL;
static struct vfs_superblock      *g_mount_list = NULL;
static struct vfs_superblock      *g_root_sb = NULL;
static struct vfs_file            *g_fd_table[VFS_MAX_FD] = {0};
static kal_mutex_t                 g_vfs_mutex;
static int                         g_vfs_mutex_inited = 0;
static uint32_t                    g_next_inode_num = 1;

void *vfs_malloc(size_t size)
{
    return kal_malloc(size);
}

void vfs_free(void *ptr)
{
    kal_free(ptr);
}

void vfs_lock(void)
{
    if (g_vfs_mutex_inited) {
        kal_mutex_lock(&g_vfs_mutex);
    }
}

void vfs_unlock(void)
{
    if (g_vfs_mutex_inited) {
        kal_mutex_unlock(&g_vfs_mutex);
    }
}


ino_t vfs_next_ino(void)
{
    vfs_lock();
    ino_t ino = (ino_t)(g_next_inode_num++);
    vfs_unlock();
    return ino;
}

struct vfs_superblock *vfs_get_root_sb(void)
{
    return g_root_sb;
}

/* =========================================================================
 * Inode Management
 * ========================================================================= */
struct vfs_inode *vfs_inode_alloc(struct vfs_superblock *sb, mode_t mode)
{
    struct vfs_inode *inode = (struct vfs_inode *)vfs_malloc(sizeof(struct vfs_inode));
    if (!inode) {
        return NULL;
    }
    memset(inode, 0, sizeof(struct vfs_inode));
    inode->i_ino = vfs_next_ino();
    inode->i_mode = mode;
    inode->i_ref = 1;
    inode->i_sb = sb;
    return inode;
}

void vfs_inode_ref(struct vfs_inode *inode)
{
    if (inode) {
        vfs_lock();
        inode->i_ref++;
        vfs_unlock();
    }
}

void vfs_inode_put(struct vfs_inode *inode)
{
    if (!inode) return;

    vfs_lock();
    if (inode->i_ref > 0) {
        inode->i_ref--;
    }
    if (inode->i_ref == 0) {
        vfs_free(inode);
    }
    vfs_unlock();
}

/* =========================================================================
 * Dentry Management
 * ========================================================================= */
struct vfs_dentry *vfs_dentry_alloc(const char *name, struct vfs_dentry *parent)
{
    struct vfs_dentry *dentry = (struct vfs_dentry *)vfs_malloc(sizeof(struct vfs_dentry));
    if (!dentry) {
        return NULL;
    }
    memset(dentry, 0, sizeof(struct vfs_dentry));
    if (name) {
        strncpy(dentry->d_name, name, VFS_NAME_MAX - 1);
        dentry->d_name[VFS_NAME_MAX - 1] = '\0';
    }
    dentry->d_parent = parent ? parent : dentry;
    dentry->d_ref = 1;

    if (parent && parent != dentry) {
        vfs_lock();
        dentry->d_next_sibling = parent->d_children;
        parent->d_children = dentry;
        vfs_unlock();
    }

    return dentry;
}

void vfs_dentry_ref(struct vfs_dentry *dentry)
{
    if (dentry) {
        vfs_lock();
        dentry->d_ref++;
        vfs_unlock();
    }
}

void vfs_dentry_put(struct vfs_dentry *dentry)
{
    if (!dentry) return;

    vfs_lock();
    if (dentry->d_ref > 0) {
        dentry->d_ref--;
    }
    if (dentry->d_ref == 0) {
        if (dentry->d_inode) {
            vfs_inode_put(dentry->d_inode);
            dentry->d_inode = NULL;
        }
        vfs_free(dentry);
    }
    vfs_unlock();
}

/* =========================================================================
 * Path Resolution
 * ========================================================================= */
static void normalize_path(const char *src, char *dst, size_t dst_size)
{
    if (!src || !dst || dst_size == 0) return;

    size_t i = 0, j = 0;
    if (src[0] != '/') {
        dst[j++] = '/';
    }
    while (src[i] && j < dst_size - 1) {
        if (src[i] == '/') {
            dst[j++] = '/';
            while (src[i] == '/') i++;
        } else {
            dst[j++] = src[i++];
        }
    }
    /* Strip trailing slash except for root */
    if (j > 1 && dst[j - 1] == '/') {
        j--;
    }
    dst[j] = '\0';
}

int vfs_path_lookup(const char *path, int flags, struct vfs_dentry **res_dentry)
{
    (void)flags;
    if (!path || !res_dentry) {
        return -EINVAL;
    }
    if (!g_root_sb || !g_root_sb->s_root) {
        return -ENOENT;
    }

    char norm_path[VFS_PATH_MAX];
    normalize_path(path, norm_path, sizeof(norm_path));

    vfs_lock();
    struct vfs_dentry *curr = g_root_sb->s_root;

    if (strcmp(norm_path, "/") == 0) {
        *res_dentry = curr;
        vfs_unlock();
        return VFS_OK;
    }

    char *p = norm_path + 1; /* Skip leading '/' */
    while (*p) {
        char comp[VFS_NAME_MAX];
        size_t len = 0;
        while (*p && *p != '/' && len < sizeof(comp) - 1) {
            comp[len++] = *p++;
        }
        comp[len] = '\0';
        if (*p == '/') p++;

        if (strcmp(comp, ".") == 0) {
            continue;
        }
        if (strcmp(comp, "..") == 0) {
            curr = curr->d_parent;
            continue;
        }

        /* Check if current dentry is a mount point */
        if (curr->d_mounted_sb && curr->d_mounted_sb->s_root) {
            curr = curr->d_mounted_sb->s_root;
        }

        /* Look for child in dentry cache */
        struct vfs_dentry *child = curr->d_children;
        struct vfs_dentry *found = NULL;
        while (child) {
            if (strcmp(child->d_name, comp) == 0) {
                found = child;
                break;
            }
            child = child->d_next_sibling;
        }

        /* If not in cache, query filesystem inode lookup */
        if (!found) {
            if (!curr->d_inode || !curr->d_inode->i_ops || !curr->d_inode->i_ops->lookup) {
                vfs_unlock();
                return -ENOENT;
            }
            struct vfs_inode *res_inode = NULL;
            int ret = curr->d_inode->i_ops->lookup(curr->d_inode, comp, &res_inode);
            if (ret != VFS_OK || !res_inode) {
                vfs_unlock();
                return -ENOENT;
            }
            /* Create new cached dentry */
            found = vfs_dentry_alloc(comp, curr);
            if (!found) {
                vfs_inode_put(res_inode);
                vfs_unlock();
                return -ENOMEM;
            }
            found->d_inode = res_inode;
        }

        curr = found;
        /* If resolved node is a mount point, traverse into mounted root */
        if (curr->d_mounted_sb && curr->d_mounted_sb->s_root) {
            curr = curr->d_mounted_sb->s_root;
        }
    }

    *res_dentry = curr;
    vfs_unlock();
    return VFS_OK;
}

int vfs_path_lookup_parent(const char *path, struct vfs_dentry **res_parent, char *last_comp)
{
    if (!path || !res_parent || !last_comp) {
        return -EINVAL;
    }
    char norm_path[VFS_PATH_MAX];
    normalize_path(path, norm_path, sizeof(norm_path));

    char *slash = strrchr(norm_path, '/');
    if (!slash) {
        return -EINVAL;
    }

    if (slash == norm_path) {
        /* Direct child of root, e.g. "/dev" or "/file" */
        strncpy(last_comp, slash + 1, VFS_NAME_MAX - 1);
        last_comp[VFS_NAME_MAX - 1] = '\0';
        return vfs_path_lookup("/", 0, res_parent);
    }

    *slash = '\0';
    strncpy(last_comp, slash + 1, VFS_NAME_MAX - 1);
    last_comp[VFS_NAME_MAX - 1] = '\0';

    return vfs_path_lookup(norm_path, 0, res_parent);
}

/* =========================================================================
 * Filesystem & Mount Management
 * ========================================================================= */
int vfs_register_fs(struct vfs_filesystem_type *fs)
{
    if (!fs || !fs->name) {
        return -EINVAL;
    }
    vfs_lock();
    struct vfs_filesystem_type *cur = g_fs_list;
    while (cur) {
        if (strcmp(cur->name, fs->name) == 0) {
            vfs_unlock();
            return -EEXIST;
        }
        cur = cur->next;
    }
    fs->next = g_fs_list;
    g_fs_list = fs;
    vfs_unlock();
    return VFS_OK;
}

int vfs_unregister_fs(struct vfs_filesystem_type *fs)
{
    if (!fs) return -EINVAL;
    vfs_lock();
    struct vfs_filesystem_type **curr = &g_fs_list;
    while (*curr) {
        if (*curr == fs) {
            *curr = fs->next;
            vfs_unlock();
            return VFS_OK;
        }
        curr = &(*curr)->next;
    }
    vfs_unlock();
    return -ENOENT;
}

struct vfs_filesystem_type *vfs_find_fs(const char *name)
{
    if (!name) return NULL;
    vfs_lock();
    struct vfs_filesystem_type *cur = g_fs_list;
    while (cur) {
        if (strcmp(cur->name, name) == 0) {
            vfs_unlock();
            return cur;
        }
        cur = cur->next;
    }
    vfs_unlock();
    return NULL;
}

int vfs_mount(const char *source, const char *target, const char *fstype, unsigned long flags, const void *data)
{
    if (!target || !fstype) {
        return -EINVAL;
    }

    struct vfs_filesystem_type *fs = vfs_find_fs(fstype);
    if (!fs || !fs->mount) {
        return -ENODEV;
    }

    struct vfs_superblock *sb = (struct vfs_superblock *)vfs_malloc(sizeof(struct vfs_superblock));
    if (!sb) {
        return -ENOMEM;
    }
    memset(sb, 0, sizeof(struct vfs_superblock));
    sb->s_type = fs;
    sb->s_flags = flags;
    if (source) strncpy(sb->s_source, source, sizeof(sb->s_source) - 1);
    strncpy(sb->s_target, target, sizeof(sb->s_target) - 1);

    int ret = fs->mount(sb, source, flags, data);
    if (ret != VFS_OK || !sb->s_root) {
        vfs_free(sb);
        return ret ? ret : -EINVAL;
    }

    vfs_lock();
    /* If mounting rootfs at "/" */
    if (strcmp(target, "/") == 0) {
        g_root_sb = sb;
    } else {
        /* Attach to target dentry */
        struct vfs_dentry *target_dentry = NULL;
        ret = vfs_path_lookup(target, 0, &target_dentry);
        if (ret != VFS_OK || !target_dentry) {
            vfs_unlock();
            if (fs->unmount) fs->unmount(sb);
            vfs_free(sb);
            return -ENOENT;
        }
        target_dentry->d_mounted_sb = sb;
    }

    sb->s_next = g_mount_list;
    g_mount_list = sb;
    vfs_unlock();

    return VFS_OK;
}

int vfs_umount(const char *target)
{
    if (!target) return -EINVAL;
    vfs_lock();

    struct vfs_superblock **curr = &g_mount_list;
    while (*curr) {
        if (strcmp((*curr)->s_target, target) == 0) {
            struct vfs_superblock *sb = *curr;
            *curr = sb->s_next;

            if (sb == g_root_sb) {
                g_root_sb = NULL;
            } else {
                struct vfs_dentry *td = NULL;
                if (vfs_path_lookup(target, 0, &td) == VFS_OK && td) {
                    td->d_mounted_sb = NULL;
                }
            }

            if (sb->s_type && sb->s_type->unmount) {
                sb->s_type->unmount(sb);
            }
            vfs_free(sb);
            vfs_unlock();
            return VFS_OK;
        }
        curr = &(*curr)->s_next;
    }

    vfs_unlock();
    return -ENOENT;
}

/* =========================================================================
 * File Descriptor Table Management
 * ========================================================================= */
int vfs_fd_alloc(struct vfs_file *file)
{
    if (!file) return -EINVAL;

    vfs_lock();
    /* Reserved: 0 (stdin), 1 (stdout), 2 (stderr) */
    for (int fd = 3; fd < VFS_MAX_FD; fd++) {
        if (g_fd_table[fd] == NULL) {
            g_fd_table[fd] = file;
            file->f_ref++;
            vfs_unlock();
            return fd;
        }
    }
    vfs_unlock();
    return -EMFILE;
}

void vfs_fd_free(int fd)
{
    if (fd < 0 || fd >= VFS_MAX_FD) return;

    vfs_lock();
    struct vfs_file *file = g_fd_table[fd];
    g_fd_table[fd] = NULL;
    vfs_unlock();

    if (file) {
        vfs_file_put(file);
    }
}

struct vfs_file *vfs_file_get(int fd)
{
    if (fd < 0 || fd >= VFS_MAX_FD) return NULL;

    vfs_lock();
    struct vfs_file *file = g_fd_table[fd];
    if (file) {
        file->f_ref++;
    }
    vfs_unlock();
    return file;
}

void vfs_file_put(struct vfs_file *file)
{
    if (!file) return;

    vfs_lock();
    if (file->f_ref > 0) {
        file->f_ref--;
    }
    if (file->f_ref == 0) {
        if (file->f_ops && file->f_ops->close) {
            file->f_ops->close(file);
        }
        if (file->f_inode) {
            vfs_inode_put(file->f_inode);
        }
        if (file->f_dentry) {
            vfs_dentry_put(file->f_dentry);
        }
        vfs_free(file);
    }
    vfs_unlock();
}

/* =========================================================================
 * VFS Initialization
 * ========================================================================= */
int vfs_init(void)
{
    if (!g_vfs_mutex_inited) {
        if (kal_mutex_init(&g_vfs_mutex) == KAL_OK) {
            g_vfs_mutex_inited = 1;
        }
    }
    return VFS_OK;
}
