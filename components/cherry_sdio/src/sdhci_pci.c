/*
 * CherrySDIO - PCIe SDHCI Driver Binding
 */

#include <stdint.h>
#include <stdbool.h>
#include "platform.h"
#include "cherry_pci.h"
#include "cherry_sdio.h"
#include "sdhci.h"
#include "cherry_printk.h"

static struct sdhci_host s_primary_host;
static bool s_primary_host_active = false;

extern int  sd_card_detect_and_init(struct sdhci_host *host);
extern void sd_block_register(struct sdhci_host *host);

static void sdhci_isr_trampoline(void)
{
    sdhci_isr(&s_primary_host);
}

static int sdhci_pci_probe(struct pci_dev *dev, const struct pci_device_id *id)
{
    (void)id;
    pr_info("cherry_sdio: probing pcie sdhci controller at [%02x:%02x.%d]...\n",
            dev->bus, dev->dev, dev->fn);

    /* Enable device MMIO and bus master */
    pci_enable_device(dev);
    pci_set_master(dev);

    uintptr_t mmio = (uintptr_t)dev->resource[0];
    if (mmio == 0) {
        pr_err("cherry_sdio: invalid bar0 address\n");
        return -1;
    }

    /* Enable MSI via ARM GICv3 ITS (fallback to INTx) */
    pci_enable_msi(dev, sdhci_isr_trampoline, &s_primary_host);

    /* Initialize SDHCI Host Controller */
    sdhci_init(&s_primary_host, mmio, dev->irq, dev->msi_enabled);
    s_primary_host.pci_dev = dev;
    dev->drv_data = &s_primary_host;

    /* Detect and initialize inserted card */
    sd_card_detect_and_init(&s_primary_host);

    /* Register Block Device with CherryVFS */
    sd_block_register(&s_primary_host);

    s_primary_host_active = true;
    pr_info("cherry_sdio: pcie sdhci device successfully bound (irq=%d, msi=%d)\n",
            dev->irq, dev->msi_enabled);
    return 0;
}

static void sdhci_pci_remove(struct pci_dev *dev)
{
    (void)dev;
    s_primary_host_active = false;
}

static const struct pci_device_id sdhci_pci_tbl[] = {
    { PCI_VENDOR_ID_REDHAT, PCI_DEVICE_ID_REDHAT_SDHCI, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0 },
    { PCI_ANY_ID, PCI_ANY_ID, PCI_ANY_ID, PCI_ANY_ID, (PCI_CLASS_SYSTEM_SDHCI << 8), 0xFFFF00, 0 },
    { 0, 0, 0, 0, 0, 0, 0 }
};

static struct pci_driver sdhci_pci_driver = {
    .name = "sdhci-pci",
    .id_table = sdhci_pci_tbl,
    .probe = sdhci_pci_probe,
    .remove = sdhci_pci_remove,
};

int sdhci_pci_register(void)
{
    return pci_register_driver(&sdhci_pci_driver);
}

struct sdhci_host *sdio_get_primary_host(void)
{
    return s_primary_host_active ? &s_primary_host : 0;
}
