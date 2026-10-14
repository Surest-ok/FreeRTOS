/*
 * CherryPCI - Modern Linux-style PCIe Subsystem for CherryOS / FreeRTOS
 *
 * Decoupled subsystem providing:
 *   - PCIe ECAM host bridge configuration access
 *   - Device and bridge discovery and BAR allocation
 *   - ARM GICv3 ITS (Interrupt Translation Service) MSI / MSI-X allocation
 *   - Linux-style struct pci_driver registration and automatic probe
 *   - CherryShell diagnostics and inspection commands
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

    /* Base Address Registers */
    uint64_t resource[PCI_BAR_COUNT];
    uint64_t resource_len[PCI_BAR_COUNT];
    uint32_t resource_flags[PCI_BAR_COUNT];

    /* Legacy interrupt */
    uint8_t  irq_line;
    uint8_t  irq_pin;

    /* Capabilities */
    uint8_t  msi_cap;
    uint8_t  msix_cap;
    bool     msi_enabled;
    uint32_t msi_irq;               /* LPI assigned if MSI enabled */

    /* Active IRQ (MSI LPI or INTx SPI) */
    uint32_t irq;

    /* Driver private context */
    void    *drv_data;
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

/* Device discovery */
int  pci_scan_bus(void);
int  pci_get_device_count(void);
struct pci_dev *pci_get_device_by_index(int idx);
struct pci_dev *pci_get_device(uint16_t vendor, uint16_t device, struct pci_dev *from);
struct pci_dev *pci_get_class(uint32_t class_code, struct pci_dev *from);

/* Device Configuration & Control */
int  pci_enable_device(struct pci_dev *dev);
void pci_set_master(struct pci_dev *dev);
void *pci_iomap(struct pci_dev *dev, int bar, unsigned long maxlen);

/* Interrupt configuration */
int  pci_enable_msi(struct pci_dev *dev, isr_fn_t handler, void *arg);

/* Driver registration */
int  pci_register_driver(struct pci_driver *driver);
void pci_unregister_driver(struct pci_driver *driver);

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

#endif /* CHERRY_PCI_H */
