/*
 * CherryVFS - Virtual File System for FreeRTOS
 *
 * Copyright (c) 2026 ForgeStitch
 * SPDX-License-Identifier: MIT
 *
 * RamFS: In-Memory Hierarchical File System (Regular Files and Directories)
 */

#include "cherry_vfs.h"
#include <string.h>

#define RAMFS_FILE_INITIAL_CAPACITY 256

/* Internal In-Memory Node Representation */
struct ramfs_node {
    char                name[VFS_NAME_MAX];
    mode_t              mode;               /* S_IFDIR or S_IFREG */
    ino_t               ino;
    struct vfs_inode   *inode;              /* Associated VFS Inode */

    /* Directory specific fields */
    struct ramfs_node  *parent;
    struct ramfs_node  *children;           /* List of entries if S_ISDIR */
    struct ramfs_node  *next_sibling;

    /* Regular file specific fields */
    char               *data;               /* File content buffer if S_ISREG */
    size_t              capacity;           /* Allocated buffer capacity */
    off_t               size;               /* Current file size */
};

/* Forward declarations */
static const struct vfs_inode_operations g_ramfs_dir_iops;
static const struct vfs_file_operations  g_ramfs_dir_fops;
static const struct vfs_inode_operations g_ramfs_file_iops;
static const struct vfs_file_operations  g_ramfs_file_fops;

static struct ramfs_node *ramfs_alloc_node(const char *name, mode_t mode, struct ramfs_node *parent, struct vfs_superblock *sb)
{
    struct ramfs_node *node = (struct ramfs_node *)vfs_malloc(sizeof(struct ramfs_node));
    if (!node) return NULL;

    memset(node, 0, sizeof(struct ramfs_node));
    if (name) {
        strncpy(node->name, name, VFS_NAME_MAX - 1);
        node->name[VFS_NAME_MAX - 1] = '\0';
    }
    node->mode = mode;
    node->parent = parent ? parent : node;

    struct vfs_inode *inode = vfs_inode_alloc(sb, mode);
    if (!inode) {
        vfs_free(node);
        return NULL;
    }
    node->ino = inode->i_ino;
    node->inode = inode;
    inode->i_private = node;

    if (S_ISDIR(mode)) {
        inode->i_ops = &g_ramfs_dir_iops;
        inode->i_fops = &g_ramfs_dir_fops;
    } else {
        inode->i_ops = &g_ramfs_file_iops;
        inode->i_fops = &g_ramfs_file_fops;
        node->capacity = RAMFS_FILE_INITIAL_CAPACITY;
        node->data = (char *)vfs_malloc(node->capacity);
        if (node->data) {
            memset(node->data, 0, node->capacity);
        }
    }

    if (parent && parent != node) {
        node->next_sibling = parent->children;
        parent->children = node;
    }

    return node;
}

static void ramfs_free_node(struct ramfs_node *node)
{
    if (!node) return;
    if (S_ISREG(node->mode) && node->data) {
        vfs_free(node->data);
        node->data = NULL;
    }
    vfs_free(node);
}

/* =========================================================================
 * Regular File Operations
 * ========================================================================= */
static int ramfs_file_open(struct vfs_file *file)
{
    (void)file;
    return VFS_OK;
}

static int ramfs_file_close(struct vfs_file *file)
{
    (void)file;
    return VFS_OK;
}

static ssize_t ramfs_file_read(struct vfs_file *file, void *buf, size_t count)
{
    if (!file || !file->f_inode || !file->f_inode->i_private || !buf) {
        return -EINVAL;
    }
    struct ramfs_node *node = (struct ramfs_node *)file->f_inode->i_private;

    if (file->f_pos >= node->size) {
        return 0; /* EOF */
    }

    size_t avail = (size_t)(node->size - file->f_pos);
    size_t to_read = (count < avail) ? count : avail;

    if (to_read > 0 && node->data) {
        memcpy(buf, node->data + file->f_pos, to_read);
        file->f_pos += to_read;
    }

    return (ssize_t)to_read;
}

static ssize_t ramfs_file_write(struct vfs_file *file, const void *buf, size_t count)
{
    if (!file || !file->f_inode || !file->f_inode->i_private) {
        return -EINVAL;
    }
    if (count == 0) return 0;
    if (!buf) return -EINVAL;

    struct ramfs_node *node = (struct ramfs_node *)file->f_inode->i_private;

    /* If O_APPEND, write to end of file */
    if (file->f_flags & O_APPEND) {
        file->f_pos = node->size;
    }

    size_t req_capacity = (size_t)(file->f_pos + count);
    if (req_capacity > node->capacity) {
        size_t new_cap = node->capacity ? (node->capacity * 2) : RAMFS_FILE_INITIAL_CAPACITY;
        while (new_cap < req_capacity) {
            new_cap *= 2;
        }

        char *new_buf = (char *)vfs_malloc(new_cap);
        if (!new_buf) {
            return -ENOMEM;
        }
        memset(new_buf, 0, new_cap);
        if (node->data && node->size > 0) {
            memcpy(new_buf, node->data, node->size);
        }
        if (node->data) {
            vfs_free(node->data);
        }
        node->data = new_buf;
        node->capacity = new_cap;
    }

    memcpy(node->data + file->f_pos, buf, count);
    file->f_pos += count;

    if (file->f_pos > node->size) {
        node->size = file->f_pos;
        file->f_inode->i_size = node->size;
    }

    return (ssize_t)count;
}

static off_t ramfs_file_lseek(struct vfs_file *file, off_t offset, int whence)
{
    if (!file || !file->f_inode || !file->f_inode->i_private) {
        return -EINVAL;
    }
    struct ramfs_node *node = (struct ramfs_node *)file->f_inode->i_private;
    off_t new_pos = 0;

    switch (whence) {
    case SEEK_SET:
        new_pos = offset;
        break;
    case SEEK_CUR:
        new_pos = file->f_pos + offset;
        break;
    case SEEK_END:
        new_pos = node->size + offset;
        break;
    default:
        return -EINVAL;
    }

    if (new_pos < 0) {
        return -EINVAL;
    }
    file->f_pos = new_pos;
    return new_pos;
}

static int ramfs_file_truncate(struct vfs_inode *inode, off_t length)
{
    if (!inode || !inode->i_private || length < 0) return -EINVAL;
    struct ramfs_node *node = (struct ramfs_node *)inode->i_private;

    if (length < node->size) {
        node->size = length;
        inode->i_size = length;
    }
    return VFS_OK;
}

static int ramfs_file_stat(struct vfs_inode *inode, struct stat *st)
{
    if (!inode || !inode->i_private || !st) return -EINVAL;
    struct ramfs_node *node = (struct ramfs_node *)inode->i_private;

    st->st_ino = node->ino;
    st->st_mode = node->mode;
    st->st_size = node->size;
    st->st_nlink = 1;
    st->st_blksize = 512;
    st->st_blocks = (uint32_t)((node->size + 511) / 512);
    return VFS_OK;
}

static const struct vfs_file_operations g_ramfs_file_fops = {
    .open = ramfs_file_open,
    .close = ramfs_file_close,
    .read = ramfs_file_read,
    .write = ramfs_file_write,
    .ioctl = NULL,
    .lseek = ramfs_file_lseek,
    .readdir = NULL,
    .sync = NULL,
};

static const struct vfs_inode_operations g_ramfs_file_iops = {
    .lookup = NULL,
    .create = NULL,
    .mkdir = NULL,
    .rmdir = NULL,
    .unlink = NULL,
    .stat = ramfs_file_stat,
    .truncate = ramfs_file_truncate,
};

/* =========================================================================
 * Directory Operations
 * ========================================================================= */
static int ramfs_lookup(struct vfs_inode *dir, const char *name, struct vfs_inode **result)
{
    if (!dir || !dir->i_private || !name || !result) {
        return -EINVAL;
    }
    struct ramfs_node *dir_node = (struct ramfs_node *)dir->i_private;

    struct ramfs_node *child = dir_node->children;
    while (child) {
        if (strcmp(child->name, name) == 0) {
            *result = child->inode;
            vfs_inode_ref(child->inode);
            return VFS_OK;
        }
        child = child->next_sibling;
    }

    return -ENOENT;
}

static int ramfs_create(struct vfs_inode *dir, const char *name, mode_t mode, struct vfs_inode **result)
{
    if (!dir || !dir->i_private || !name || !result) {
        return -EINVAL;
    }
    struct ramfs_node *dir_node = (struct ramfs_node *)dir->i_private;

    /* Check if already exists */
    struct ramfs_node *child = dir_node->children;
    while (child) {
        if (strcmp(child->name, name) == 0) {
            return -EEXIST;
        }
        child = child->next_sibling;
    }

    struct ramfs_node *new_node = ramfs_alloc_node(name, S_IFREG | (mode & 0777), dir_node, dir->i_sb);
    if (!new_node) {
        return -ENOMEM;
    }

    *result = new_node->inode;
    vfs_inode_ref(new_node->inode);
    return VFS_OK;
}

static int ramfs_mkdir(struct vfs_inode *dir, const char *name, mode_t mode)
{
    if (!dir || !dir->i_private || !name) {
        return -EINVAL;
    }
    struct ramfs_node *dir_node = (struct ramfs_node *)dir->i_private;

    struct ramfs_node *child = dir_node->children;
    while (child) {
        if (strcmp(child->name, name) == 0) {
            return -EEXIST;
        }
        child = child->next_sibling;
    }

    struct ramfs_node *new_dir = ramfs_alloc_node(name, S_IFDIR | (mode & 0777), dir_node, dir->i_sb);
    if (!new_dir) {
        return -ENOMEM;
    }

    return VFS_OK;
}

static int ramfs_rmdir(struct vfs_inode *dir, const char *name)
{
    if (!dir || !dir->i_private || !name) return -EINVAL;
    struct ramfs_node *dir_node = (struct ramfs_node *)dir->i_private;

    struct ramfs_node **curr = &dir_node->children;
    while (*curr) {
        if (strcmp((*curr)->name, name) == 0) {
            struct ramfs_node *target = *curr;
            if (!S_ISDIR(target->mode)) {
                return -ENOTDIR;
            }
            if (target->children != NULL) {
                return -ENOTEMPTY;
            }
            *curr = target->next_sibling;
            ramfs_free_node(target);
            return VFS_OK;
        }
        curr = &(*curr)->next_sibling;
    }

    return -ENOENT;
}

static int ramfs_unlink(struct vfs_inode *dir, const char *name)
{
    if (!dir || !dir->i_private || !name) return -EINVAL;
    struct ramfs_node *dir_node = (struct ramfs_node *)dir->i_private;

    struct ramfs_node **curr = &dir_node->children;
    while (*curr) {
        if (strcmp((*curr)->name, name) == 0) {
            struct ramfs_node *target = *curr;
            if (S_ISDIR(target->mode)) {
                return -EISDIR;
            }
            *curr = target->next_sibling;
            ramfs_free_node(target);
            return VFS_OK;
        }
        curr = &(*curr)->next_sibling;
    }

    return -ENOENT;
}

static int ramfs_readdir(struct vfs_file *file, struct dirent *dirent)
{
    if (!file || !file->f_inode || !file->f_inode->i_private || !dirent) {
        return -EINVAL;
    }
    struct ramfs_node *dir_node = (struct ramfs_node *)file->f_inode->i_private;

    off_t idx = 0;
    struct ramfs_node *cur = dir_node->children;
    while (cur && idx < file->f_pos) {
        cur = cur->next_sibling;
        idx++;
    }

    if (!cur) {
        return 0; /* EOF */
    }

    dirent->d_ino = cur->ino;
    dirent->d_type = S_ISDIR(cur->mode) ? DT_DIR : DT_REG;
    strncpy(dirent->d_name, cur->name, VFS_NAME_MAX - 1);
    dirent->d_name[VFS_NAME_MAX - 1] = '\0';

    file->f_pos++;
    return 1;
}

static const struct vfs_inode_operations g_ramfs_dir_iops = {
    .lookup = ramfs_lookup,
    .create = ramfs_create,
    .mkdir = ramfs_mkdir,
    .rmdir = ramfs_rmdir,
    .unlink = ramfs_unlink,
    .stat = NULL,
    .truncate = NULL,
};

static const struct vfs_file_operations g_ramfs_dir_fops = {
    .open = NULL,
    .close = NULL,
    .read = NULL,
    .write = NULL,
    .ioctl = NULL,
    .lseek = NULL,
    .readdir = ramfs_readdir,
    .sync = NULL,
};

/* =========================================================================
 * RamFS Mount / Unmount
 * ========================================================================= */
static int ramfs_mount(struct vfs_superblock *sb, const char *source, unsigned long flags, const void *data)
{
    (void)source;
    (void)flags;
    (void)data;

    struct ramfs_node *root_node = ramfs_alloc_node("/", S_IFDIR | 0755, NULL, sb);
    if (!root_node) {
        return -ENOMEM;
    }

    struct vfs_dentry *root_dentry = vfs_dentry_alloc("/", NULL);
    if (!root_dentry) {
        ramfs_free_node(root_node);
        return -ENOMEM;
    }
    root_dentry->d_inode = root_node->inode;
    sb->s_root = root_dentry;
    sb->s_fs_info = root_node;

    return VFS_OK;
}

static int ramfs_unmount(struct vfs_superblock *sb)
{
    if (sb && sb->s_root) {
        vfs_dentry_put(sb->s_root);
        sb->s_root = NULL;
    }
    return VFS_OK;
}

static struct vfs_filesystem_type g_ramfs_type = {
    .name = "ramfs",
    .mount = ramfs_mount,
    .unmount = ramfs_unmount,
    .next = NULL,
};

int ramfs_init(void)
{
    return vfs_register_fs(&g_ramfs_type);
}
