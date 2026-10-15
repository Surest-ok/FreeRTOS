/*
 * CherryFSTools - fdisk Command & Partition Tool
 *
 * Copyright (c) 2026 ForgeStitch
 * SPDX-License-Identifier: MIT
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "cherry_kal.h"
#include "cherry_printk.h"
#include "vfs_dev.h"
#include "fdisk.h"
#include "cherry_fstools.h"

static uint32_t parse_num(const char *s)
{
    uint32_t val = 0;
    while (*s >= '0' && *s <= '9') {
        val = val * 10 + (*s - '0');
        s++;
    }
    return val;
}

static struct vfs_bdev *resolve_bdev(const char *dev_name)
{
    if (!dev_name) return NULL;
    struct vfs_bdev *bdev = vfs_bdev_find(dev_name);
    if (!bdev && dev_name[0] == '/') {
        bdev = vfs_bdev_find(dev_name + 5);
    }
    return bdev;
}

int cherry_fdisk(const char *dev)
{
    struct vfs_bdev *bdev = resolve_bdev(dev);
    if (!bdev) {
        printk("fdisk: device '%s' not found\n", dev ? dev : "(null)");
        return -1;
    }
    return part_dump_table(bdev);
}

int cherry_part_create(const char *dev, int part_no, uint32_t start_lba, uint32_t sectors, uint8_t type)
{
    struct vfs_bdev *bdev = resolve_bdev(dev);
    if (!bdev) {
        printk("fdisk: device '%s' not found\n", dev ? dev : "(null)");
        return -1;
    }
    return part_create(bdev, part_no, start_lba, sectors, type);
}

int cmd_fdisk(int argc, char **argv)
{
    if (argc < 2) {
        printk("usage: fdisk <device>\n");
        printk("       fdisk <device> create <part_no(1..4)> <start_lba> <sectors> [type_hex]\n");
        return -1;
    }

    const char *dev = argv[1];
    struct vfs_bdev *bdev = resolve_bdev(dev);
    if (!bdev) {
        printk("fdisk: block device '%s' not found\n", dev);
        return -1;
    }

    if (argc == 2) {
        return part_dump_table(bdev);
    }

    if (argc >= 6 && strcmp(argv[2], "create") == 0) {
        int part_no = (int)parse_num(argv[3]);
        uint32_t start_lba = parse_num(argv[4]);
        uint32_t sectors = parse_num(argv[5]);
        uint8_t type = (argc >= 7) ? (uint8_t)parse_num(argv[6]) : MBR_TYPE_LINUX;

        return part_create(bdev, part_no, start_lba, sectors, type);
    }

    printk("fdisk: unrecognized option '%s'\n", argv[2]);
    return -1;
}
