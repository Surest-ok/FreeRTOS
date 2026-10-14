/*
 * CherryNVMe - CherryShell Diagnostic Commands (nvme_info, nvme_read, nvme_write)
 *
 * Copyright (c) 2026 ForgeStitch
 * SPDX-License-Identifier: MIT
 */

#include <stdint.h>
#include <string.h>
#include "cherry_nvme.h"
#include "cherry_shell.h"
#include "cherry_printk.h"

static uint32_t simple_strtoul(const char *str)
{
    uint32_t val = 0;
    while (*str >= '0' && *str <= '9') {
        val = val * 10 + (*str - '0');
        str++;
    }
    return val;
}

int cmd_nvme_info(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    struct nvme_dev *dev = nvme_get_primary_dev();
    if (!dev || !dev->is_ready) {
        printk("nvme: no active nvme controller detected\n");
        return -1;
    }

    printk("=== cherry_nvme: controller status ===\n");
    printk("model       : %s\n", dev->model);
    printk("serial no.  : %s\n", dev->serial);
    printk("firmware    : %s\n", dev->firmware);
    printk("namespaces  : %u\n", dev->num_namespaces);
    printk("primary ns  : nsid=%u, blocks=%llu, block_size=%u, capacity=%llu mb\n",
           dev->ns.nsid,
           (unsigned long long)dev->ns.block_count,
           dev->ns.block_size,
           (unsigned long long)(dev->ns.block_count * dev->ns.block_size / (1024 * 1024)));
    printk("block device: /dev/nvme0n1 (alias /dev/nvme0)\n");
    return 0;
}

int cmd_nvme_read(int argc, char **argv)
{
    if (argc < 2) {
        printk("usage: nvme_read <lba> [count]\n");
        return -1;
    }

    uint32_t lba = simple_strtoul(argv[1]);
    uint32_t count = argc >= 3 ? simple_strtoul(argv[2]) : 1;

    struct nvme_dev *dev = nvme_get_primary_dev();
    if (!dev || !dev->is_ready) {
        printk("nvme: controller not ready\n");
        return -1;
    }

    uint8_t buffer[512];
    for (uint32_t i = 0; i < count; i++) {
        if (nvme_read_blocks(dev, dev->ns.nsid, lba + i, 1, buffer) != 0) {
            printk("nvme_read failed at lba %u\n", lba + i);
            return -1;
        }

        printk("--- NVMe LBA %u ---\n", lba + i);
        for (int row = 0; row < 64; row += 16) {
            printk("%04x: %02x %02x %02x %02x %02x %02x %02x %02x  %02x %02x %02x %02x %02x %02x %02x %02x\n",
                   row,
                   buffer[row+0], buffer[row+1], buffer[row+2], buffer[row+3],
                   buffer[row+4], buffer[row+5], buffer[row+6], buffer[row+7],
                   buffer[row+8], buffer[row+9], buffer[row+10], buffer[row+11],
                   buffer[row+12], buffer[row+13], buffer[row+14], buffer[row+15]);
        }
    }
    return 0;
}

int cmd_nvme_write(int argc, char **argv)
{
    if (argc < 3) {
        printk("usage: nvme_write <lba> <string>\n");
        return -1;
    }

    uint32_t lba = simple_strtoul(argv[1]);
    const char *text = argv[2];

    struct nvme_dev *dev = nvme_get_primary_dev();
    if (!dev || !dev->is_ready) {
        printk("nvme: controller not ready\n");
        return -1;
    }

    uint8_t buffer[512];
    memset(buffer, 0, sizeof(buffer));
    strncpy((char *)buffer, text, sizeof(buffer) - 1);

    if (nvme_write_blocks(dev, dev->ns.nsid, lba, 1, buffer) != 0) {
        printk("nvme_write failed at lba %u\n", lba);
        return -1;
    }

    printk("successfully wrote text to nvme lba %u\n", lba);
    return 0;
}

void nvme_register_shell_commands(void)
{
    cherry_shell_register_symbol("nvme_info", (uintptr_t)cmd_nvme_info, CHERRY_SHELL_SYM_CMD, "display nvme controller and namespace info");
    cherry_shell_register_symbol("nvme_read", (uintptr_t)cmd_nvme_read, CHERRY_SHELL_SYM_CMD, "read blocks from nvme device: nvme_read <lba> [count]");
    cherry_shell_register_symbol("nvme_write", (uintptr_t)cmd_nvme_write, CHERRY_SHELL_SYM_CMD, "write text data to nvme block: nvme_write <lba> <string>");
}
