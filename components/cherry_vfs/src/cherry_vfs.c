/*
 * CherryVFS - Virtual File System for FreeRTOS
 *
 * Copyright (c) 2026 ForgeStitch
 * SPDX-License-Identifier: MIT
 *
 * CherryVFS Master Initialization
 */

#include "cherry_vfs.h"

extern int ramfs_init(void);
extern int devfs_init(void);
extern int dev_null_zero_init(void);
extern int dev_ramdisk_init(void);
extern int dev_socket_init(void);
extern int dev_pl011_platform_init(void);


int cherry_vfs_init(void)
{
    int ret;

    /* 1. Initialize core VFS structures and synchronization */
    ret = vfs_init();
    if (ret != VFS_OK) return ret;

    /* 2. Initialize Platform Bus subsystem */
    ret = platform_bus_init();
    if (ret != VFS_OK) return ret;

    /* 3. Register built-in filesystems */
    ret = ramfs_init();
    if (ret != VFS_OK) return ret;

    ret = devfs_init();
    if (ret != VFS_OK) return ret;

    /* 4. Mount Root Filesystem (ramfs) at "/" */
    ret = vfs_mount(NULL, "/", "ramfs", 0, NULL);
    if (ret != VFS_OK) return ret;

    /* 5. Create /dev directory and mount DevFS at "/dev" */
    ret = mkdir("/dev", 0755);
    if (ret != VFS_OK && ret != -EEXIST) return ret;

    ret = vfs_mount(NULL, "/dev", "devfs", 0, NULL);
    if (ret != VFS_OK) return ret;

    /* 6. Register standard pseudo and core devices */
    ret = dev_null_zero_init();
    if (ret != VFS_OK) return ret;

    ret = dev_ramdisk_init();
    if (ret != VFS_OK) return ret;

    ret = dev_socket_init();
    if (ret != VFS_OK) return ret;

    /* 7. Register and probe hardware platform drivers (PL011 UART) */
    ret = dev_pl011_platform_init();
    if (ret != VFS_OK) return ret;

    return VFS_OK;
}
