/*
 * CherryFSTools - mount & umount Commands
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
#include "cherry_fstools.h"

int fstools_mount(const char *src, const char *target, const char *fstype)
{
    if (!src || !target) return -1;
    mkdir(target, 0755);
    int ret = mount(src, target, fstype ? fstype : "ext4", 0, NULL);
    if (ret != 0) {
        printk("mount: failed to mount '%s' on '%s' (ret=%d)\n", src, target, ret);
    } else {
        printk("mount: '%s' mounted on '%s' type %s\n", src, target, fstype ? fstype : "ext4");
    }
    return ret;
}

int fstools_umount(const char *target)
{
    if (!target) return -1;
    int ret = umount(target);
    if (ret != 0) {
        printk("umount: failed to unmount '%s' (ret=%d)\n", target, ret);
    } else {
        printk("umount: '%s' successfully unmounted\n", target);
    }
    return ret;
}

int cmd_mount(int argc, char **argv)
{
    if (argc < 3) {
        printk("usage: mount <source> <target> [fstype]\n");
        return -1;
    }

    const char *src = argv[1];
    const char *tgt = argv[2];
    const char *fstype = (argc >= 4) ? argv[3] : "ext4";

    return fstools_mount(src, tgt, fstype);
}

int cmd_umount(int argc, char **argv)
{
    if (argc < 2) {
        printk("usage: umount <target>\n");
        return -1;
    }
    return fstools_umount(argv[1]);
}
