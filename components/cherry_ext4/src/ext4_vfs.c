/*
 * CherryExt4 - CherryVFS Virtual Filesystem Bridge & POSIX Inode/File Ops
 *
 * Implements struct vfs_filesystem_type for "ext4", supporting standard
 * VFS mount/umount, directory operations, extent reads, transactional writes.
 *
 * Copyright (c) 2026 ForgeStitch
 * SPDX-License-Identifier: MIT
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "cherry_kal.h"
#include "cherry_printk.h"
#include "cherry_vfs.h"
#include "vfs_core.h"
#include "vfs_inode.h"
#include "vfs_dev.h"
#include "vfs_posix.h"
#include "ext4_types.h"
#include "ext4_jbd2.h"
#include "ext4_fs.h"

static const struct vfs_inode_operations s_ext4_inode_ops;
static const struct vfs_file_operations  s_ext4_file_ops;

/* =========================================================================
 * 1. File Operations
 * ========================================================================= */
static int ext4_vfs_open(struct vfs_file *file)
{
    (void)file;
    return VFS_OK;
}

static int ext4_vfs_close(struct vfs_file *file)
{
    (void)file;
    return VFS_OK;
}

static ssize_t ext4_vfs_read(struct vfs_file *file, void *buf, size_t count)
{
    if (!file || !file->f_inode || !file->f_inode->i_sb) return -1;
    struct ext4_fs *fs = (struct ext4_fs *)file->f_inode->i_sb->s_fs_info;
    if (!fs) return -1;

    struct ext4_inode in;
    if (ext4_read_inode(fs, (uint32_t)file->f_inode->i_ino, &in) != 0) {
        return -5; /* EIO */
    }

    ssize_t ret = ext4_read_file(fs, &in, file->f_pos, buf, count);
    if (ret > 0) {
        file->f_pos += ret;
    }
    return ret;
}

static ssize_t ext4_vfs_write(struct vfs_file *file, const void *buf, size_t count)
{
    if (!file || !file->f_inode || !file->f_inode->i_sb) return -1;
    struct ext4_fs *fs = (struct ext4_fs *)file->f_inode->i_sb->s_fs_info;
    if (!fs) return -1;

    struct ext4_inode in;
    if (ext4_read_inode(fs, (uint32_t)file->f_inode->i_ino, &in) != 0) {
        return -5;
    }

    /* Wrap write in a JBD2 journal transaction */
    struct jbd2_handle *jh = jbd2_trans_start(fs, 4);

    ssize_t ret = ext4_write_file(fs, (uint32_t)file->f_inode->i_ino, &in, file->f_pos, buf, count);
    if (ret > 0) {
        file->f_pos += ret;
        file->f_inode->i_size = in.i_size_lo;
    }

    if (jh) {
        jbd2_trans_commit(jh);
    }

    return ret;
}

static off_t ext4_vfs_lseek(struct vfs_file *file, off_t offset, int whence)
{
    if (!file || !file->f_inode) return -1;

    off_t new_pos = file->f_pos;
    switch (whence) {
    case SEEK_SET: new_pos = offset; break;
    case SEEK_CUR: new_pos += offset; break;
    case SEEK_END: new_pos = file->f_inode->i_size + offset; break;
    default: return -22;
    }

    if (new_pos < 0) return -22;
    file->f_pos = new_pos;
    return new_pos;
}

static int ext4_vfs_readdir(struct vfs_file *file, struct dirent *dirent)
{
    if (!file || !file->f_inode || !file->f_inode->i_sb || !dirent) return -1;
    struct ext4_fs *fs = (struct ext4_fs *)file->f_inode->i_sb->s_fs_info;
    if (!fs) return -1;

    struct ext4_inode dir_in;
    if (ext4_read_inode(fs, (uint32_t)file->f_inode->i_ino, &dir_in) != 0) {
        return -5;
    }

    uint32_t dir_size = dir_in.i_size_lo;
    if ((uint32_t)file->f_pos >= dir_size) {
        return 0; /* EOF */
    }

    uint8_t *block_buf = (uint8_t *)kal_malloc(fs->block_size);
    if (!block_buf) return -12;

    uint32_t lblock = (uint32_t)file->f_pos / fs->block_size;
    uint32_t blk_off = (uint32_t)file->f_pos % fs->block_size;

    uint32_t pblock = 0;
    ext4_bmap(fs, &dir_in, lblock, &pblock, false);
    if (pblock == 0 || ext4_read_block(fs, pblock, block_buf) != 0) {
        kal_free(block_buf);
        return -5;
    }

    struct ext4_dir_entry_2 *de = (struct ext4_dir_entry_2 *)(block_buf + blk_off);
    if (de->rec_len < 8 || blk_off + de->rec_len > fs->block_size) {
        kal_free(block_buf);
        return 0;
    }

    dirent->d_ino = de->inode;
    dirent->d_type = (de->file_type == EXT4_FT_DIR) ? DT_DIR : DT_REG;
    size_t copy_len = de->name_len < sizeof(dirent->d_name) - 1 ? de->name_len : sizeof(dirent->d_name) - 1;
    memcpy(dirent->d_name, de->name, copy_len);
    dirent->d_name[copy_len] = '\0';

    file->f_pos += de->rec_len;
    kal_free(block_buf);
    return 1; /* One entry read */
}

static int ext4_vfs_sync(struct vfs_file *file)
{
    if (!file || !file->f_inode || !file->f_inode->i_sb) return -1;
    struct ext4_fs *fs = (struct ext4_fs *)file->f_inode->i_sb->s_fs_info;
    if (fs && fs->bdev && fs->bdev->ops->sync) {
        return fs->bdev->ops->sync(fs->bdev);
    }
    return 0;
}

static const struct vfs_file_operations s_ext4_file_ops = {
    .open    = ext4_vfs_open,
    .close   = ext4_vfs_close,
    .read    = ext4_vfs_read,
    .write   = ext4_vfs_write,
    .lseek   = ext4_vfs_lseek,
    .readdir = ext4_vfs_readdir,
    .sync    = ext4_vfs_sync,
};

/* =========================================================================
 * 2. Inode Operations
 * ========================================================================= */
static int ext4_vfs_lookup(struct vfs_inode *dir, const char *name, struct vfs_inode **result)
{
    if (!dir || !dir->i_sb || !name || !result) return -1;
    struct ext4_fs *fs = (struct ext4_fs *)dir->i_sb->s_fs_info;
    if (!fs) return -1;

    struct ext4_inode dir_in;
    if (ext4_read_inode(fs, (uint32_t)dir->i_ino, &dir_in) != 0) return -5;

    uint32_t found_ino = 0;
    uint8_t found_type = 0;
    int ret = ext4_dir_lookup(fs, &dir_in, name, &found_ino, &found_type);
    if (ret != 0 || found_ino == 0) {
        return -ENOENT;
    }

    struct ext4_inode child_in;
    if (ext4_read_inode(fs, found_ino, &child_in) != 0) return -5;

    mode_t mode = child_in.i_mode;
    struct vfs_inode *res = vfs_inode_alloc(dir->i_sb, mode);
    if (!res) return -12;

    res->i_ino = found_ino;
    res->i_size = child_in.i_size_lo;
    res->i_ops = &s_ext4_inode_ops;
    res->i_fops = &s_ext4_file_ops;

    *result = res;
    return VFS_OK;
}

static int ext4_vfs_create(struct vfs_inode *dir, const char *name, mode_t mode, struct vfs_inode **result)
{
    if (!dir || !dir->i_sb || !name) return -1;
    struct ext4_fs *fs = (struct ext4_fs *)dir->i_sb->s_fs_info;
    if (!fs) return -1;

    struct ext4_inode dir_in;
    if (ext4_read_inode(fs, (uint32_t)dir->i_ino, &dir_in) != 0) return -5;

    /* Start JBD2 transaction */
    struct jbd2_handle *jh = jbd2_trans_start(fs, 4);

    uint32_t new_ino = 0;
    mode_t fmode = S_IFREG | (mode & 0777);
    int ret = ext4_alloc_inode(fs, fmode, &new_ino);
    if (ret != 0) {
        if (jh) jbd2_trans_commit(jh);
        return ret;
    }

    ret = ext4_dir_add_entry(fs, (uint32_t)dir->i_ino, &dir_in, name, new_ino, EXT4_FT_REG_FILE);
    if (ret != 0) {
        ext4_free_inode(fs, new_ino);
        if (jh) jbd2_trans_commit(jh);
        return ret;
    }

    if (jh) {
        jbd2_trans_commit(jh);
    }

    if (result) {
        struct vfs_inode *res = vfs_inode_alloc(dir->i_sb, fmode);
        if (res) {
            res->i_ino = new_ino;
            res->i_size = 0;
            res->i_ops = &s_ext4_inode_ops;
            res->i_fops = &s_ext4_file_ops;
            *result = res;
        }
    }
    return VFS_OK;
}

static int ext4_vfs_mkdir(struct vfs_inode *dir, const char *name, mode_t mode)
{
    if (!dir || !dir->i_sb || !name) return -1;
    struct ext4_fs *fs = (struct ext4_fs *)dir->i_sb->s_fs_info;
    if (!fs) return -1;

    struct ext4_inode dir_in;
    if (ext4_read_inode(fs, (uint32_t)dir->i_ino, &dir_in) != 0) return -5;

    struct jbd2_handle *jh = jbd2_trans_start(fs, 4);

    uint32_t new_ino = 0;
    mode_t dmode = S_IFDIR | (mode & 0777);
    int ret = ext4_alloc_inode(fs, dmode, &new_ino);
    if (ret != 0) {
        if (jh) jbd2_trans_commit(jh);
        return ret;
    }

    /* Initialize child directory block with "." and ".." */
    struct ext4_inode child_in;
    ext4_read_inode(fs, new_ino, &child_in);

    uint32_t child_pblk = 0;
    ret = ext4_bmap(fs, &child_in, 0, &child_pblk, true);
    if (ret != 0 || child_pblk == 0) {
        ext4_free_inode(fs, new_ino);
        if (jh) jbd2_trans_commit(jh);
        return -28;
    }

    uint8_t *buf = (uint8_t *)kal_malloc(fs->block_size);
    if (buf) {
        memset(buf, 0, fs->block_size);
        struct ext4_dir_entry_2 *dot = (struct ext4_dir_entry_2 *)buf;
        dot->inode = new_ino;
        dot->rec_len = 12;
        dot->name_len = 1;
        dot->file_type = EXT4_FT_DIR;
        dot->name[0] = '.';

        struct ext4_dir_entry_2 *dotdot = (struct ext4_dir_entry_2 *)(buf + 12);
        dotdot->inode = (uint32_t)dir->i_ino;
        dotdot->rec_len = (uint16_t)(fs->block_size - 12);
        dotdot->name_len = 2;
        dotdot->file_type = EXT4_FT_DIR;
        dotdot->name[0] = '.';
        dotdot->name[1] = '.';

        ext4_write_block(fs, child_pblk, buf);
        kal_free(buf);
    }

    child_in.i_size_lo = fs->block_size;
    child_in.i_links_count = 2;
    ext4_write_inode(fs, new_ino, &child_in);

    /* Add entry to parent directory */
    ret = ext4_dir_add_entry(fs, (uint32_t)dir->i_ino, &dir_in, name, new_ino, EXT4_FT_DIR);
    if (ret != 0) {
        ext4_free_inode(fs, new_ino);
        if (jh) jbd2_trans_commit(jh);
        return ret;
    }

    dir_in.i_links_count++;
    ext4_write_inode(fs, (uint32_t)dir->i_ino, &dir_in);

    if (jh) {
        jbd2_trans_commit(jh);
    }
    return VFS_OK;
}

static int ext4_vfs_unlink(struct vfs_inode *dir, const char *name)
{
    if (!dir || !dir->i_sb || !name) return -1;
    struct ext4_fs *fs = (struct ext4_fs *)dir->i_sb->s_fs_info;
    if (!fs) return -1;

    struct ext4_inode dir_in;
    if (ext4_read_inode(fs, (uint32_t)dir->i_ino, &dir_in) != 0) return -5;

    uint32_t child_ino = 0;
    uint8_t child_type = 0;
    if (ext4_dir_lookup(fs, &dir_in, name, &child_ino, &child_type) != 0) {
        return -ENOENT;
    }

    struct jbd2_handle *jh = jbd2_trans_start(fs, 4);

    ext4_dir_remove_entry(fs, (uint32_t)dir->i_ino, &dir_in, name);
    ext4_free_inode(fs, child_ino);

    if (jh) {
        jbd2_trans_commit(jh);
    }
    return VFS_OK;
}

static int ext4_vfs_rmdir(struct vfs_inode *dir, const char *name)
{
    if (!dir || !dir->i_sb || !name) return -1;
    struct ext4_fs *fs = (struct ext4_fs *)dir->i_sb->s_fs_info;
    if (!fs) return -1;

    struct ext4_inode dir_in;
    if (ext4_read_inode(fs, (uint32_t)dir->i_ino, &dir_in) != 0) return -5;

    uint32_t child_ino = 0;
    uint8_t child_type = 0;
    if (ext4_dir_lookup(fs, &dir_in, name, &child_ino, &child_type) != 0) {
        return -ENOENT;
    }

    struct ext4_inode child_in;
    if (ext4_read_inode(fs, child_ino, &child_in) != 0) return -5;

    if (!ext4_dir_is_empty(fs, &child_in)) {
        return -39; /* ENOTEMPTY */
    }

    struct jbd2_handle *jh = jbd2_trans_start(fs, 4);

    ext4_dir_remove_entry(fs, (uint32_t)dir->i_ino, &dir_in, name);
    ext4_free_inode(fs, child_ino);

    if (dir_in.i_links_count > 2) {
        dir_in.i_links_count--;
        ext4_write_inode(fs, (uint32_t)dir->i_ino, &dir_in);
    }

    if (jh) {
        jbd2_trans_commit(jh);
    }
    return VFS_OK;
}

static int ext4_vfs_stat(struct vfs_inode *inode, struct stat *st)
{
    if (!inode || !st) return -1;
    st->st_ino = inode->i_ino;
    st->st_mode = inode->i_mode;
    st->st_size = inode->i_size;
    st->st_nlink = 1;
    return VFS_OK;
}

static int ext4_vfs_truncate(struct vfs_inode *inode, off_t length)
{
    if (!inode || !inode->i_sb) return -1;
    struct ext4_fs *fs = (struct ext4_fs *)inode->i_sb->s_fs_info;
    if (!fs) return -1;

    struct ext4_inode in;
    if (ext4_read_inode(fs, (uint32_t)inode->i_ino, &in) != 0) return -5;

    int ret = ext4_truncate_file(fs, (uint32_t)inode->i_ino, &in, length);
    if (ret == 0) {
        inode->i_size = length;
    }
    return ret;
}

static const struct vfs_inode_operations s_ext4_inode_ops = {
    .lookup   = ext4_vfs_lookup,
    .create   = ext4_vfs_create,
    .mkdir    = ext4_vfs_mkdir,
    .rmdir    = ext4_vfs_rmdir,
    .unlink   = ext4_vfs_unlink,
    .stat     = ext4_vfs_stat,
    .truncate = ext4_vfs_truncate,
};

/* =========================================================================
 * 3. Mount / Unmount Operations
 * ========================================================================= */
static int ext4_vfs_mount(struct vfs_superblock *sb, const char *source, unsigned long flags, const void *data)
{
    (void)flags;
    (void)data;
    if (!sb || !source) return -EINVAL;

    struct vfs_bdev *bdev = vfs_bdev_find(source);
    if (!bdev && source[0] == '/') {
        bdev = vfs_bdev_find(source + 5); /* skip "/dev/" */
    }
    if (!bdev) {
        pr_err("cherry_ext4: mount block device '%s' not found\n", source);
        return -ENODEV;
    }

    /* Read Superblock at offset 1024 */
    uint8_t *sb_buf = (uint8_t *)kal_malloc(1024);
    if (!sb_buf) return -ENOMEM;

    if (bdev->ops->read_blocks(bdev, sb_buf, 2, 2) != 0) {
        kal_free(sb_buf);
        return -EIO;
    }

    struct ext4_super_block *raw_sb = (struct ext4_super_block *)sb_buf;
    if (raw_sb->s_magic != EXT4_SUPER_MAGIC) {
        pr_err("cherry_ext4: invalid magic 0x%04x on '%s'\n", raw_sb->s_magic, source);
        kal_free(sb_buf);
        return -EINVAL;
    }

    struct ext4_fs *fs = (struct ext4_fs *)kal_malloc(sizeof(struct ext4_fs));
    if (!fs) {
        kal_free(sb_buf);
        return -ENOMEM;
    }
    memset(fs, 0, sizeof(struct ext4_fs));

    fs->bdev = bdev;
    memcpy(&fs->sb, raw_sb, sizeof(struct ext4_super_block));
    fs->block_size = 1024 << raw_sb->s_log_block_size;
    fs->sectors_per_block = fs->block_size / 512;
    fs->first_data_block = raw_sb->s_first_data_block;
    fs->groups_count = (raw_sb->s_blocks_count_lo + raw_sb->s_blocks_per_group - 1) / raw_sb->s_blocks_per_group;
    kal_mutex_init(&fs->lock);

    /* Read GDT */
    size_t gdt_size = fs->groups_count * sizeof(struct ext4_group_desc);
    fs->group_descs = (struct ext4_group_desc *)kal_malloc(gdt_size);
    if (!fs->group_descs) {
        kal_free(sb_buf);
        kal_free(fs);
        return -ENOMEM;
    }

    uint32_t gdt_block = (fs->block_size == 1024) ? 2 : 1;
    uint8_t *gdt_buf = (uint8_t *)kal_malloc(fs->block_size);
    if (gdt_buf) {
        ext4_read_block(fs, gdt_block, gdt_buf);
        memcpy(fs->group_descs, gdt_buf, gdt_size);
        kal_free(gdt_buf);
    }

    /* Initialize JBD2 Journal */
    jbd2_init(fs);

    kal_free(sb_buf);
    sb->s_fs_info = fs;

    /* Build Root VFS Inode & Dentry */
    struct ext4_inode root_in;
    if (ext4_read_inode(fs, EXT4_ROOT_INO, &root_in) != 0) {
        kal_free(fs->group_descs);
        kal_free(fs);
        return -EIO;
    }

    struct vfs_inode *root_inode = vfs_inode_alloc(sb, root_in.i_mode);
    if (!root_inode) {
        kal_free(fs->group_descs);
        kal_free(fs);
        return -ENOMEM;
    }

    root_inode->i_ino = EXT4_ROOT_INO;
    root_inode->i_size = root_in.i_size_lo;
    root_inode->i_ops = &s_ext4_inode_ops;
    root_inode->i_fops = &s_ext4_file_ops;

    struct vfs_dentry *root_dentry = vfs_dentry_alloc("/", NULL);
    if (!root_dentry) {
        vfs_inode_put(root_inode);
        kal_free(fs->group_descs);
        kal_free(fs);
        return -ENOMEM;
    }
    root_dentry->d_inode = root_inode;
    sb->s_root = root_dentry;
    fs->is_mounted = true;

    pr_info("cherry_ext4: mounted '%s' on '%s' (vol='%s', blksize=%u)\n",
            source, sb->s_target, fs->sb.s_volume_name, fs->block_size);
    return VFS_OK;
}

static int ext4_vfs_unmount(struct vfs_superblock *sb)
{
    if (!sb || !sb->s_fs_info) return -EINVAL;
    struct ext4_fs *fs = (struct ext4_fs *)sb->s_fs_info;

    ext4_sync_sb_and_gdt(fs);
    fs->is_mounted = false;

    if (fs->group_descs) {
        kal_free(fs->group_descs);
    }
    kal_free(fs);
    sb->s_fs_info = NULL;

    pr_info("cherry_ext4: unmounted '%s'\n", sb->s_target);
    return VFS_OK;
}

static struct vfs_filesystem_type s_ext4_fs_type = {
    .name    = "ext4",
    .mount   = ext4_vfs_mount,
    .unmount = ext4_vfs_unmount,
    .next    = NULL,
};

int ext4_register_vfs(void)
{
    return vfs_register_fs(&s_ext4_fs_type);
}
