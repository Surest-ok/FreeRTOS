/*
 * CherryPCI - Interrupt Allocation & Management (MSI / MSI-X & Legacy INTx)
 *
 * Copyright (c) 2026 ForgeStitch
 * SPDX-License-Identifier: MIT
 */

#include <stdint.h>
#include <stdbool.h>
#include "platform.h"
#include "gic.h"
#include "cherry_pci.h"
#include "cherry_its.h"
#include "cherry_printk.h"
#include "arch_api/irq.h"

int pci_assign_irq(struct pci_dev *dev)
{
    if (!dev) return -1;

    dev->irq_pin = pci_read_config_byte(dev, PCI_INTERRUPT_PIN);
    if (dev->irq_pin > 0 && dev->irq_pin <= 4) {
        /* Standard QEMU virt legacy INTx mapping: SPI 35..38 */
        uint32_t spi = 32 + 3 + (dev->dev + dev->irq_pin - 1) % 4;
        dev->irq_line = (uint8_t)spi;
        pci_write_config_byte(dev, PCI_INTERRUPT_LINE, (uint8_t)spi);
        dev->irq = spi;
    } else {
        dev->irq = 0;
    }
    return 0;
}

int pci_enable_msi(struct pci_dev *dev, isr_fn_t handler, void *arg)
{
    if (!dev) return -1;

    /* 1. Try GICv3 ITS MSI if capability is present and ITS is active */
    if (dev->msi_cap != 0 && its_is_available()) {
        uint32_t lpi = 0;
        uint32_t event_id = 0;
        if (its_alloc_msi(dev, handler, arg, &lpi, &event_id) == 0) {
            uint16_t flags = pci_read_config_word(dev, dev->msi_cap + PCI_MSI_FLAGS);
            bool is_64 = (flags & PCI_MSI_FLAGS_64BIT) != 0;

            pci_write_config_dword(dev, dev->msi_cap + PCI_MSI_ADDRESS_LO, (uint32_t)GITS_TRANSLATER);
            if (is_64) {
                pci_write_config_dword(dev, dev->msi_cap + PCI_MSI_ADDRESS_HI, 0);
                pci_write_config_word(dev, dev->msi_cap + PCI_MSI_DATA_64, (uint16_t)event_id);
            } else {
                pci_write_config_word(dev, dev->msi_cap + PCI_MSI_DATA_32, (uint16_t)event_id);
            }

            /* Enable MSI in PCIe config space */
            flags |= PCI_MSI_FLAGS_ENABLE;
            pci_write_config_word(dev, dev->msi_cap + PCI_MSI_FLAGS, flags);

            dev->msi_enabled = true;
            dev->msi_irq = lpi;
            dev->irq = lpi;

            pr_info("cherry_pci: [%02x:%02x.%d] enabled gic its msi (lpi=%d, translater=0x%lx)\n",
                    dev->bus, dev->dev, dev->fn, (int)lpi, (unsigned long)GITS_TRANSLATER);
            return 0;
        }
    }

    /* 2. Fallback to legacy INTx interrupt */
    if (dev->irq_pin > 0) {
        uint32_t spi = 32 + 3 + (dev->dev + dev->irq_pin - 1) % 4;
        arch_irq_register(spi, (arch_irq_handler_t)handler, arg);
        gic_enable_irq(spi);
        dev->irq = spi;
        pr_info("cherry_pci: [%02x:%02x.%d] using legacy pci intx (spi=%d)\n",
                dev->bus, dev->dev, dev->fn, (int)spi);
        return 0;
    }

    return -1;
}

int pci_disable_msi(struct pci_dev *dev)
{
    if (!dev || !dev->msi_enabled || dev->msi_cap == 0) {
        return 0;
    }

    uint16_t flags = pci_read_config_word(dev, dev->msi_cap + PCI_MSI_FLAGS);
    flags &= ~PCI_MSI_FLAGS_ENABLE;
    pci_write_config_word(dev, dev->msi_cap + PCI_MSI_FLAGS, flags);

    dev->msi_enabled = false;
    dev->msi_irq = 0;

    /* Revert to default INTx */
    pci_assign_irq(dev);
    return 0;
}

int pci_alloc_irq_vectors(struct pci_dev *dev, unsigned int min_vecs, unsigned int max_vecs, unsigned int flags)
{
    (void)min_vecs;
    (void)max_vecs;

    if (!dev) return -1;

    if ((flags & (PCI_IRQ_MSI | PCI_IRQ_MSIX)) && dev->msi_cap != 0 && its_is_available()) {
        return 1;
    }

    if ((flags & PCI_IRQ_LEGACY) && dev->irq_pin > 0) {
        return 1;
    }

    return -1;
}

void pci_free_irq_vectors(struct pci_dev *dev)
{
    if (dev && dev->msi_enabled) {
        pci_disable_msi(dev);
    }
}
