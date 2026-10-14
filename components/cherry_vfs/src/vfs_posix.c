/*
 * CherryVFS - Virtual File System for FreeRTOS
 *
 * Copyright (c) 2026 ForgeStitch
 * SPDX-License-Identifier: MIT
 *
 * POSIX System Call Wrappers: open, close, read, write, ioctl, lseek, stat, mkdir...
 */

#include "cherry_vfs.h"
#include <string.h>
#include <stdarg.h>

int open(const char *path, int flags, ...)
{
    if (!path) {
        return -EINVAL;
    }

    struct vfs_dentry *dentry = NULL;
    int ret = vfs_path_lookup(path, flags, &dentry);

    if (ret != VFS_OK) {
        /* File doesn't exist; check if O_CREAT is requested */
        if (flags & O_CREAT) {
            struct vfs_dentry *parent = NULL;
            char last_comp[VFS_NAME_MAX];
            ret = vfs_path_lookup_parent(path, &parent, last_comp);
            if (ret != VFS_OK || !parent || !parent->d_inode) {
                return ret ? ret : -ENOENT;
            }
            if (!parent->d_inode->i_ops || !parent->d_inode->i_ops->create) {
                return -ENOSYS;
            }

            mode_t mode = S_IFREG | 0644;
            struct vfs_inode *new_inode = NULL;
            ret = parent->d_inode->i_ops->create(parent->d_inode, last_comp, mode, &new_inode);
            if (ret != VFS_OK || !new_inode) {
                return ret ? ret : -EIO;
            }

            /* Create cached child dentry */
            dentry = vfs_dentry_alloc(last_comp, parent);
            if (!dentry) {
                vfs_inode_put(new_inode);
                return -ENOMEM;
            }
            dentry->d_inode = new_inode;
        } else {
            return ret;
        }
    } else {
        /* File exists; check O_EXCL */
        if ((flags & O_CREAT) && (flags & O_EXCL)) {
            return -EEXIST;
        }
        /* Check if directory is opened with regular open or without O_DIRECTORY */
        if (dentry->d_inode && S_ISDIR(dentry->d_inode->i_mode)) {
            if (!(flags & O_DIRECTORY) && ((flags & O_ACCMODE) != O_RDONLY)) {
                return -EISDIR;
            }
        }
    }

    if (!dentry->d_inode) {
        return -ENOENT;
    }

    /* Allocate Open File Object */
    struct vfs_file *file = (struct vfs_file *)vfs_malloc(sizeof(struct vfs_file));
    if (!file) {
        return -ENOMEM;
    }
    memset(file, 0, sizeof(struct vfs_file));
    file->f_inode = dentry->d_inode;
    file->f_dentry = dentry;
    file->f_flags = (uint32_t)flags;
    file->f_ops = dentry->d_inode->i_fops;
    file->f_pos = 0;
    file->f_ref = 1;

    vfs_inode_ref(dentry->d_inode);
    vfs_dentry_ref(dentry);

    /* Invoke filesystem open hook if provided */
    if (file->f_ops && file->f_ops->open) {
        ret = file->f_ops->open(file);
        if (ret != VFS_OK) {
            vfs_file_put(file);
            return ret;
        }
    }

    /* If O_TRUNC is set on a regular file */
    if ((flags & O_TRUNC) && S_ISREG(dentry->d_inode->i_mode)) {
        if (dentry->d_inode->i_ops && dentry->d_inode->i_ops->truncate) {
            dentry->d_inode->i_ops->truncate(dentry->d_inode, 0);
        }
    }

    /* Allocate File Descriptor */
    int fd = vfs_fd_alloc(file);
    if (fd < 0) {
        vfs_file_put(file);
        return fd;
    }

    return fd;
}

int close(int fd)
{
    struct vfs_file *file = vfs_file_get(fd);
    if (!file) {
        return -EBADF;
    }
    vfs_file_put(file); /* Decrement get ref */
    vfs_fd_free(fd);     /* Release slot & file ref */
    return VFS_OK;
}

ssize_t read(int fd, void *buf, size_t count)
{
    if (!buf) {
        return -EINVAL;
    }
    struct vfs_file *file = vfs_file_get(fd);
    if (!file) {
        return -EBADF;
    }
    if ((file->f_flags & O_ACCMODE) == O_WRONLY) {
        vfs_file_put(file);
        return -EBADF;
    }
    if (!file->f_ops || !file->f_ops->read) {
        vfs_file_put(file);
        return -EINVAL;
    }

    ssize_t ret = file->f_ops->read(file, buf, count);
    vfs_file_put(file);
    return ret;
}

ssize_t write(int fd, const void *buf, size_t count)
{
    if (!buf && count > 0) {
        return -EINVAL;
    }
    struct vfs_file *file = vfs_file_get(fd);
    if (!file) {
        return -EBADF;
    }
    if ((file->f_flags & O_ACCMODE) == O_RDONLY) {
        vfs_file_put(file);
        return -EBADF;
    }
    if (!file->f_ops || !file->f_ops->write) {
        vfs_file_put(file);
        return -EINVAL;
    }

    ssize_t ret = file->f_ops->write(file, buf, count);
    vfs_file_put(file);
    return ret;
}

off_t lseek(int fd, off_t offset, int whence)
{
    struct vfs_file *file = vfs_file_get(fd);
    if (!file) {
        return -EBADF;
    }
    if (!file->f_ops || !file->f_ops->lseek) {
        /* Default generic seek implementation */
        off_t new_pos = 0;
        switch (whence) {
        case SEEK_SET:
            new_pos = offset;
            break;
        case SEEK_CUR:
            new_pos = file->f_pos + offset;
            break;
        case SEEK_END:
            if (!file->f_inode) {
                vfs_file_put(file);
                return -ESPIPE;
            }
            new_pos = file->f_inode->i_size + offset;
            break;
        default:
            vfs_file_put(file);
            return -EINVAL;
        }

        if (new_pos < 0) {
            vfs_file_put(file);
            return -EINVAL;
        }
        file->f_pos = new_pos;
        vfs_file_put(file);
        return new_pos;
    }

    off_t ret = file->f_ops->lseek(file, offset, whence);
    vfs_file_put(file);
    return ret;
}

int ioctl(int fd, unsigned int cmd, ...)
{
    struct vfs_file *file = vfs_file_get(fd);
    if (!file) {
        return -EBADF;
    }
    if (!file->f_ops || !file->f_ops->ioctl) {
        vfs_file_put(file);
        return -ENOTTY;
    }

    va_list ap;
    va_start(ap, cmd);
    unsigned long arg = va_arg(ap, unsigned long);
    va_end(ap);

    int ret = file->f_ops->ioctl(file, cmd, arg);
    vfs_file_put(file);
    return ret;
}

int stat(const char *path, struct stat *buf)
{
    if (!path || !buf) {
        return -EINVAL;
    }
    struct vfs_dentry *dentry = NULL;
    int ret = vfs_path_lookup(path, 0, &dentry);
    if (ret != VFS_OK || !dentry || !dentry->d_inode) {
        return ret ? ret : -ENOENT;
    }

    memset(buf, 0, sizeof(struct stat));
    if (dentry->d_inode->i_ops && dentry->d_inode->i_ops->stat) {
        return dentry->d_inode->i_ops->stat(dentry->d_inode, buf);
    }

    buf->st_ino = dentry->d_inode->i_ino;
    buf->st_mode = dentry->d_inode->i_mode;
    buf->st_size = dentry->d_inode->i_size;
    buf->st_rdev = dentry->d_inode->i_rdev;
    buf->st_nlink = 1;
    buf->st_blksize = 512;
    buf->st_blocks = (uint32_t)((buf->st_size + 511) / 512);
    return VFS_OK;
}

int fstat(int fd, struct stat *buf)
{
    if (!buf) {
        return -EINVAL;
    }
    struct vfs_file *file = vfs_file_get(fd);
    if (!file || !file->f_inode) {
        return -EBADF;
    }

    memset(buf, 0, sizeof(struct stat));
    if (file->f_inode->i_ops && file->f_inode->i_ops->stat) {
        int ret = file->f_inode->i_ops->stat(file->f_inode, buf);
        vfs_file_put(file);
        return ret;
    }

    buf->st_ino = file->f_inode->i_ino;
    buf->st_mode = file->f_inode->i_mode;
    buf->st_size = file->f_inode->i_size;
    buf->st_rdev = file->f_inode->i_rdev;
    buf->st_nlink = 1;
    buf->st_blksize = 512;
    buf->st_blocks = (uint32_t)((buf->st_size + 511) / 512);

    vfs_file_put(file);
    return VFS_OK;
}

int sync(void)
{
    return VFS_OK;
}

int fsync(int fd)
{
    struct vfs_file *file = vfs_file_get(fd);
    if (!file) {
        return -EBADF;
    }
    int ret = VFS_OK;
    if (file->f_ops && file->f_ops->sync) {
        ret = file->f_ops->sync(file);
    }
    vfs_file_put(file);
    return ret;
}

int truncate(const char *path, off_t length)
{
    struct vfs_dentry *dentry = NULL;
    int ret = vfs_path_lookup(path, 0, &dentry);
    if (ret != VFS_OK || !dentry || !dentry->d_inode) {
        return ret ? ret : -ENOENT;
    }
    if (!dentry->d_inode->i_ops || !dentry->d_inode->i_ops->truncate) {
        return -ENOSYS;
    }
    return dentry->d_inode->i_ops->truncate(dentry->d_inode, length);
}

int ftruncate(int fd, off_t length)
{
    struct vfs_file *file = vfs_file_get(fd);
    if (!file || !file->f_inode) {
        return -EBADF;
    }
    if (!file->f_inode->i_ops || !file->f_inode->i_ops->truncate) {
        vfs_file_put(file);
        return -ENOSYS;
    }
    int ret = file->f_inode->i_ops->truncate(file->f_inode, length);
    vfs_file_put(file);
    return ret;
}

int mkdir(const char *path, mode_t mode)
{
    struct vfs_dentry *parent = NULL;
    char last_comp[VFS_NAME_MAX];
    int ret = vfs_path_lookup_parent(path, &parent, last_comp);
    if (ret != VFS_OK || !parent || !parent->d_inode) {
        return ret ? ret : -ENOENT;
    }
    if (!parent->d_inode->i_ops || !parent->d_inode->i_ops->mkdir) {
        return -ENOSYS;
    }
    return parent->d_inode->i_ops->mkdir(parent->d_inode, last_comp, mode);
}

int rmdir(const char *path)
{
    struct vfs_dentry *parent = NULL;
    char last_comp[VFS_NAME_MAX];
    int ret = vfs_path_lookup_parent(path, &parent, last_comp);
    if (ret != VFS_OK || !parent || !parent->d_inode) {
        return ret ? ret : -ENOENT;
    }
    if (!parent->d_inode->i_ops || !parent->d_inode->i_ops->rmdir) {
        return -ENOSYS;
    }
    ret = parent->d_inode->i_ops->rmdir(parent->d_inode, last_comp);
    if (ret == VFS_OK) {
        vfs_dentry_drop(parent, last_comp);
    }
    return ret;
}

int unlink(const char *path)
{
    struct vfs_dentry *parent = NULL;
    char last_comp[VFS_NAME_MAX];
    int ret = vfs_path_lookup_parent(path, &parent, last_comp);
    if (ret != VFS_OK || !parent || !parent->d_inode) {
        return ret ? ret : -ENOENT;
    }
    if (!parent->d_inode->i_ops || !parent->d_inode->i_ops->unlink) {
        return -ENOSYS;
    }
    ret = parent->d_inode->i_ops->unlink(parent->d_inode, last_comp);
    if (ret == VFS_OK) {
        vfs_dentry_drop(parent, last_comp);
    }
    return ret;
}

int rename(const char *oldpath, const char *newpath)
{
    if (!oldpath || !newpath) return -EINVAL;
    if (strcmp(oldpath, newpath) == 0) return 0;

    int old_fd = open(oldpath, O_RDONLY);
    if (old_fd < 0) return old_fd;

    struct stat st;
    if (fstat(old_fd, &st) < 0) {
        close(old_fd);
        return -EIO;
    }

    int new_fd = open(newpath, O_WRONLY | O_CREAT | O_TRUNC, st.st_mode);
    if (new_fd < 0) {
        close(old_fd);
        return new_fd;
    }

    char buf[256];
    ssize_t n;
    while ((n = read(old_fd, buf, sizeof(buf))) > 0) {
        if (write(new_fd, buf, n) != n) {
            close(old_fd);
            close(new_fd);
            unlink(newpath);
            return -EIO;
        }
    }

    close(old_fd);
    close(new_fd);
    unlink(oldpath);
    return 0;
}

/* =========================================================================
 * Directory Traversal
 * ========================================================================= */
DIR *opendir(const char *path)
{
    int fd = open(path, O_RDONLY | O_DIRECTORY);
    if (fd < 0) {
        return NULL;
    }
    DIR *dirp = (DIR *)vfs_malloc(sizeof(DIR));
    if (!dirp) {
        close(fd);
        return NULL;
    }
    memset(dirp, 0, sizeof(DIR));
    dirp->fd = fd;
    return dirp;
}

struct dirent *readdir(DIR *dirp)
{
    if (!dirp || dirp->fd < 0) {
        return NULL;
    }
    struct vfs_file *file = vfs_file_get(dirp->fd);
    if (!file || !file->f_ops || !file->f_ops->readdir) {
        if (file) vfs_file_put(file);
        return NULL;
    }

    memset(&dirp->cur_entry, 0, sizeof(struct dirent));
    int ret = file->f_ops->readdir(file, &dirp->cur_entry);
    vfs_file_put(file);

    if (ret <= 0) {
        return NULL;
    }
    return &dirp->cur_entry;
}

int closedir(DIR *dirp)
{
    if (!dirp) return -EINVAL;
    close(dirp->fd);
    vfs_free(dirp);
    return VFS_OK;
}

/* =========================================================================
 * Mount Call Wrappers
 * ========================================================================= */
int mount(const char *source, const char *target, const char *fstype,
          unsigned long flags, const void *data)
{
    return vfs_mount(source, target, fstype, flags, data);
}

int umount(const char *target)
{
    return vfs_umount(target);
}
