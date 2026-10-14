/*
 * CherryVFS - Virtual File System for FreeRTOS
 *
 * Copyright (c) 2026 ForgeStitch
 * SPDX-License-Identifier: MIT
 *
 * DevFS: Virtual Device Filesystem Mounted at /dev
 */

#include "cherry_vfs.h"
#include <string.h>

/* =========================================================================
 * 1. Character Device File Operations
 * ========================================================================= */
static int devfs_cdev_open(struct vfs_file *file)
{
    if (!file || !file->f_inode || !file->f_inode->i_private) {
        return -EINVAL;
    }
    struct vfs_cdev *cdev = (struct vfs_cdev *)file->f_inode->i_private;
    if (cdev->ops && cdev->ops->open) {
        return cdev->ops->open(cdev, (int)file->f_flags);
    }
    return VFS_OK;
}

static int devfs_cdev_close(struct vfs_file *file)
{
    if (!file || !file->f_inode || !file->f_inode->i_private) {
        return -EINVAL;
    }
    struct vfs_cdev *cdev = (struct vfs_cdev *)file->f_inode->i_private;
    if (cdev->ops && cdev->ops->close) {
        return cdev->ops->close(cdev);
    }
    return VFS_OK;
}

static ssize_t devfs_cdev_read(struct vfs_file *file, void *buf, size_t count)
{
    if (!file || !file->f_inode || !file->f_inode->i_private) {
        return -EINVAL;
    }
    struct vfs_cdev *cdev = (struct vfs_cdev *)file->f_inode->i_private;
    if (cdev->ops && cdev->ops->read) {
        return cdev->ops->read(cdev, buf, count);
    }
    return -ENOSYS;
}

static ssize_t devfs_cdev_write(struct vfs_file *file, const void *buf, size_t count)
{
    if (!file || !file->f_inode || !file->f_inode->i_private) {
        return -EINVAL;
    }
    struct vfs_cdev *cdev = (struct vfs_cdev *)file->f_inode->i_private;
    if (cdev->ops && cdev->ops->write) {
        return cdev->ops->write(cdev, buf, count);
    }
    return -ENOSYS;
}

static int devfs_cdev_ioctl(struct vfs_file *file, unsigned int cmd, unsigned long arg)
{
    if (!file || !file->f_inode || !file->f_inode->i_private) {
        return -EINVAL;
    }
    struct vfs_cdev *cdev = (struct vfs_cdev *)file->f_inode->i_private;
    if (cdev->ops && cdev->ops->ioctl) {
        return cdev->ops->ioctl(cdev, cmd, arg);
    }
    return -ENOTTY;
}

static const struct vfs_file_operations g_devfs_cdev_fops = {
    .open = devfs_cdev_open,
    .close = devfs_cdev_close,
    .read = devfs_cdev_read,
    .write = devfs_cdev_write,
    .ioctl = devfs_cdev_ioctl,
    .lseek = NULL,
    .readdir = NULL,
    .sync = NULL,
};

/* =========================================================================
 * 2. Block Device File Operations
 * ========================================================================= */
static int devfs_bdev_open(struct vfs_file *file)
{
    if (!file || !file->f_inode || !file->f_inode->i_private) {
        return -EINVAL;
    }
    struct vfs_bdev *bdev = (struct vfs_bdev *)file->f_inode->i_private;
    if (bdev->ops && bdev->ops->open) {
        return bdev->ops->open(bdev, (int)file->f_flags);
    }
    return VFS_OK;
}

static int devfs_bdev_close(struct vfs_file *file)
{
    if (!file || !file->f_inode || !file->f_inode->i_private) {
        return -EINVAL;
    }
    struct vfs_bdev *bdev = (struct vfs_bdev *)file->f_inode->i_private;
    if (bdev->ops && bdev->ops->close) {
        return bdev->ops->close(bdev);
    }
    return VFS_OK;
}

static ssize_t devfs_bdev_read(struct vfs_file *file, void *buf, size_t count)
{
    if (!file || !file->f_inode || !file->f_inode->i_private) {
        return -EINVAL;
    }
    struct vfs_bdev *bdev = (struct vfs_bdev *)file->f_inode->i_private;
    if (!bdev->ops || !bdev->ops->read_blocks || bdev->block_size == 0) {
        return -ENOSYS;
    }

    uint64_t lba = (uint64_t)(file->f_pos / bdev->block_size);
    size_t num_blocks = count / bdev->block_size;
    if (num_blocks == 0 && count > 0) {
        num_blocks = 1;
    }

    if (lba + num_blocks > bdev->total_blocks) {
        if (lba >= bdev->total_blocks) return 0; /* EOF */
        num_blocks = (size_t)(bdev->total_blocks - lba);
    }

    int ret = bdev->ops->read_blocks(bdev, buf, lba, num_blocks);
    if (ret != 0) {
        return -EIO;
    }

    size_t bytes_read = num_blocks * bdev->block_size;
    file->f_pos += bytes_read;
    return (ssize_t)bytes_read;
}

static ssize_t devfs_bdev_write(struct vfs_file *file, const void *buf, size_t count)
{
    if (!file || !file->f_inode || !file->f_inode->i_private) {
        return -EINVAL;
    }
    struct vfs_bdev *bdev = (struct vfs_bdev *)file->f_inode->i_private;
    if (!bdev->ops || !bdev->ops->write_blocks || bdev->block_size == 0) {
        return -ENOSYS;
    }

    uint64_t lba = (uint64_t)(file->f_pos / bdev->block_size);
    size_t num_blocks = count / bdev->block_size;
    if (num_blocks == 0 && count > 0) {
        num_blocks = 1;
    }

    if (lba + num_blocks > bdev->total_blocks) {
        if (lba >= bdev->total_blocks) return -ENOSPC;
        num_blocks = (size_t)(bdev->total_blocks - lba);
    }

    int ret = bdev->ops->write_blocks(bdev, buf, lba, num_blocks);
    if (ret != 0) {
        return -EIO;
    }

    size_t bytes_written = num_blocks * bdev->block_size;
    file->f_pos += bytes_written;
    return (ssize_t)bytes_written;
}

static int devfs_bdev_ioctl(struct vfs_file *file, unsigned int cmd, unsigned long arg)
{
    if (!file || !file->f_inode || !file->f_inode->i_private) {
        return -EINVAL;
    }
    struct vfs_bdev *bdev = (struct vfs_bdev *)file->f_inode->i_private;

    switch (cmd) {
    case BLOCK_IOCTL_GET_SECTOR_SIZE:
    case BLOCK_IOCTL_GET_BLOCK_SIZE:
        if (arg) {
            *(uint32_t *)arg = bdev->block_size;
            return VFS_OK;
        }
        return -EINVAL;
    case BLOCK_IOCTL_GET_SECTOR_COUNT:
        if (arg) {
            *(uint64_t *)arg = bdev->total_blocks;
            return VFS_OK;
        }
        return -EINVAL;
    case BLOCK_IOCTL_SYNC:
        if (bdev->ops && bdev->ops->sync) {
            return bdev->ops->sync(bdev);
        }
        return VFS_OK;
    default:
        if (bdev->ops && bdev->ops->ioctl) {
            return bdev->ops->ioctl(bdev, cmd, arg);
        }
        return -ENOTTY;
    }
}

static off_t devfs_bdev_lseek(struct vfs_file *file, off_t offset, int whence)
{
    if (!file || !file->f_inode || !file->f_inode->i_private) {
        return -EINVAL;
    }
    struct vfs_bdev *bdev = (struct vfs_bdev *)file->f_inode->i_private;
    off_t total_size = (off_t)(bdev->block_size * bdev->total_blocks);
    off_t new_pos = 0;

    switch (whence) {
    case SEEK_SET:
        new_pos = offset;
        break;
    case SEEK_CUR:
        new_pos = file->f_pos + offset;
        break;
    case SEEK_END:
        new_pos = total_size + offset;
        break;
    default:
        return -EINVAL;
    }

    if (new_pos < 0 || new_pos > total_size) {
        return -EINVAL;
    }

    file->f_pos = new_pos;
    return new_pos;
}

static const struct vfs_file_operations g_devfs_bdev_fops = {
    .open = devfs_bdev_open,
    .close = devfs_bdev_close,
    .read = devfs_bdev_read,
    .write = devfs_bdev_write,
    .ioctl = devfs_bdev_ioctl,
    .lseek = devfs_bdev_lseek,
    .readdir = NULL,
    .sync = NULL,
};

/* =========================================================================
 * 3. DevFS Directory Operations (lookup, readdir)
 * ========================================================================= */
static int devfs_lookup(struct vfs_inode *dir, const char *name, struct vfs_inode **result)
{
    (void)dir;
    if (!name || !result) {
        return -EINVAL;
    }

    struct vfs_device *dev = vfs_device_find(name);
    if (!dev) {
        return -ENOENT;
    }

    mode_t mode = 0;
    const struct vfs_file_operations *fops = NULL;

    switch (dev->type) {
    case VFS_DEV_TYPE_CHAR:
        mode = S_IFCHR | 0666;
        fops = &g_devfs_cdev_fops;
        break;
    case VFS_DEV_TYPE_BLOCK:
        mode = S_IFBLK | 0666;
        fops = &g_devfs_bdev_fops;
        break;
    case VFS_DEV_TYPE_SOCKET:
        mode = S_IFSOCK | 0666;
        fops = NULL; /* Socket ops managed via dev_socket */
        break;
    default:
        return -ENODEV;
    }

    struct vfs_inode *inode = vfs_inode_alloc(dir->i_sb, mode);
    if (!inode) {
        return -ENOMEM;
    }
    inode->i_fops = fops;
    inode->i_private = dev;
    inode->i_rdev = dev->dev_id;

    if (dev->type == VFS_DEV_TYPE_BLOCK) {
        struct vfs_bdev *bdev = (struct vfs_bdev *)dev;
        inode->i_size = (off_t)(bdev->block_size * bdev->total_blocks);
    }

    *result = inode;
    return VFS_OK;
}

struct devfs_dir_ctx {
    off_t target_idx;
    off_t cur_idx;
    struct dirent *out_dirent;
    int found;
};

static int devfs_iter_cb(struct vfs_device *dev, void *data)
{
    struct devfs_dir_ctx *ctx = (struct devfs_dir_ctx *)data;
    if (ctx->cur_idx == ctx->target_idx) {
        ctx->out_dirent->d_ino = (ino_t)dev->dev_id;
        if (dev->type == VFS_DEV_TYPE_CHAR) {
            ctx->out_dirent->d_type = DT_CHR;
        } else if (dev->type == VFS_DEV_TYPE_BLOCK) {
            ctx->out_dirent->d_type = DT_BLK;
        } else if (dev->type == VFS_DEV_TYPE_SOCKET) {
            ctx->out_dirent->d_type = DT_SOCK;
        } else {
            ctx->out_dirent->d_type = DT_UNKNOWN;
        }
        strncpy(ctx->out_dirent->d_name, dev->name, VFS_NAME_MAX - 1);
        ctx->out_dirent->d_name[VFS_NAME_MAX - 1] = '\0';
        ctx->found = 1;
        return 1; /* Stop iteration */
    }
    ctx->cur_idx++;
    return 0;
}

static int devfs_readdir(struct vfs_file *file, struct dirent *dirent)
{
    if (!file || !dirent) return -EINVAL;

    struct devfs_dir_ctx ctx;
    ctx.target_idx = file->f_pos;
    ctx.cur_idx = 0;
    ctx.out_dirent = dirent;
    ctx.found = 0;

    vfs_device_for_each(&ctx, devfs_iter_cb);

    if (ctx.found) {
        file->f_pos++;
        return 1; /* 1 entry returned */
    }
    return 0; /* End of directory */
}

static const struct vfs_inode_operations g_devfs_dir_iops = {
    .lookup = devfs_lookup,
    .create = NULL,
    .mkdir = NULL,
    .rmdir = NULL,
    .unlink = NULL,
    .stat = NULL,
    .truncate = NULL,
};

static const struct vfs_file_operations g_devfs_dir_fops = {
    .open = NULL,
    .close = NULL,
    .read = NULL,
    .write = NULL,
    .ioctl = NULL,
    .lseek = NULL,
    .readdir = devfs_readdir,
    .sync = NULL,
};

/* =========================================================================
 * 4. DevFS Mount / Unmount
 * ========================================================================= */
static int devfs_mount(struct vfs_superblock *sb, const char *source, unsigned long flags, const void *data)
{
    (void)source;
    (void)flags;
    (void)data;

    struct vfs_inode *root_inode = vfs_inode_alloc(sb, S_IFDIR | 0755);
    if (!root_inode) {
        return -ENOMEM;
    }
    root_inode->i_ops = &g_devfs_dir_iops;
    root_inode->i_fops = &g_devfs_dir_fops;

    struct vfs_dentry *root_dentry = vfs_dentry_alloc("dev", NULL);
    if (!root_dentry) {
        vfs_inode_put(root_inode);
        return -ENOMEM;
    }
    root_dentry->d_inode = root_inode;
    sb->s_root = root_dentry;

    return VFS_OK;
}

static int devfs_unmount(struct vfs_superblock *sb)
{
    if (sb && sb->s_root) {
        vfs_dentry_put(sb->s_root);
        sb->s_root = NULL;
    }
    return VFS_OK;
}

static struct vfs_filesystem_type g_devfs_type = {
    .name = "devfs",
    .mount = devfs_mount,
    .unmount = devfs_unmount,
    .next = NULL,
};

int devfs_init(void)
{
    return vfs_register_fs(&g_devfs_type);
}
