/*
 * CherryPCI - Common Vendor IDs, Device IDs, and Class Codes
 */

#ifndef CHERRY_PCI_IDS_H
#define CHERRY_PCI_IDS_H

/* Common Vendor IDs */
#define PCI_VENDOR_ID_REDHAT            0x1B36
#define PCI_VENDOR_ID_QEMU              0x1234
#define PCI_VENDOR_ID_VIRTIO            0x1AF4
#define PCI_VENDOR_ID_INTEL             0x8086
#define PCI_VENDOR_ID_ANY               0xFFFF

#define PCI_DEVICE_ID_ANY               0xFFFF

/* QEMU / Red Hat Device IDs */
#define PCI_DEVICE_ID_REDHAT_SDHCI      0x0007  /* QEMU PCI-SDHCI Controller */
#define PCI_DEVICE_ID_REDHAT_PCIE_HOST  0x0008  /* QEMU PCIe Host Bridge */

/* Device Classes */
#define PCI_CLASS_STORAGE_SCSI          0x0100
#define PCI_CLASS_STORAGE_IDE           0x0101
#define PCI_CLASS_STORAGE_SATA          0x0106
#define PCI_CLASS_STORAGE_NVME          0x0108
#define PCI_CLASS_NETWORK_ETHERNET      0x0200
#define PCI_CLASS_SYSTEM_SDHCI          0x0805  /* Generic SDHCI */

#define PCI_ANY_ID (~0U)

#endif /* CHERRY_PCI_IDS_H */
