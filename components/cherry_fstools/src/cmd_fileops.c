/*
 * CherryFSTools - File Operation Utilities (ls, cat, cp, mv, rm, mkdir, echo, mkfs, fsck)
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
#include "vfs_posix.h"
#include "cherry_ext4.h"
#include "cherry_fstools.h"

/* =========================================================================
 * 1. ls Implementation
 * ========================================================================= */
int fstools_ls(const char *path)
{
    const char *dir_path = (path && strlen(path) > 0) ? path : "/";
    DIR *dir = opendir(dir_path);
    if (!dir) {
        printk("ls: cannot open directory '%s'\n", dir_path);
        return -1;
    }

    printk("Directory %s:\n", dir_path);
    printk("  %-6s  %-8s  %s\n", "Type", "Inode", "Name");
    printk("  --------------------------------------\n");

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        const char *t_str = (entry->d_type == DT_DIR) ? "[DIR] " : "[FILE]";
        printk("  %-6s  %-8lu  %s\n", t_str, (unsigned long)entry->d_ino, entry->d_name);
    }

    closedir(dir);
    return 0;
}

int cmd_ls(int argc, char **argv)
{
    const char *p = (argc >= 2) ? argv[1] : "/";
    return fstools_ls(p);
}

/* =========================================================================
 * 2. cat Implementation
 * ========================================================================= */
int fstools_cat(const char *path)
{
    if (!path) return -1;
    int fd = open(path, O_RDONLY, 0);
    if (fd < 0) {
        printk("cat: cannot open file '%s'\n", path);
        return -1;
    }

    char buf[256];
    ssize_t n;
    while ((n = read(fd, buf, sizeof(buf) - 1)) > 0) {
        buf[n] = '\0';
        printk("%s", buf);
    }
    printk("\n");
    close(fd);
    return 0;
}

int cmd_cat(int argc, char **argv)
{
    if (argc < 2) {
        printk("usage: cat <file>\n");
        return -1;
    }
    return fstools_cat(argv[1]);
}

/* =========================================================================
 * 3. cp & mv Implementation
 * ========================================================================= */
int fstools_cp(const char *src, const char *dst)
{
    if (!src || !dst) return -1;

    int sfd = open(src, O_RDONLY, 0);
    if (sfd < 0) {
        printk("cp: cannot open source '%s'\n", src);
        return -1;
    }

    int dfd = open(dst, O_CREAT | O_WRONLY | O_TRUNC, 0666);
    if (dfd < 0) {
        printk("cp: cannot create destination '%s'\n", dst);
        close(sfd);
        return -1;
    }

    char buf[512];
    ssize_t n;
    while ((n = read(sfd, buf, sizeof(buf))) > 0) {
        if (write(dfd, buf, (size_t)n) != n) {
            printk("cp: write error writing to '%s'\n", dst);
            close(sfd);
            close(dfd);
            return -1;
        }
    }

    close(sfd);
    close(dfd);
    return 0;
}

int cmd_cp(int argc, char **argv)
{
    if (argc < 3) {
        printk("usage: cp <source> <dest>\n");
        return -1;
    }
    return fstools_cp(argv[1], argv[2]);
}

int fstools_mv(const char *src, const char *dst)
{
    if (!src || !dst) return -1;
    int ret = fstools_cp(src, dst);
    if (ret == 0) {
        unlink(src);
    }
    return ret;
}

int cmd_mv(int argc, char **argv)
{
    if (argc < 3) {
        printk("usage: mv <source> <dest>\n");
        return -1;
    }
    return fstools_mv(argv[1], argv[2]);
}

/* =========================================================================
 * 4. rm & mkdir Implementation
 * ========================================================================= */
int fstools_rm(const char *path)
{
    if (!path) return -1;

    /* Try unlink first */
    int ret = unlink(path);
    if (ret != 0) {
        /* If not a file, try rmdir */
        ret = rmdir(path);
    }

    if (ret != 0) {
        printk("rm: failed to remove '%s'\n", path);
    }
    return ret;
}

int cmd_rm(int argc, char **argv)
{
    if (argc < 2) {
        printk("usage: rm <path>\n");
        return -1;
    }
    return fstools_rm(argv[1]);
}

int fstools_mkdir(const char *path)
{
    if (!path) return -1;
    int ret = mkdir(path, 0755);
    if (ret != 0) {
        printk("mkdir: cannot create directory '%s'\n", path);
    }
    return ret;
}

int cmd_mkdir(int argc, char **argv)
{
    if (argc < 2) {
        printk("usage: mkdir <path>\n");
        return -1;
    }
    return fstools_mkdir(argv[1]);
}

/* =========================================================================
 * 5. echo Implementation (with > and >> support)
 * ========================================================================= */
int fstools_echo(const char *str, const char *redir_file, bool append)
{
    if (!str) return 0;

    if (!redir_file) {
        printk("%s\n", str);
        return 0;
    }

    int flags = O_CREAT | O_WRONLY | (append ? O_APPEND : O_TRUNC);
    int fd = open(redir_file, flags, 0666);
    if (fd < 0) {
        printk("echo: failed to open output file '%s'\n", redir_file);
        return -1;
    }

    size_t len = strlen(str);
    write(fd, str, len);
    write(fd, "\n", 1);
    close(fd);
    return 0;
}

int cmd_echo(int argc, char **argv)
{
    if (argc < 2) {
        printk("\n");
        return 0;
    }

    char line_buf[256];
    line_buf[0] = '\0';

    const char *redir_file = NULL;
    bool append = false;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], ">") == 0 && i + 1 < argc) {
            redir_file = argv[i + 1];
            append = false;
            break;
        } else if (strcmp(argv[i], ">>") == 0 && i + 1 < argc) {
            redir_file = argv[i + 1];
            append = true;
            break;
        }

        if (line_buf[0] != '\0') {
            strncat(line_buf, " ", sizeof(line_buf) - strlen(line_buf) - 1);
        }
        strncat(line_buf, argv[i], sizeof(line_buf) - strlen(line_buf) - 1);
    }

    return fstools_echo(line_buf, redir_file, append);
}

/* =========================================================================
 * 6. mkfs & fsck Commands
 * ========================================================================= */
int cmd_mkfs(int argc, char **argv)
{
    if (argc < 2) {
        printk("usage: mkfs <device> [volume_name]\n");
        return -1;
    }
    const char *dev = argv[1];
    const char *vol = (argc >= 3) ? argv[2] : "ext4";
    return ext4_mkfs(dev, vol);
}

int cmd_fsck(int argc, char **argv)
{
    if (argc < 2) {
        printk("usage: fsck <device> [-y]\n");
        return -1;
    }
    const char *dev = argv[1];
    bool repair = (argc >= 3 && strcmp(argv[2], "-y") == 0);
    return ext4_fsck(dev, repair);
}
