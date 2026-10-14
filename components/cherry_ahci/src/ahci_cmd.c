/*
 * CherryAHCI - Command Execution and ATA Command Engine
 *
 * Copyright (c) 2026 ForgeStitch
 * SPDX-License-Identifier: MIT
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "cherry_ahci.h"
#include "ahci_spec.h"
#include "cherry_printk.h"

static inline void ahci_dmb(void)
{
    __asm__ volatile("dmb sy" ::: "memory");
}

int ahci_exec_cmd(struct ahci_port *port, uint8_t ata_cmd, uint64_t lba, uint32_t count, void *buf, bool is_write)
{
    if (!port || !port->is_active) return -1;

    uint32_t slot = 0;
    struct ahci_cmd_header *hdr = &port->cmd_list[slot];
    struct ahci_cmd_table *tbl = port->cmd_table;
    memset(tbl, 0, sizeof(*tbl));

    uint32_t byte_count = count * (port->block_size ? port->block_size : 512);
    if (ata_cmd == ATA_CMD_IDENTIFY) {
        byte_count = 512;
    } else if (ata_cmd == ATA_CMD_FLUSH_CACHE_EXT) {
        byte_count = 0;
    }

    /* 1. Setup Command Header */
    uint16_t flags = (sizeof(struct ahci_fis_reg_h2d) / 4);
    if (is_write) {
        flags |= (1U << 6);
    }
    hdr->flags = flags;
    hdr->prdtl = byte_count > 0 ? 1 : 0;
    hdr->prdbc = 0;
    hdr->ctba = (uint32_t)((uintptr_t)tbl & 0xFFFFFFFF);
    hdr->ctbau = (uint32_t)((uint64_t)(uintptr_t)tbl >> 32);

    /* 2. Setup PRDT Entry if data transfer is required */
    if (byte_count > 0 && buf) {
        tbl->prdt[0].dba = (uint32_t)((uintptr_t)buf & 0xFFFFFFFF);
        tbl->prdt[0].dbau = (uint32_t)((uint64_t)(uintptr_t)buf >> 32);
        tbl->prdt[0].dbc_ioc = ((byte_count - 1) & 0x3FFFFF) | (1U << 31);
    }

    /* 3. Setup Command FIS (Register H2D) */
    struct ahci_fis_reg_h2d *cfis = (struct ahci_fis_reg_h2d *)tbl->cfis;
    cfis->fis_type = AHCI_FIS_REG_H2D;
    cfis->pmport_c = (1U << 7); /* Command bit set */
    cfis->command = ata_cmd;

    if (ata_cmd == ATA_CMD_READ_DMA_EXT || ata_cmd == ATA_CMD_WRITE_DMA_EXT) {
        cfis->device = (1U << 6); /* LBA mode */
        cfis->lba_lo = (uint8_t)(lba & 0xFF);
        cfis->lba_mid = (uint8_t)((lba >> 8) & 0xFF);
        cfis->lba_hi = (uint8_t)((lba >> 16) & 0xFF);
        cfis->lba_exp_lo = (uint8_t)((lba >> 24) & 0xFF);
        cfis->lba_exp_mid = (uint8_t)((lba >> 32) & 0xFF);
        cfis->lba_exp_hi = (uint8_t)((lba >> 40) & 0xFF);
        cfis->count_lo = (uint8_t)(count & 0xFF);
        cfis->count_hi = (uint8_t)((count >> 8) & 0xFF);
    } else if (ata_cmd == ATA_CMD_IDENTIFY) {
        cfis->device = 0;
    } else if (ata_cmd == ATA_CMD_FLUSH_CACHE_EXT) {
        cfis->device = (1U << 6);
    }

    ahci_dmb();

    /* 4. Issue Command via PxCI */
    *(volatile uint32_t *)(port->mmio + AHCI_PORT_CI) = (1U << slot);

    /* 5. Wait for Completion */
    uint32_t timeout = 5000000;
    while ((*(volatile uint32_t *)(port->mmio + AHCI_PORT_CI) & (1U << slot)) && --timeout) {
        uint32_t is = *(volatile uint32_t *)(port->mmio + AHCI_PORT_IS);
        uint32_t tfd = *(volatile uint32_t *)(port->mmio + AHCI_PORT_TFD);
        if (tfd & AHCI_PORT_TFD_ERR) {
            pr_err("cherry_ahci: port %u cmd 0x%02x error (tfd=0x%x, is=0x%x)\n",
                   port->port_no, ata_cmd, tfd, is);
            *(volatile uint32_t *)(port->mmio + AHCI_PORT_IS) = 0xFFFFFFFF;
            return -1;
        }
        __asm__ volatile("yield");
    }

    /* Clear Interrupt and Error status */
    *(volatile uint32_t *)(port->mmio + AHCI_PORT_IS) = 0xFFFFFFFF;
    *(volatile uint32_t *)(port->mmio + AHCI_PORT_SERR) = 0xFFFFFFFF;

    if (timeout == 0) {
        pr_err("cherry_ahci: port %u cmd 0x%02x timed out!\n", port->port_no, ata_cmd);
        return -1;
    }

    uint32_t tfd = *(volatile uint32_t *)(port->mmio + AHCI_PORT_TFD);
    if (tfd & AHCI_PORT_TFD_ERR) {
        pr_err("cherry_ahci: port %u cmd 0x%02x completion error (tfd=0x%x)\n",
               port->port_no, ata_cmd, tfd);
        return -1;
    }

    return 0;
}

int ahci_port_read_blocks(struct ahci_port *port, uint64_t lba, uint32_t count, void *buf)
{
    if (!port || !port->is_active || count == 0 || !buf) return -1;

    uint32_t max_blocks = 128; /* 64KB per batch */
    uint8_t *ptr = (uint8_t *)buf;

    while (count > 0) {
        uint32_t batch = count > max_blocks ? max_blocks : count;
        int ret = ahci_exec_cmd(port, ATA_CMD_READ_DMA_EXT, lba, batch, ptr, false);
        if (ret != 0) return ret;

        lba += batch;
        ptr += batch * (port->block_size ? port->block_size : 512);
        count -= batch;
    }

    return 0;
}

int ahci_port_write_blocks(struct ahci_port *port, uint64_t lba, uint32_t count, const void *buf)
{
    if (!port || !port->is_active || count == 0 || !buf) return -1;

    uint32_t max_blocks = 128;
    const uint8_t *ptr = (const uint8_t *)buf;

    while (count > 0) {
        uint32_t batch = count > max_blocks ? max_blocks : count;
        int ret = ahci_exec_cmd(port, ATA_CMD_WRITE_DMA_EXT, lba, batch, (void *)ptr, true);
        if (ret != 0) return ret;

        lba += batch;
        ptr += batch * (port->block_size ? port->block_size : 512);
        count -= batch;
    }

    return 0;
}

int ahci_port_flush(struct ahci_port *port)
{
    if (!port || !port->is_active) return -1;
    return ahci_exec_cmd(port, ATA_CMD_FLUSH_CACHE_EXT, 0, 0, NULL, false);
}
