/*
 * CherryPCI - PCI & PCIe Capabilities List Traversal & Access Engine
 *
 * Copyright (c) 2026 ForgeStitch
 * SPDX-License-Identifier: MIT
 */

#include <stdint.h>
#include <stdbool.h>
#include "cherry_pci.h"
#include "pci_regs.h"
#include "cherry_printk.h"

int pci_find_capability(struct pci_dev *dev, int cap)
{
    if (!dev) return 0;

    uint16_t status = pci_read_config_word(dev, PCI_STATUS);
    if (!(status & PCI_STATUS_CAP_LIST)) {
        return 0;
    }

    uint8_t pos = pci_read_config_byte(dev, PCI_CAPABILITY_LIST) & ~0x03;
    int limit = 48; /* Maximum iterations to prevent circular loop */

    while (pos >= 0x40 && pos <= 0xFC && limit-- > 0) {
        uint8_t id = pci_read_config_byte(dev, pos);
        if (id == (uint8_t)cap) {
            return pos;
        }
        pos = pci_read_config_byte(dev, pos + 1) & ~0x03;
    }

    return 0;
}

int pci_find_ext_capability(struct pci_dev *dev, int cap)
{
    if (!dev) return 0;

    int pos = 0x100; /* Start of PCIe extended configuration space */
    int limit = 64;

    while (pos >= 0x100 && pos <= 0xFFC && limit-- > 0) {
        uint32_t header = pci_read_config_dword(dev, pos);
        if (header == 0 || header == 0xFFFFFFFF) {
            break;
        }

        uint16_t ext_id = (uint16_t)(header & 0xFFFF);
        if (ext_id == (uint16_t)cap) {
            return pos;
        }

        uint16_t next = (uint16_t)((header >> 20) & 0xFFF);
        if (next < 0x100) {
            break;
        }
        pos = next;
    }

    return 0;
}

int pci_read_cap_dword(struct pci_dev *dev, int cap, int offset, uint32_t *val)
{
    if (!dev || !val) return -1;
    int pos = pci_find_capability(dev, cap);
    if (!pos) return -1;

    *val = pci_read_config_dword(dev, pos + offset);
    return 0;
}

int pci_write_cap_dword(struct pci_dev *dev, int cap, int offset, uint32_t val)
{
    if (!dev) return -1;
    int pos = pci_find_capability(dev, cap);
    if (!pos) return -1;

    pci_write_config_dword(dev, pos + offset, val);
    return 0;
}

void pci_parse_capabilities(struct pci_dev *dev)
{
    if (!dev) return;

    /* Cache standard capabilities */
    dev->pm_cap   = (uint8_t)pci_find_capability(dev, PCI_CAP_ID_PM);
    dev->msi_cap  = (uint8_t)pci_find_capability(dev, PCI_CAP_ID_MSI);
    dev->msix_cap = (uint8_t)pci_find_capability(dev, PCI_CAP_ID_MSIX);
    dev->pcie_cap = (uint8_t)pci_find_capability(dev, PCI_CAP_ID_EXP);

    /* Cache PCIe extended capabilities */
    dev->aer_cap  = (uint16_t)pci_find_ext_capability(dev, PCI_EXT_CAP_ID_ERR);
}
