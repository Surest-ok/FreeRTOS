/*
 * CherryPCI - Modern Linux-style PCIe Subsystem for CherryOS / FreeRTOS
 *
 * Decoupled subsystem providing:
 *   - PCIe ECAM host bridge configuration access
 *   - Device and bridge discovery and 32/64/Prefetchable BAR allocation
 *   - Capability list traversal (Standard PCI & PCIe Extended)
 *   - ARM GICv3 ITS (Interrupt Translation Service) MSI / MSI-X & INTx allocation
 *   - Linux-style struct pci_driver registration and automatic probe/binding
 *   - CherryShell diagnostics and inspection commands
 *
 * Copyright (c) 2026 ForgeStitch
 * SPDX-License-Identifier: MIT
 */

#ifndef CHERRY_PCI_H
#define CHERRY_PCI_H

#include <stdint.h>
#include <stdbool.h>
#include "pci_regs.h"
#include "pci_ids.h"
#include "cherry_its.h"
#include "app_irq.h"

#define PCI_MAX_DEVICES 32
#define PCI_MAX_DRIVERS 16

struct pci_driver;

/* PCI Device representation */
struct pci_dev {
    uint32_t bdf;                   /* (bus << 8) | (dev << 3) | fn */
    uint8_t  bus;
    uint8_t  dev;
    uint8_t  fn;

    uint16_t vendor;
    uint16_t device;
    uint16_t subsystem_vendor;
    uint16_t subsystem_device;
    uint32_t class_code;            /* class (24..8) | prog_if (7..0) */
    uint8_t  revision;
    uint8_t  header_type;

    /* Base Address Registers (BAR0 .. BAR5) */
    uint64_t resource[PCI_BAR_COUNT];
    uint64_t resource_len[PCI_BAR_COUNT];
    uint32_t resource_flags[PCI_BAR_COUNT];

    /* Legacy interrupt */
    uint8_t  irq_line;
    uint8_t  irq_pin;

    /* Cached Capabilities */
    uint8_t  pm_cap;                /* Power Management Cap offset */
    uint8_t  msi_cap;               /* MSI Cap offset */
    uint8_t  msix_cap;              /* MSI-X Cap offset */
    uint8_t  pcie_cap;              /* PCI Express Cap offset */
    uint16_t aer_cap;               /* Extended AER Cap offset */

    bool     msi_enabled;
    uint32_t msi_irq;               /* LPI assigned if MSI enabled */

    /* Active IRQ (MSI LPI or INTx SPI) */
    uint32_t irq;

    /* Device lifecycle state */
    bool     is_enabled;
    bool     is_bus_master;

    /* Bound driver and driver-specific context */
    struct pci_driver *driver;
    void              *drv_data;
};

/* Device ID matching structure */
struct pci_device_id {
    uint32_t vendor;
    uint32_t device;
    uint32_t subvendor;
    uint32_t subdevice;
    uint32_t class_code;
    uint32_t class_mask;
    unsigned long driver_data;
};

/* PCI Driver structure */
struct pci_driver {
    const char *name;
    const struct pci_device_id *id_table;
    int (*probe)(struct pci_dev *dev, const struct pci_device_id *id);
    void (*remove)(struct pci_dev *dev);
};

/* Subsystem Management */
int  cherry_pci_init(void);
void cherry_pci_run_tests(void);

/* Device discovery & enumeration */
int  pci_scan_bus(void);
int  pci_get_device_count(void);
struct pci_dev *pci_get_device_by_index(int idx);
struct pci_dev *pci_get_device(uint16_t vendor, uint16_t device, struct pci_dev *from);
struct pci_dev *pci_get_class(uint32_t class_code, struct pci_dev *from);

/* Driver registration & device binding */
int  pci_register_driver(struct pci_driver *driver);
void pci_unregister_driver(struct pci_driver *driver);
const struct pci_device_id *pci_match_device(struct pci_driver *drv, struct pci_dev *dev);

/* Capability traversal & access */
int  pci_find_capability(struct pci_dev *dev, int cap);
int  pci_find_ext_capability(struct pci_dev *dev, int cap);
int  pci_read_cap_dword(struct pci_dev *dev, int cap, int offset, uint32_t *val);
int  pci_write_cap_dword(struct pci_dev *dev, int cap, int offset, uint32_t val);
void pci_parse_capabilities(struct pci_dev *dev);

/* BAR & Resource Allocation (32-bit, 64-bit, and Prefetchable/Non-prefetchable) */
uint64_t pci_alloc_resource(uint64_t size, uint32_t flags);
int      pci_probe_bar(struct pci_dev *dev, int bar_idx);

/* Device Configuration & Control */
int   pci_enable_device(struct pci_dev *dev);
void  pci_disable_device(struct pci_dev *dev);
void  pci_set_master(struct pci_dev *dev);
void  pci_clear_master(struct pci_dev *dev);
void *pci_iomap(struct pci_dev *dev, int bar, unsigned long maxlen);
void  pci_iounmap(struct pci_dev *dev, void *addr);

/* Interrupt configuration & allocation */
int  pci_assign_irq(struct pci_dev *dev);
int  pci_enable_msi(struct pci_dev *dev, isr_fn_t handler, void *arg);
int  pci_disable_msi(struct pci_dev *dev);
int  pci_alloc_irq_vectors(struct pci_dev *dev, unsigned int min_vecs, unsigned int max_vecs, unsigned int flags);
void pci_free_irq_vectors(struct pci_dev *dev);

/* Config space read/write helpers */
uint8_t  pci_read_config_byte(struct pci_dev *dev, int where);
uint16_t pci_read_config_word(struct pci_dev *dev, int where);
uint32_t pci_read_config_dword(struct pci_dev *dev, int where);
void     pci_write_config_byte(struct pci_dev *dev, int where, uint8_t val);
void     pci_write_config_word(struct pci_dev *dev, int where, uint16_t val);
void     pci_write_config_dword(struct pci_dev *dev, int where, uint32_t val);

/* Low-level raw BDF config access */
uint32_t pci_raw_read32(uint32_t bdf, int where);
uint16_t pci_raw_read16(uint32_t bdf, int where);
uint8_t  pci_raw_read8(uint32_t bdf, int where);
void     pci_raw_write32(uint32_t bdf, int where, uint32_t val);
void     pci_raw_write16(uint32_t bdf, int where, uint16_t val);
void     pci_raw_write8(uint32_t bdf, int where, uint8_t val);

/* Resource access helpers */
static inline uint64_t pci_resource_start(const struct pci_dev *dev, int bar)
{
    return (bar >= 0 && bar < PCI_BAR_COUNT) ? dev->resource[bar] : 0;
}

static inline uint64_t pci_resource_len(const struct pci_dev *dev, int bar)
{
    return (bar >= 0 && bar < PCI_BAR_COUNT) ? dev->resource_len[bar] : 0;
}

static inline uint64_t pci_resource_end(const struct pci_dev *dev, int bar)
{
    uint64_t len = pci_resource_len(dev, bar);
    return len ? (dev->resource[bar] + len - 1) : 0;
}

static inline uint32_t pci_resource_flags(const struct pci_dev *dev, int bar)
{
    return (bar >= 0 && bar < PCI_BAR_COUNT) ? dev->resource_flags[bar] : 0;
}

static inline void pci_set_drvdata(struct pci_dev *pdev, void *data)
{
    if (pdev) {
        pdev->drv_data = data;
    }
}

static inline void *pci_get_drvdata(const struct pci_dev *pdev)
{
    return pdev ? pdev->drv_data : 0;
}

#endif /* CHERRY_PCI_H */
