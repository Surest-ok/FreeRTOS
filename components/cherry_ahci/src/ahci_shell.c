/*
 * CherryAHCI - CherryShell Diagnostic Commands (ahci_info, ahci_read, ahci_write)
 *
 * Copyright (c) 2026 ForgeStitch
 * SPDX-License-Identifier: MIT
 */

#include <stdint.h>
#include <string.h>
#include "cherry_ahci.h"
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

int cmd_ahci_info(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    struct ahci_port *port = ahci_get_primary_port();
    if (!port || !port->is_active) {
        printk("ahci: no active ahci port or sata disk detected\n");
        return -1;
    }

    printk("=== cherry_ahci: port status ===\n");
    printk("port index  : %u\n", port->port_no);
    printk("model       : %s\n", port->model);
    printk("serial no.  : %s\n", port->serial);
    printk("firmware    : %s\n", port->firmware);
    printk("capacity    : %llu mb (%llu sectors)\n",
           (unsigned long long)(port->block_count * port->block_size / (1024 * 1024)),
           (unsigned long long)port->block_count);
    printk("sector size : %u bytes\n", port->block_size);
    printk("block device: /dev/sda (alias /dev/sata0)\n");
    return 0;
}

int cmd_ahci_read(int argc, char **argv)
{
    if (argc < 2) {
        printk("usage: ahci_read <lba> [count]\n");
        return -1;
    }

    uint32_t lba = simple_strtoul(argv[1]);
    uint32_t count = argc >= 3 ? simple_strtoul(argv[2]) : 1;

    struct ahci_port *port = ahci_get_primary_port();
    if (!port || !port->is_active) {
        printk("ahci: port not ready\n");
        return -1;
    }

    uint8_t buffer[512];
    for (uint32_t i = 0; i < count; i++) {
        if (ahci_port_read_blocks(port, lba + i, 1, buffer) != 0) {
            printk("ahci_read failed at lba %u\n", lba + i);
            return -1;
        }

        printk("--- SATA LBA %u ---\n", lba + i);
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

int cmd_ahci_write(int argc, char **argv)
{
    if (argc < 3) {
        printk("usage: ahci_write <lba> <string>\n");
        return -1;
    }

    uint32_t lba = simple_strtoul(argv[1]);
    const char *text = argv[2];

    struct ahci_port *port = ahci_get_primary_port();
    if (!port || !port->is_active) {
        printk("ahci: port not ready\n");
        return -1;
    }

    uint8_t buffer[512];
    memset(buffer, 0, sizeof(buffer));
    strncpy((char *)buffer, text, sizeof(buffer) - 1);

    if (ahci_port_write_blocks(port, lba, 1, buffer) != 0) {
        printk("ahci_write failed at lba %u\n", lba);
        return -1;
    }

    printk("successfully wrote text to sata lba %u\n", lba);
    return 0;
}

void ahci_register_shell_commands(void)
{
    cherry_shell_register_symbol("ahci_info", (uintptr_t)cmd_ahci_info, CHERRY_SHELL_SYM_CMD, "display ahci sata port and disk info");
    cherry_shell_register_symbol("ahci_read", (uintptr_t)cmd_ahci_read, CHERRY_SHELL_SYM_CMD, "read sectors from ahci sata disk: ahci_read <lba> [count]");
    cherry_shell_register_symbol("ahci_write", (uintptr_t)cmd_ahci_write, CHERRY_SHELL_SYM_CMD, "write text data to ahci sata sector: ahci_write <lba> <string>");
}
