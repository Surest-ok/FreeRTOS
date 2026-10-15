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

int cmd_fstool(int argc, char **argv)
{
    if (argc < 2) {
        printk("Usage: fstool <subcommand> [args...]\n");
        printk("CherryFS Storage & Filesystem Admin Utility:\n");
        printk("  fdisk <dev>               - Manage MBR partition table\n");
        printk("  mount <dev> <target> [fs] - Mount filesystem on target\n");
        printk("  umount <target>           - Unmount filesystem\n");
        printk("  mkfs <dev> [vol_name]     - Format partition (ext4)\n");
        printk("  fsck <dev> [-y]           - Check/repair filesystem\n");
        printk("  ls [path]                 - List directory contents\n");
        printk("  cat <file>                - Display file contents\n");
        printk("  cp <src> <dst>            - Copy file\n");
        printk("  mv <src> <dst>            - Move/rename file\n");
        printk("  rm <path>                 - Remove file or directory\n");
        printk("  mkdir <path>              - Create directory\n");
        printk("  echo <text> [> >> file]   - Write text to file\n");
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

    printk("fstool: unknown subcommand '%s' (type 'fstool' for list)\n", sub);
    return -1;
}

int cherry_fstools_init(void)
{
    pr_info("cherry_fstools: initializing partition tools & file utility suite...\n");

    /* Scan existing block devices (e.g. mmcblk0) for partition tables */
    struct vfs_bdev *sd = vfs_bdev_find("mmcblk0");
    if (sd) {
        part_scan_and_register(sd);
    }

    /* Register Primary Root CLI Command */
    cherry_shell_register_symbol("fstool",  (uintptr_t)cmd_fstool,  CHERRY_SHELL_SYM_CMD, "filesystem admin tool: fstool <fdisk|mount|umount|mkfs|fsck|ls|cat|cp|mv|rm|mkdir|echo>");
    cherry_shell_register_symbol("fstools", (uintptr_t)cmd_fstool,  CHERRY_SHELL_SYM_CMD, "alias to fstool");

    /* Register CLI Subcommands & Direct Callbacks */
    cherry_shell_register_symbol("fdisk",   (uintptr_t)cmd_fdisk,   CHERRY_SHELL_SYM_CMD, "disk partition manager: fdisk <dev>");
    cherry_shell_register_symbol("mount",   (uintptr_t)cmd_mount,   CHERRY_SHELL_SYM_CMD, "mount filesystem: mount <dev> <target> [type]");
    cherry_shell_register_symbol("umount",  (uintptr_t)cmd_umount,  CHERRY_SHELL_SYM_CMD, "unmount filesystem: umount <target>");
    cherry_shell_register_symbol("ls",      (uintptr_t)cmd_ls,      CHERRY_SHELL_SYM_CMD, "list directory contents: ls [path]");
    cherry_shell_register_symbol("cat",     (uintptr_t)cmd_cat,     CHERRY_SHELL_SYM_CMD, "concatenate and display file: cat <file>");
    cherry_shell_register_symbol("cp",      (uintptr_t)cmd_cp,      CHERRY_SHELL_SYM_CMD, "copy file: cp <src> <dst>");
    cherry_shell_register_symbol("mv",      (uintptr_t)cmd_mv,      CHERRY_SHELL_SYM_CMD, "move/rename file: mv <src> <dst>");
    cherry_shell_register_symbol("rm",      (uintptr_t)cmd_rm,      CHERRY_SHELL_SYM_CMD, "remove file/directory: rm <path>");
    cherry_shell_register_symbol("mkdir",   (uintptr_t)cmd_mkdir,   CHERRY_SHELL_SYM_CMD, "create directory: mkdir <path>");
    cherry_shell_register_symbol("mkdior",  (uintptr_t)cmd_mkdir,   CHERRY_SHELL_SYM_CMD, "create directory (alias): mkdior <path>");
    cherry_shell_register_symbol("echo",    (uintptr_t)cmd_echo,    CHERRY_SHELL_SYM_CMD, "write text or redirect to file: echo <text> [> >> file]");
    cherry_shell_register_symbol("mkfs",    (uintptr_t)cmd_mkfs,    CHERRY_SHELL_SYM_CMD, "format filesystem: mkfs <dev> [vol_name]");
    cherry_shell_register_symbol("fsck",    (uintptr_t)cmd_fsck,    CHERRY_SHELL_SYM_CMD, "check filesystem: fsck <dev> [-y]");

    /* Register C Functions */
    cherry_shell_register_symbol("fstools_fdisk",       (uintptr_t)fstools_fdisk,       CHERRY_SHELL_SYM_FUNC, "fdisk(dev)");
    cherry_shell_register_symbol("fstools_mount",       (uintptr_t)fstools_mount,       CHERRY_SHELL_SYM_FUNC, "mount(src, tgt, type)");
    cherry_shell_register_symbol("fstools_umount",      (uintptr_t)fstools_umount,      CHERRY_SHELL_SYM_FUNC, "umount(tgt)");
    cherry_shell_register_symbol("fstools_ls",          (uintptr_t)fstools_ls,          CHERRY_SHELL_SYM_FUNC, "ls(path)");
    cherry_shell_register_symbol("fstools_cat",         (uintptr_t)fstools_cat,         CHERRY_SHELL_SYM_FUNC, "cat(file)");
    cherry_shell_register_symbol("fstools_cp",          (uintptr_t)fstools_cp,          CHERRY_SHELL_SYM_FUNC, "cp(src, dst)");
    cherry_shell_register_symbol("fstools_mv",          (uintptr_t)fstools_mv,          CHERRY_SHELL_SYM_FUNC, "mv(src, dst)");
    cherry_shell_register_symbol("fstools_rm",          (uintptr_t)fstools_rm,          CHERRY_SHELL_SYM_FUNC, "rm(path)");
    cherry_shell_register_symbol("fstools_mkdir",       (uintptr_t)fstools_mkdir,       CHERRY_SHELL_SYM_FUNC, "mkdir(path)");
    cherry_shell_register_symbol("fstools_echo",        (uintptr_t)fstools_echo,        CHERRY_SHELL_SYM_FUNC, "echo(str, file, append)");
    cherry_shell_register_symbol("fstools_part_create", (uintptr_t)fstools_part_create, CHERRY_SHELL_SYM_FUNC, "part_create(dev, no, lba, cnt, type)");

    pr_info("cherry_fstools: subsystem initialized successfully\n");
    return 0;
}
