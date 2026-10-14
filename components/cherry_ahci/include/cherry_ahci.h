/*
 * CherryAHCI - Serial ATA Advanced Host Controller Subsystem
 *
 * Copyright (c) 2026 ForgeStitch
 * SPDX-License-Identifier: MIT
 */

#ifndef _CHERRY_AHCI_H_
#define _CHERRY_AHCI_H_

#include <stdint.h>
#include <stdbool.h>
#include "ahci_spec.h"
#include "cherry_pci.h"
#include "cherry_vfs.h"
#include "vfs_dev.h"

#define AHCI_MAX_PORTS  4

struct ahci_dev;

struct ahci_port {
    struct ahci_dev        *dev;
    uint32_t                port_no;
    uintptr_t               mmio;
    struct ahci_cmd_header *cmd_list;
    struct ahci_rcv_fis    *rcv_fis;
    struct ahci_cmd_table  *cmd_table;
    bool                    is_active;
    bool                    is_sata;
    uint64_t                block_count;
    uint32_t                block_size;
    char                    model[41];
    char                    serial[21];
    char                    firmware[9];
    struct vfs_bdev         bdev;
    struct vfs_bdev         alias_bdev;
};

struct ahci_dev {
    struct pci_dev         *pdev;
    uintptr_t               abar;
    uint32_t                cap;
    uint32_t                ports_impl;
    uint32_t                num_ports;
    struct ahci_port        ports[AHCI_MAX_PORTS];
    bool                    is_ready;
};

/* Subsystem Management */
int  cherry_ahci_init(void);
void cherry_ahci_run_tests(void);
struct ahci_dev *ahci_get_primary_dev(void);
struct ahci_port *ahci_get_primary_port(void);

/* Block I/O Operations */
int  ahci_port_read_blocks(struct ahci_port *port, uint64_t lba, uint32_t count, void *buf);
int  ahci_port_write_blocks(struct ahci_port *port, uint64_t lba, uint32_t count, const void *buf);
int  ahci_port_flush(struct ahci_port *port);

/* Shell Diagnostic Commands */
void ahci_register_shell_commands(void);

#endif /* _CHERRY_AHCI_H_ */
