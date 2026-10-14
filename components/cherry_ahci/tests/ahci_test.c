/*
 * CherryAHCI - Acceptance Test Suite
 *
 * Copyright (c) 2026 ForgeStitch
 * SPDX-License-Identifier: MIT
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "cherry_ahci.h"
#include "cherry_vfs.h"
#include "vfs_dev.h"
#include "cherry_printk.h"

void cherry_ahci_run_tests(void)
{
    pr_info("cherry_ahci: ==> starting cherryahci acceptance test suite...\n");

    struct ahci_dev *dev = ahci_get_primary_dev();
    struct ahci_port *port = ahci_get_primary_port();

    bool probe_pass = (dev != NULL && dev->is_ready);
    bool port_scan_pass = false;
    bool rebase_pass = false;
    bool identify_pass = false;
    bool single_io_pass = false;
    bool multi_io_pass = false;
    bool vfs_bdev_pass = false;
    bool flush_pass = false;

    if (!probe_pass || !port) {
        pr_err("cherry_ahci: primary controller or port not detected!\n");
        pr_info("cherry_ahci: [ahci-verify] probe=fail all=fail\n");
        return;
    }

    /* 1. Port scan status */
    if (port->is_active && port->is_sata) {
        port_scan_pass = true;
    }

    /* 2. Rebase memory validation */
    if (port->cmd_list && port->rcv_fis && port->cmd_table) {
        rebase_pass = true;
    }

    /* 3. Identify Data */
    if (strlen(port->model) > 0 && port->block_count > 0 && port->block_size == 512) {
        identify_pass = true;
    }

    /* 4. Single Sector Write and Readback */
    static uint8_t wbuf[512] __attribute__((aligned(512)));
    static uint8_t rbuf[512] __attribute__((aligned(512)));

    for (int i = 0; i < 512; i++) {
        wbuf[i] = (uint8_t)(0x33 ^ (i & 0xFF));
    }
    memset(rbuf, 0, sizeof(rbuf));

    uint64_t test_lba = 150;
    if (ahci_port_write_blocks(port, test_lba, 1, wbuf) == 0 &&
        ahci_port_read_blocks(port, test_lba, 1, rbuf) == 0) {
        if (memcmp(wbuf, rbuf, 512) == 0) {
            single_io_pass = true;
        } else {
            pr_err("cherry_ahci: single io data mismatch!\n");
        }
    } else {
        pr_err("cherry_ahci: single io command failed!\n");
    }

    /* 5. Multi-sector Write and Readback (8 sectors = 4KB) */
    static uint8_t multi_wbuf[4096] __attribute__((aligned(512)));
    static uint8_t multi_rbuf[4096] __attribute__((aligned(512)));

    for (int i = 0; i < 4096; i++) {
        multi_wbuf[i] = (uint8_t)(0xCC ^ (i & 0xFF));
    }
    memset(multi_rbuf, 0, sizeof(multi_rbuf));

    uint64_t multi_lba = 250;
    if (ahci_port_write_blocks(port, multi_lba, 8, multi_wbuf) == 0 &&
        ahci_port_read_blocks(port, multi_lba, 8, multi_rbuf) == 0) {
        if (memcmp(multi_wbuf, multi_rbuf, 4096) == 0) {
            multi_io_pass = true;
        } else {
            pr_err("cherry_ahci: multi io data mismatch!\n");
        }
    } else {
        pr_err("cherry_ahci: multi io command failed!\n");
    }

    /* 6. VFS Block Device Access (/dev/sda) */
    struct vfs_bdev *bdev = vfs_bdev_find("sda");
    if (bdev && bdev->ops && bdev->ops->read_blocks && bdev->ops->write_blocks) {
        memset(rbuf, 0, sizeof(rbuf));
        if (bdev->ops->write_blocks(bdev, wbuf, test_lba + 2, 1) == 0 &&
            bdev->ops->read_blocks(bdev, rbuf, test_lba + 2, 1) == 0) {
            if (memcmp(wbuf, rbuf, 512) == 0) {
                vfs_bdev_pass = true;
            }
        }
    }

    /* 7. Flush Command */
    if (ahci_port_flush(port) == 0) {
        flush_pass = true;
    }

    bool all_pass = probe_pass && port_scan_pass && rebase_pass && identify_pass &&
                    single_io_pass && multi_io_pass && vfs_bdev_pass && flush_pass;

    pr_info("cherry_ahci: [ahci-verify] probe=%s port_scan=%s rebase=%s identify=%s single_io=%s multi_io=%s bdev=%s flush=%s all=%s\n",
            probe_pass ? "pass" : "fail",
            port_scan_pass ? "pass" : "fail",
            rebase_pass ? "pass" : "fail",
            identify_pass ? "pass" : "fail",
            single_io_pass ? "pass" : "fail",
            multi_io_pass ? "pass" : "fail",
            vfs_bdev_pass ? "pass" : "fail",
            flush_pass ? "pass" : "fail",
            all_pass ? "pass" : "fail");
}
