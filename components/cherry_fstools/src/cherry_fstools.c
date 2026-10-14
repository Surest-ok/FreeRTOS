/*
 * CherryFSTools - Subsystem Initializer & Shell Registration
 *
 * Copyright (c) 2026 ForgeStitch
 * SPDX-License-Identifier: MIT
 */

#include <stdint.h>
#include <stdbool.h>
#include "cherry_fstools.h"
#include "fdisk.h"
#include "vfs_dev.h"
#include "cherry_shell.h"
#include "cherry_printk.h"

int cherry_fstools_init(void)
{
    pr_info("cherry_fstools: initializing partition tools & file utility suite...\n");

    /* Scan existing block devices (e.g. mmcblk0) for partition tables */
    struct vfs_bdev *sd = vfs_bdev_find("mmcblk0");
    if (sd) {
        part_scan_and_register(sd);
    }

    /* Register CLI Commands */
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
