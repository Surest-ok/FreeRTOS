/*
 * CherryAHCI - Subsystem Core and PCIe Controller Driver
 *
 * Copyright (c) 2026 ForgeStitch
 * SPDX-License-Identifier: MIT
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "platform.h"
#include "cherry_pci.h"
#include "cherry_ahci.h"
#include "ahci_spec.h"
#include "cherry_printk.h"

static struct ahci_dev s_primary_ahci;
static bool s_primary_ahci_active = false;

/* Aligned Hardware Structures for Port 0 */
static __attribute__((aligned(1024))) struct ahci_cmd_header s_port0_cmd_list[32];
static __attribute__((aligned(256)))  struct ahci_rcv_fis    s_port0_rcv_fis;
static __attribute__((aligned(128)))  struct ahci_cmd_table  s_port0_cmd_table;
static __attribute__((aligned(512)))  uint8_t                s_identify_buf[512];

extern int  ahci_exec_cmd(struct ahci_port *port, uint8_t ata_cmd, uint64_t lba, uint32_t count, void *buf, bool is_write);
extern int  ahci_bdev_register(struct ahci_port *port);

static void ata_string_copy(char *dst, const uint8_t *src, size_t len)
{
    for (size_t i = 0; i < len; i += 2) {
        dst[i]     = (char)src[i + 1];
        dst[i + 1] = (char)src[i];
    }
    dst[len] = '\0';

    /* Trim trailing whitespace */
    while (len > 0 && (dst[len - 1] == ' ' || dst[len - 1] == '\0')) {
        dst[len - 1] = '\0';
        len--;
    }
}

static void ahci_port_stop(uintptr_t port_base)
{
    /* Clear ST */
    uint32_t cmd = *(volatile uint32_t *)(port_base + AHCI_PORT_CMD);
    cmd &= ~AHCI_PORT_CMD_ST;
    *(volatile uint32_t *)(port_base + AHCI_PORT_CMD) = cmd;

    uint32_t timeout = 100000;
    while ((*(volatile uint32_t *)(port_base + AHCI_PORT_CMD) & AHCI_PORT_CMD_CR) && --timeout) {
        __asm__ volatile("yield");
    }

    /* Clear FRE */
    cmd = *(volatile uint32_t *)(port_base + AHCI_PORT_CMD);
    cmd &= ~AHCI_PORT_CMD_FRE;
    *(volatile uint32_t *)(port_base + AHCI_PORT_CMD) = cmd;

    timeout = 100000;
    while ((*(volatile uint32_t *)(port_base + AHCI_PORT_CMD) & AHCI_PORT_CMD_FR) && --timeout) {
        __asm__ volatile("yield");
    }
}

static void ahci_port_start(uintptr_t port_base)
{
    uint32_t timeout = 100000;
    while ((*(volatile uint32_t *)(port_base + AHCI_PORT_CMD) & AHCI_PORT_CMD_CR) && --timeout) {
        __asm__ volatile("yield");
    }

    /* Set FRE then ST */
    uint32_t cmd = *(volatile uint32_t *)(port_base + AHCI_PORT_CMD);
    cmd |= AHCI_PORT_CMD_FRE;
    *(volatile uint32_t *)(port_base + AHCI_PORT_CMD) = cmd;

    cmd |= AHCI_PORT_CMD_ST;
    *(volatile uint32_t *)(port_base + AHCI_PORT_CMD) = cmd;
}

static int ahci_pci_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
    (void)id;
    pr_info("cherry_ahci: probing ahci sata controller at [%02x:%02x.%d]...\n",
            pdev->bus, pdev->dev, pdev->fn);

    /* Enable device MMIO and bus master */
    pci_enable_device(pdev);
    pci_set_master(pdev);

    /* AHCI ABAR resides at BAR5 */
    uintptr_t abar = (uintptr_t)pdev->resource[5];
    if (abar == 0) {
        pr_err("cherry_ahci: invalid bar5 abar address\n");
        return -1;
    }

    struct ahci_dev *dev = &s_primary_ahci;
    memset(dev, 0, sizeof(*dev));
    dev->pdev = pdev;
    dev->abar = abar;

    /* 1. Read Capabilities & Enable AHCI mode */
    dev->cap = *(volatile uint32_t *)(abar + AHCI_GHC_CAP);
    dev->ports_impl = *(volatile uint32_t *)(abar + AHCI_GHC_PI);
    dev->num_ports = (dev->cap & 0x1F) + 1;

    uint32_t ghc = *(volatile uint32_t *)(abar + AHCI_GHC_GHC);
    *(volatile uint32_t *)(abar + AHCI_GHC_GHC) = ghc | AHCI_GHC_AE;

    pr_info("cherry_ahci: abar=0x%lx cap=0x%x pi=0x%x num_ports=%u\n",
            (unsigned long)abar, dev->cap, dev->ports_impl, dev->num_ports);

    /* 2. Scan ports */
    for (uint32_t p = 0; p < AHCI_MAX_PORTS; p++) {
        if (!(dev->ports_impl & (1U << p))) {
            continue;
        }

        uintptr_t port_base = abar + AHCI_PORT_OFFSET(p);
        uint32_t ssts = *(volatile uint32_t *)(port_base + AHCI_PORT_SSTS);
        uint8_t det = ssts & AHCI_PORT_SSTS_DET_MASK;

        if (det != AHCI_PORT_SSTS_DET_PRESENT) {
            continue;
        }

        uint32_t sig = *(volatile uint32_t *)(port_base + AHCI_PORT_SIG);
        pr_info("cherry_ahci: port %u device detected (ssts=0x%x, sig=0x%08x)\n", p, ssts, sig);

        struct ahci_port *port = &dev->ports[p];
        port->dev = dev;
        port->port_no = p;
        port->mmio = port_base;

        if (sig == AHCI_SIG_ATA || sig == 0x00000000 || sig == 0xFFFFFFFF) {
            port->is_sata = true;
        }

        /* 3. Rebase Port Memory (Port 0 uses statically allocated buffers) */
        if (p == 0) {
            ahci_port_stop(port_base);

            memset(s_port0_cmd_list, 0, sizeof(s_port0_cmd_list));
            memset(&s_port0_rcv_fis, 0, sizeof(s_port0_rcv_fis));
            memset(&s_port0_cmd_table, 0, sizeof(s_port0_cmd_table));

            port->cmd_list = s_port0_cmd_list;
            port->rcv_fis = &s_port0_rcv_fis;
            port->cmd_table = &s_port0_cmd_table;

            *(volatile uint32_t *)(port_base + AHCI_PORT_CLB) = (uint32_t)((uintptr_t)s_port0_cmd_list & 0xFFFFFFFF);
            *(volatile uint32_t *)(port_base + AHCI_PORT_CLBU) = (uint32_t)((uint64_t)(uintptr_t)s_port0_cmd_list >> 32);
            *(volatile uint32_t *)(port_base + AHCI_PORT_FB) = (uint32_t)((uintptr_t)&s_port0_rcv_fis & 0xFFFFFFFF);
            *(volatile uint32_t *)(port_base + AHCI_PORT_FBU) = (uint32_t)((uint64_t)(uintptr_t)&s_port0_rcv_fis >> 32);

            *(volatile uint32_t *)(port_base + AHCI_PORT_SERR) = 0xFFFFFFFF;
            *(volatile uint32_t *)(port_base + AHCI_PORT_IS) = 0xFFFFFFFF;

            ahci_port_start(port_base);
            port->is_active = true;

            /* 4. Issue ATA IDENTIFY DEVICE */
            memset(s_identify_buf, 0, sizeof(s_identify_buf));
            if (ahci_exec_cmd(port, ATA_CMD_IDENTIFY, 0, 1, s_identify_buf, false) == 0) {
                const uint16_t *id_words = (const uint16_t *)s_identify_buf;

                ata_string_copy(port->serial, &s_identify_buf[20], 20);
                ata_string_copy(port->firmware, &s_identify_buf[46], 8);
                ata_string_copy(port->model, &s_identify_buf[54], 40);

                uint64_t sectors = ((uint64_t)id_words[103] << 48) |
                                   ((uint64_t)id_words[102] << 32) |
                                   ((uint64_t)id_words[101] << 16) |
                                   id_words[100];
                if (sectors == 0) {
                    sectors = ((uint32_t)id_words[61] << 16) | id_words[60];
                }

                port->block_size = 512;
                port->block_count = sectors;

                pr_info("cherry_ahci: port %u identified (model='%s', serial='%s', fw='%s', sectors=%llu, %llu mb)\n",
                        p, port->model, port->serial, port->firmware,
                        (unsigned long long)port->block_count,
                        (unsigned long long)(port->block_count * port->block_size / (1024 * 1024)));

                /* 5. Register CherryVFS Block Device */
                ahci_bdev_register(port);
            } else {
                pr_warn("cherry_ahci: port %u ata identify failed\n", p);
            }
        }
    }

    dev->is_ready = true;
    s_primary_ahci_active = true;
    pdev->drv_data = dev;
    return 0;
}

static void ahci_pci_remove(struct pci_dev *pdev)
{
    (void)pdev;
    s_primary_ahci_active = false;
}

static const struct pci_device_id ahci_pci_tbl[] = {
    { 0x8086, 0x2922, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0 },
    { PCI_ANY_ID, PCI_ANY_ID, PCI_ANY_ID, PCI_ANY_ID, 0x010601, 0xFFFF00, 0 },
    { 0, 0, 0, 0, 0, 0, 0 }
};

static struct pci_driver ahci_pci_driver = {
    .name = "ahci-pci",
    .id_table = ahci_pci_tbl,
    .probe = ahci_pci_probe,
    .remove = ahci_pci_remove,
};

int cherry_ahci_init(void)
{
    pr_info("cherry_ahci: initializing pci ahci subsystem...\n");

    /* Register AHCI PCIe Driver */
    pci_register_driver(&ahci_pci_driver);

    /* Register Shell Commands */
    ahci_register_shell_commands();

    pr_info("cherry_ahci: subsystem initialized successfully\n");
    return 0;
}

struct ahci_dev *ahci_get_primary_dev(void)
{
    return s_primary_ahci_active ? &s_primary_ahci : NULL;
}

struct ahci_port *ahci_get_primary_port(void)
{
    if (!s_primary_ahci_active) return NULL;
    for (int i = 0; i < AHCI_MAX_PORTS; i++) {
        if (s_primary_ahci.ports[i].is_active) {
            return &s_primary_ahci.ports[i];
        }
    }
    return NULL;
}
