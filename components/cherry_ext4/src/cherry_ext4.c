/*
 * CherryExt4 - Subsystem Initializer & Shell Registration
 *
 * Copyright (c) 2026 ForgeStitch
 * SPDX-License-Identifier: MIT
 */

#include <stdint.h>
#include <stdbool.h>
#include "cherry_ext4.h"
#include "cherry_shell.h"
#include "cherry_printk.h"

extern int ext4_register_vfs(void);

int cherry_ext4_init(void)
{
    pr_info("cherry_ext4: initializing ext4 filesystem & jbd2 journaling engine...\n");

    int ret = ext4_register_vfs();
    if (ret != 0) {
        pr_err("cherry_ext4: failed to register 'ext4' filesystem to cherry_vfs (ret=%d)\n", ret);
        return ret;
    }

    /* Register C functions as commands in CherryShell */
    cherry_shell_register_symbol("mkfs_ext4", (uintptr_t)ext4_mkfs, CHERRY_SHELL_SYM_FUNC,
                                 "format block device with ext4: mkfs_ext4(dev, vol_name)");
    cherry_shell_register_symbol("fsck_ext4", (uintptr_t)ext4_fsck, CHERRY_SHELL_SYM_FUNC,
                                 "check and repair ext4 filesystem: fsck_ext4(dev, repair)");

    pr_info("cherry_ext4: subsystem initialized successfully\n");
    return 0;
}
