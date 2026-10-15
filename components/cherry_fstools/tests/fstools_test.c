/*
 * CherryFSTools - Acceptance & Verification Test Suite
 *
 * Copyright (c) 2026 ForgeStitch
 * SPDX-License-Identifier: MIT
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "cherry_fstools.h"
#include "fdisk.h"
#include "cherry_ext4.h"
#include "vfs_dev.h"
#include "vfs_posix.h"
#include "cherry_printk.h"

void cherry_fstools_run_tests(void)
{
    pr_info("cherry_fstools: ==> starting cherryfstools acceptance test suite...\n");

    bool fdisk_pass = false;
    bool part_bdev_pass = false;
    bool mkfs_pass = false;
    bool mount_pass = false;
    bool echo_cat_pass = false;
    bool cp_mv_pass = false;
    bool ls_rm_pass = false;
    bool umount_pass = false;

    /* 1. Create partition 1 on /dev/mmcblk0: Start LBA 2048, 65536 sectors (32MB), Linux 0x83 */
    int p_ret = fstools_part_create("/dev/mmcblk0", 1, 2048, 65536, MBR_TYPE_LINUX);
    if (p_ret >= 0) {
        fdisk_pass = true;
    }

    /* Dump partition table */
    fstools_fdisk("/dev/mmcblk0");

    /* 2. Check if partition block device /dev/mmcblk0p1 was registered */
    struct vfs_bdev *part1 = vfs_bdev_find("mmcblk0p1");
    if (!part1) part1 = vfs_bdev_find("/dev/mmcblk0p1");
    if (part1 != NULL && part1->total_blocks == 65536) {
        part_bdev_pass = true;
    }

    /* 3. Format partition /dev/mmcblk0p1 with Ext4 */
    int m_ret = ext4_mkfs("/dev/mmcblk0p1", "part1_fs");
    mkfs_pass = (m_ret == 0);

    /* 4. Mount partition on /mnt/part1 */
    mkdir("/mnt", 0755);
    mkdir("/mnt/part1", 0755);
    int mnt_ret = fstools_mount("/dev/mmcblk0p1", "/mnt/part1", "ext4");
    mount_pass = (mnt_ret == 0);

    if (mount_pass) {
        /* 5. echo and cat testing */
        int e1 = fstools_echo("First line of message", "/mnt/part1/msg.txt", false);
        int e2 = fstools_echo("Second line appended", "/mnt/part1/msg.txt", true);
        int c1 = fstools_cat("/mnt/part1/msg.txt");
        if (e1 == 0 && e2 == 0 && c1 == 0) {
            echo_cat_pass = true;
        }

        /* 6. cp and mv testing */
        int cp_ret = fstools_cp("/mnt/part1/msg.txt", "/mnt/part1/msg_copy.txt");
        int mv_ret = fstools_mv("/mnt/part1/msg_copy.txt", "/mnt/part1/msg_moved.txt");
        if (cp_ret == 0 && mv_ret == 0) {
            cp_mv_pass = true;
        }

        /* 7. ls, mkdir and rm testing */
        int ls1 = cherry_ls("/mnt/part1");
        char *fs_argv[] = { (char *)"fs", (char *)"ls", (char *)"/mnt/part1", NULL };
        int fs_cmd_ret = cmd_fs(3, fs_argv);
        int mk1 = cherry_mkdir("/mnt/part1/test_dir");
        int rm1 = cherry_rm("/mnt/part1/test_dir");
        int rm2 = cherry_rm("/mnt/part1/msg_moved.txt");
        if (ls1 == 0 && fs_cmd_ret == 0 && mk1 == 0 && rm1 == 0 && rm2 == 0) {
            ls_rm_pass = true;
        }

        /* 8. umount */
        int u_ret = cherry_umount("/mnt/part1");
        umount_pass = (u_ret == 0);
    }

    /* 9. Verify with fsck */
    int f_ret = ext4_fsck("/dev/mmcblk0p1", false);
    if (f_ret != 0) {
        mkfs_pass = false;
    }

    bool all_pass = (fdisk_pass && part_bdev_pass && mkfs_pass && mount_pass &&
                     echo_cat_pass && cp_mv_pass && ls_rm_pass && umount_pass);

    pr_info("cherry_fstools: [fstools-verify] fdisk=%s part_bdev=%s mkfs=%s mount=%s echo_cat=%s cp_mv=%s ls_rm=%s umount=%s all=%s\n",
            fdisk_pass ? "pass" : "fail",
            part_bdev_pass ? "pass" : "fail",
            mkfs_pass ? "pass" : "fail",
            mount_pass ? "pass" : "fail",
            echo_cat_pass ? "pass" : "fail",
            cp_mv_pass ? "pass" : "fail",
            ls_rm_pass ? "pass" : "fail",
            umount_pass ? "pass" : "fail",
            all_pass ? "pass" : "fail");
}
