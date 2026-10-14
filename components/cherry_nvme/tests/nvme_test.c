/*
 * CherryNVMe - Acceptance Test Suite
 *
 * Copyright (c) 2026 ForgeStitch
 * SPDX-License-Identifier: MIT
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "cherry_nvme.h"
#include "cherry_vfs.h"
#include "vfs_dev.h"
#include "cherry_printk.h"

void cherry_nvme_run_tests(void)
{
    pr_info("cherry_nvme: ==> starting cherrynvme acceptance test suite...\n");

    struct nvme_dev *dev = nvme_get_primary_dev();
    bool probe_pass = (dev != NULL && dev->is_ready);
    bool id_ctrl_pass = false;
    bool queues_pass = false;
    bool id_ns_pass = false;
    bool single_io_pass = false;
    bool multi_io_pass = false;
    bool vfs_bdev_pass = false;
    bool flush_pass = false;

    if (!probe_pass) {
        pr_err("cherry_nvme: primary controller not detected or ready!\n");
        pr_info("cherry_nvme: [nvme-verify] probe=fail all=fail\n");
        return;
    }

    /* 1. Verify Identify Controller Information */
    if (strlen(dev->model) > 0 && strlen(dev->serial) > 0) {
        id_ctrl_pass = true;
    }

    /* 2. Verify Queues Initialized */
    if (dev->admin_q.sq && dev->admin_q.cq && dev->io_q.sq && dev->io_q.cq) {
        queues_pass = true;
    }

    /* 3. Verify Identify Namespace */
    if (dev->ns.nsid == 1 && dev->ns.block_size >= 512 && dev->ns.block_count > 0) {
        id_ns_pass = true;
    }

    /* 4. Single Block Write and Readback */
    static uint8_t wbuf[512] __attribute__((aligned(512)));
    static uint8_t rbuf[512] __attribute__((aligned(512)));

    for (int i = 0; i < 512; i++) {
        wbuf[i] = (uint8_t)(0xAA ^ (i & 0xFF));
    }
    memset(rbuf, 0, sizeof(rbuf));

    uint64_t test_lba = 100;
    if (nvme_write_blocks(dev, dev->ns.nsid, test_lba, 1, wbuf) == 0 &&
        nvme_read_blocks(dev, dev->ns.nsid, test_lba, 1, rbuf) == 0) {
        if (memcmp(wbuf, rbuf, 512) == 0) {
            single_io_pass = true;
        } else {
            pr_err("cherry_nvme: single io data mismatch!\n");
        }
    } else {
        pr_err("cherry_nvme: single io command failed!\n");
    }

    /* 5. Multi-block Write and Readback (8 blocks = 4KB) */
    static uint8_t multi_wbuf[4096] __attribute__((aligned(4096)));
    static uint8_t multi_rbuf[4096] __attribute__((aligned(4096)));

    for (int i = 0; i < 4096; i++) {
        multi_wbuf[i] = (uint8_t)(0x55 ^ (i & 0xFF));
    }
    memset(multi_rbuf, 0, sizeof(multi_rbuf));

    uint64_t multi_lba = 200;
    if (nvme_write_blocks(dev, dev->ns.nsid, multi_lba, 8, multi_wbuf) == 0 &&
        nvme_read_blocks(dev, dev->ns.nsid, multi_lba, 8, multi_rbuf) == 0) {
        if (memcmp(multi_wbuf, multi_rbuf, 4096) == 0) {
            multi_io_pass = true;
        } else {
            pr_err("cherry_nvme: multi io data mismatch!\n");
        }
    } else {
        pr_err("cherry_nvme: multi io command failed!\n");
    }

    /* 6. VFS Block Device Access (/dev/nvme0n1) */
    struct vfs_bdev *bdev = vfs_bdev_find("nvme0n1");
    if (bdev && bdev->ops && bdev->ops->read_blocks && bdev->ops->write_blocks) {
        memset(rbuf, 0, sizeof(rbuf));
        if (bdev->ops->write_blocks(bdev, wbuf, test_lba + 5, 1) == 0 &&
            bdev->ops->read_blocks(bdev, rbuf, test_lba + 5, 1) == 0) {
            if (memcmp(wbuf, rbuf, 512) == 0) {
                vfs_bdev_pass = true;
            }
        }
    }

    /* 7. Flush Command */
    if (nvme_flush(dev, dev->ns.nsid) == 0) {
        flush_pass = true;
    }

    bool all_pass = probe_pass && id_ctrl_pass && queues_pass && id_ns_pass &&
                    single_io_pass && multi_io_pass && vfs_bdev_pass && flush_pass;

    pr_info("cherry_nvme: [nvme-verify] probe=%s ctrl=%s queues=%s ns=%s single_io=%s multi_io=%s bdev=%s flush=%s all=%s\n",
            probe_pass ? "pass" : "fail",
            id_ctrl_pass ? "pass" : "fail",
            queues_pass ? "pass" : "fail",
            id_ns_pass ? "pass" : "fail",
            single_io_pass ? "pass" : "fail",
            multi_io_pass ? "pass" : "fail",
            vfs_bdev_pass ? "pass" : "fail",
            flush_pass ? "pass" : "fail",
            all_pass ? "pass" : "fail");
}
