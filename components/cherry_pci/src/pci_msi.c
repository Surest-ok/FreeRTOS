/*
 * CherryPCI - MSI / MSI-X & Interrupt Management
 */

#include <stdint.h>
#include <stdbool.h>
#include "platform.h"
#include "gic.h"
#include "cherry_pci.h"
#include "cherry_its.h"
#include "cherry_printk.h"
#include "arch_api/irq.h"

int pci_enable_msi(struct pci_dev *dev, isr_fn_t handler, void *arg)
{
    if (dev == 0) {
        return -1;
    }

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
