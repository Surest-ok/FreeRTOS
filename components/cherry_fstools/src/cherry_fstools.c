/*
 * CherryFSTools - Subsystem Initializer & Shell Registration
 *
 * Copyright (c) 2026 ForgeStitch
 * SPDX-License-Identifier: MIT
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "cherry_fstools.h"
#include "fdisk.h"
#include "vfs_dev.h"
#include "cherry_shell.h"
#include "cherry_printk.h"

int cmd_fs(int argc, char **argv)
{
    if (argc < 2) {
        printk("Usage: fs <subcommand> [args...]\n");
        printk("CherryFS Storage & Filesystem Admin Utility:\n");
        printk("  fs fdisk <dev>               - Manage MBR partition table\n");
        printk("  fs mount <dev> <target> [fs] - Mount filesystem on target\n");
        printk("  fs umount <target>           - Unmount filesystem\n");
        printk("  fs mkfs <dev> [vol_name]     - Format partition (ext4)\n");
        printk("  fs fsck <dev> [-y]           - Check/repair filesystem\n");
        printk("  fs ls [path]                 - List directory contents\n");
        printk("  fs cat <file>                - Display file contents\n");
        printk("  fs cp <src> <dst>            - Copy file\n");
        printk("  fs mv <src> <dst>            - Move/rename file\n");
        printk("  fs rm <path>                 - Remove file or directory\n");
        printk("  fs mkdir <path>              - Create directory\n");
        printk("  fs echo <text> [> >> file]   - Write text to file\n");
        return -1;
    }

    const char *sub = argv[1];
    char **sub_argv = &argv[1];
    int sub_argc = argc - 1;

    if (strcmp(sub, "fdisk") == 0) return cmd_fdisk(sub_argc, sub_argv);
    if (strcmp(sub, "mount") == 0) return cmd_mount(sub_argc, sub_argv);
    if (strcmp(sub, "umount") == 0) return cmd_umount(sub_argc, sub_argv);
    if (strcmp(sub, "mkfs") == 0) return cmd_mkfs(sub_argc, sub_argv);
    if (strcmp(sub, "fsck") == 0) return cmd_fsck(sub_argc, sub_argv);
    if (strcmp(sub, "ls") == 0) return cmd_ls(sub_argc, sub_argv);
    if (strcmp(sub, "cat") == 0) return cmd_cat(sub_argc, sub_argv);
    if (strcmp(sub, "cp") == 0) return cmd_cp(sub_argc, sub_argv);
    if (strcmp(sub, "mv") == 0) return cmd_mv(sub_argc, sub_argv);
    if (strcmp(sub, "rm") == 0) return cmd_rm(sub_argc, sub_argv);
    if (strcmp(sub, "mkdir") == 0 || strcmp(sub, "mkdior") == 0) return cmd_mkdir(sub_argc, sub_argv);
    if (strcmp(sub, "echo") == 0) return cmd_echo(sub_argc, sub_argv);

    printk("fs: unknown subcommand '%s' (type 'fs' for list)\n", sub);
    return -1;
}

int cmd_fstool(int argc, char **argv)
{
    return cmd_fs(argc, argv);
}

int cherry_fstools_init(void)
{
    pr_info("cherry_fstools: initializing partition tools & file utility suite...\n");

    /* Scan existing block devices (e.g. mmcblk0) for partition tables */
    struct vfs_bdev *sd = vfs_bdev_find("mmcblk0");
    if (sd) {
        part_scan_and_register(sd);
    }

    /* 1. Register Primary Root Leading CLI Command: 'fs' */
    cherry_shell_register_symbol("fs",      (uintptr_t)cmd_fs,      CHERRY_SHELL_SYM_CMD, "filesystem admin tool: fs <fdisk|mount|umount|mkfs|fsck|ls|cat|cp|mv|rm|mkdir|echo>");
    cherry_shell_register_symbol("fstool",  (uintptr_t)cmd_fs,      CHERRY_SHELL_SYM_CMD, "alias to fs");
    cherry_shell_register_symbol("fstools", (uintptr_t)cmd_fs,      CHERRY_SHELL_SYM_CMD, "alias to fs");

    /* 2. Register Direct CLI Commands with 'cherry_' prefix */
    cherry_shell_register_symbol("cherry_fdisk",   (uintptr_t)cmd_fdisk,   CHERRY_SHELL_SYM_CMD, "disk partition manager: cherry_fdisk <dev>");
    cherry_shell_register_symbol("cherry_mount",   (uintptr_t)cmd_mount,   CHERRY_SHELL_SYM_CMD, "mount filesystem: cherry_mount <dev> <target> [type]");
    cherry_shell_register_symbol("cherry_umount",  (uintptr_t)cmd_umount,  CHERRY_SHELL_SYM_CMD, "unmount filesystem: cherry_umount <target>");
    cherry_shell_register_symbol("cherry_ls",      (uintptr_t)cmd_ls,      CHERRY_SHELL_SYM_CMD, "list directory contents: cherry_ls [path]");
    cherry_shell_register_symbol("cherry_cat",     (uintptr_t)cmd_cat,     CHERRY_SHELL_SYM_CMD, "concatenate and display file: cherry_cat <file>");
    cherry_shell_register_symbol("cherry_cp",      (uintptr_t)cmd_cp,      CHERRY_SHELL_SYM_CMD, "copy file: cherry_cp <src> <dst>");
    cherry_shell_register_symbol("cherry_mv",      (uintptr_t)cmd_mv,      CHERRY_SHELL_SYM_CMD, "move/rename file: cherry_mv <src> <dst>");
    cherry_shell_register_symbol("cherry_rm",      (uintptr_t)cmd_rm,      CHERRY_SHELL_SYM_CMD, "remove file/directory: cherry_rm <path>");
    cherry_shell_register_symbol("cherry_mkdir",   (uintptr_t)cmd_mkdir,   CHERRY_SHELL_SYM_CMD, "create directory: cherry_mkdir <path>");
    cherry_shell_register_symbol("cherry_mkdior",  (uintptr_t)cmd_mkdir,   CHERRY_SHELL_SYM_CMD, "create directory (alias): cherry_mkdior <path>");
    cherry_shell_register_symbol("cherry_echo",    (uintptr_t)cmd_echo,    CHERRY_SHELL_SYM_CMD, "write text or redirect to file: cherry_echo <text> [> >> file]");
    cherry_shell_register_symbol("cherry_mkfs",    (uintptr_t)cmd_mkfs,    CHERRY_SHELL_SYM_CMD, "format filesystem: cherry_mkfs <dev> [vol_name]");
    cherry_shell_register_symbol("cherry_fsck",    (uintptr_t)cmd_fsck,    CHERRY_SHELL_SYM_CMD, "check filesystem: cherry_fsck <dev> [-y]");

    /* 3. Register Unprefixed Aliases for Compatibility */
    cherry_shell_register_symbol("fdisk",   (uintptr_t)cmd_fdisk,   CHERRY_SHELL_SYM_CMD, "alias to cherry_fdisk");
    cherry_shell_register_symbol("mount",   (uintptr_t)cmd_mount,   CHERRY_SHELL_SYM_CMD, "alias to cherry_mount");
    cherry_shell_register_symbol("umount",  (uintptr_t)cmd_umount,  CHERRY_SHELL_SYM_CMD, "alias to cherry_umount");
    cherry_shell_register_symbol("mkfs",    (uintptr_t)cmd_mkfs,    CHERRY_SHELL_SYM_CMD, "alias to cherry_mkfs");
    cherry_shell_register_symbol("fsck",    (uintptr_t)cmd_fsck,    CHERRY_SHELL_SYM_CMD, "alias to cherry_fsck");
    cherry_shell_register_symbol("mkdior",  (uintptr_t)cmd_mkdir,   CHERRY_SHELL_SYM_CMD, "alias to cherry_mkdir");

    /* 4. Register C Functions with 'cherry_' prefix */
    cherry_shell_register_symbol("cherry_fdisk",       (uintptr_t)cherry_fdisk,       CHERRY_SHELL_SYM_FUNC, "fdisk(dev)");
    cherry_shell_register_symbol("cherry_mount",       (uintptr_t)cherry_mount,       CHERRY_SHELL_SYM_FUNC, "mount(src, tgt, type)");
    cherry_shell_register_symbol("cherry_umount",      (uintptr_t)cherry_umount,      CHERRY_SHELL_SYM_FUNC, "umount(tgt)");
    cherry_shell_register_symbol("cherry_ls",          (uintptr_t)cherry_ls,          CHERRY_SHELL_SYM_FUNC, "ls(path)");
    cherry_shell_register_symbol("cherry_cat",         (uintptr_t)cherry_cat,         CHERRY_SHELL_SYM_FUNC, "cat(file)");
    cherry_shell_register_symbol("cherry_cp",          (uintptr_t)cherry_cp,          CHERRY_SHELL_SYM_FUNC, "cp(src, dst)");
    cherry_shell_register_symbol("cherry_mv",          (uintptr_t)cherry_mv,          CHERRY_SHELL_SYM_FUNC, "mv(src, dst)");
    cherry_shell_register_symbol("cherry_rm",          (uintptr_t)cherry_rm,          CHERRY_SHELL_SYM_FUNC, "rm(path)");
    cherry_shell_register_symbol("cherry_mkdir",       (uintptr_t)cherry_mkdir,       CHERRY_SHELL_SYM_FUNC, "mkdir(path)");
    cherry_shell_register_symbol("cherry_echo",        (uintptr_t)cherry_echo,        CHERRY_SHELL_SYM_FUNC, "echo(str, file, append)");
    cherry_shell_register_symbol("cherry_part_create", (uintptr_t)cherry_part_create, CHERRY_SHELL_SYM_FUNC, "part_create(dev, no, lba, cnt, type)");

    /* Backward compatibility C symbols */
    cherry_shell_register_symbol("fstools_fdisk",       (uintptr_t)cherry_fdisk,       CHERRY_SHELL_SYM_FUNC, "alias to cherry_fdisk");
    cherry_shell_register_symbol("fstools_mount",       (uintptr_t)cherry_mount,       CHERRY_SHELL_SYM_FUNC, "alias to cherry_mount");
    cherry_shell_register_symbol("fstools_umount",      (uintptr_t)cherry_umount,      CHERRY_SHELL_SYM_FUNC, "alias to cherry_umount");
    cherry_shell_register_symbol("fstools_ls",          (uintptr_t)cherry_ls,          CHERRY_SHELL_SYM_FUNC, "alias to cherry_ls");
    cherry_shell_register_symbol("fstools_cat",         (uintptr_t)cherry_cat,         CHERRY_SHELL_SYM_FUNC, "alias to cherry_cat");
    cherry_shell_register_symbol("fstools_cp",          (uintptr_t)cherry_cp,          CHERRY_SHELL_SYM_FUNC, "alias to cherry_cp");
    cherry_shell_register_symbol("fstools_mv",          (uintptr_t)cherry_mv,          CHERRY_SHELL_SYM_FUNC, "alias to cherry_mv");
    cherry_shell_register_symbol("fstools_rm",          (uintptr_t)cherry_rm,          CHERRY_SHELL_SYM_FUNC, "alias to cherry_rm");
    cherry_shell_register_symbol("fstools_mkdir",       (uintptr_t)cherry_mkdir,       CHERRY_SHELL_SYM_FUNC, "alias to cherry_mkdir");
    cherry_shell_register_symbol("fstools_echo",        (uintptr_t)cherry_echo,        CHERRY_SHELL_SYM_FUNC, "alias to cherry_echo");
    cherry_shell_register_symbol("fstools_part_create", (uintptr_t)cherry_part_create, CHERRY_SHELL_SYM_FUNC, "alias to cherry_part_create");

    pr_info("cherry_fstools: subsystem initialized successfully\n");
    return 0;
}
