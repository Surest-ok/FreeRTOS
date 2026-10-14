/*
 * CherryPCI - PCIe Bus Enumeration, Resource Allocation & Management
 */

#include <stdint.h>
#include <stdbool.h>
#include "platform.h"
#include "cherry_pci.h"
#include "cherry_printk.h"

static struct pci_dev s_devices[PCI_MAX_DEVICES];
static int s_device_count = 0;

static uint64_t s_mmio32_alloc_ptr = PCIE_MMIO32_BASE;
static const uint64_t s_mmio32_limit = PCIE_MMIO32_BASE + PCIE_MMIO32_SIZE;

static uint64_t pci_alloc_mmio32(uint64_t size)
{
    /* Align to size (minimum 4096 bytes) */
    uint64_t align = size < 4096 ? 4096 : size;
    uint64_t aligned_base = (s_mmio32_alloc_ptr + align - 1) & ~(align - 1);

    if (aligned_base + size > s_mmio32_limit) {
        pr_err("cherry_pci: out of mmio32 address space!\n");
        return 0;
    }

    s_mmio32_alloc_ptr = aligned_base + size;
    return aligned_base;
}

static void pci_parse_capabilities(struct pci_dev *dev)
{
    uint16_t status = pci_read_config_word(dev, PCI_STATUS);
    if (!(status & PCI_STATUS_CAP_LIST)) {
        return;
    }

    uint8_t pos = pci_read_config_byte(dev, PCI_CAPABILITY_LIST) & ~0x03;
    while (pos != 0 && pos < 0xFC) {
        uint8_t cap_id = pci_read_config_byte(dev, pos);
        uint8_t next_pos = pci_read_config_byte(dev, pos + 1) & ~0x03;

        if (cap_id == PCI_CAP_ID_MSI && dev->msi_cap == 0) {
            dev->msi_cap = pos;
        } else if (cap_id == PCI_CAP_ID_MSIX && dev->msix_cap == 0) {
            dev->msix_cap = pos;
        }

        pos = next_pos;
    }
}

static void pci_probe_bar(struct pci_dev *dev, int bar_idx)
{
    int reg = PCI_BASE_ADDRESS_0 + (bar_idx * 4);
    uint32_t orig_val = pci_read_config_dword(dev, reg);

    /* Write all 1s to discover size */
    pci_write_config_dword(dev, reg, 0xFFFFFFFF);
    uint32_t mask = pci_read_config_dword(dev, reg);

    /* Restore original */
    pci_write_config_dword(dev, reg, orig_val);

    if (mask == 0 || mask == 0xFFFFFFFF) {
        return;
    }

    /* Check if IO or Memory */
    if (mask & PCI_BASE_ADDRESS_SPACE_IO) {
        dev->resource_flags[bar_idx] = PCI_BASE_ADDRESS_SPACE_IO;
        uint32_t size = ~(mask & PCI_BASE_ADDRESS_IO_MASK) + 1;
        dev->resource_len[bar_idx] = size;
        dev->resource[bar_idx] = orig_val & PCI_BASE_ADDRESS_IO_MASK;
    } else {
        dev->resource_flags[bar_idx] = PCI_BASE_ADDRESS_SPACE_MEMORY;
        uint32_t size = ~(mask & PCI_BASE_ADDRESS_MEM_MASK) + 1;
        dev->resource_len[bar_idx] = size;

        uint64_t base_addr = orig_val & PCI_BASE_ADDRESS_MEM_MASK;
        if (base_addr == 0) {
            /* Dynamically assign MMIO window if unassigned */
            base_addr = pci_alloc_mmio32(size);
            pci_write_config_dword(dev, reg, (uint32_t)base_addr);
        }
        dev->resource[bar_idx] = base_addr;
    }
}

int pci_scan_bus(void)
{
    s_device_count = 0;
    pr_info("cherry_pci: starting pcie bus scan...\n");

    for (uint32_t bus = 0; bus < 1; bus++) {
        for (uint32_t dev = 0; dev < 32; dev++) {
            uint32_t bdf = (bus << 8) | (dev << 3);
            uint16_t vendor = pci_raw_read16(bdf, PCI_VENDOR_ID);
            if (vendor == 0xFFFF || vendor == 0) {
                continue;
            }

            uint8_t header_type = pci_raw_read8(bdf, PCI_HEADER_TYPE);
            uint8_t max_fn = (header_type & PCI_HEADER_TYPE_MFD) ? 8 : 1;

            for (uint32_t fn = 0; fn < max_fn; fn++) {
                uint32_t fn_bdf = (bus << 8) | (dev << 3) | fn;
                uint16_t fn_vid = pci_raw_read16(fn_bdf, PCI_VENDOR_ID);
                if (fn_vid == 0xFFFF || fn_vid == 0) {
                    continue;
                }

                if (s_device_count >= PCI_MAX_DEVICES) {
                    pr_warn("cherry_pci: device table full\n");
                    break;
                }

                struct pci_dev *pdev = &s_devices[s_device_count++];
                pdev->bdf = fn_bdf;
                pdev->bus = (uint8_t)bus;
                pdev->dev = (uint8_t)dev;
                pdev->fn  = (uint8_t)fn;
                pdev->vendor = fn_vid;
                pdev->device = pci_raw_read16(fn_bdf, PCI_DEVICE_ID);
                pdev->class_code = pci_raw_read32(fn_bdf, PCI_CLASS_REVISION) >> 8;
                pdev->revision = pci_raw_read8(fn_bdf, PCI_REVISION_ID);
                pdev->header_type = header_type & ~PCI_HEADER_TYPE_MFD;

                /* Probe BARs */
                for (int b = 0; b < PCI_BAR_COUNT; b++) {
                    pci_probe_bar(pdev, b);
                }

                /* Probe Capabilities */
                pci_parse_capabilities(pdev);

                /* Read Interrupt Pin & Line */
                pdev->irq_pin = pci_read_config_byte(pdev, PCI_INTERRUPT_PIN);
                pdev->irq_line = pci_read_config_byte(pdev, PCI_INTERRUPT_LINE);

                /* Default QEMU virt legacy INTx mapping: SPI 35..38 */
                if (pdev->irq_pin > 0) {
                    pdev->irq = 32 + 3 + (pdev->dev + pdev->irq_pin - 1) % 4;
                } else {
                    pdev->irq = 0;
                }

                /* Enable Memory space and Bus Mastering by default */
                pci_set_master(pdev);

                pr_info("cherry_pci: [%02x:%02x.%d] id=%04x:%04x class=%06x bar0=0x%lx (size=%lu) msi_cap=%d irq=%d\n",
                        pdev->bus, pdev->dev, pdev->fn,
                        pdev->vendor, pdev->device, pdev->class_code,
                        (unsigned long)pdev->resource[0], (unsigned long)pdev->resource_len[0],
                        (int)pdev->msi_cap, (int)pdev->irq);
            }
        }
    }

    pr_info("cherry_pci: pcie bus scan completed, %d devices enumerated\n", s_device_count);
    return s_device_count;
}

int pci_get_device_count(void)
{
    return s_device_count;
}

struct pci_dev *pci_get_device_by_index(int idx)
{
    if (idx >= 0 && idx < s_device_count) {
        return &s_devices[idx];
    }
    return 0;
}

struct pci_dev *pci_get_device(uint16_t vendor, uint16_t device, struct pci_dev *from)
{
    int start = 0;
    if (from != 0) {
        for (int i = 0; i < s_device_count; i++) {
            if (&s_devices[i] == from) {
                start = i + 1;
                break;
            }
        }
    }

    for (int i = start; i < s_device_count; i++) {
        if ((vendor == PCI_VENDOR_ID_ANY || s_devices[i].vendor == vendor) &&
            (device == PCI_DEVICE_ID_ANY || s_devices[i].device == device)) {
            return &s_devices[i];
        }
    }
    return 0;
}

struct pci_dev *pci_get_class(uint32_t class_code, struct pci_dev *from)
{
    int start = 0;
    if (from != 0) {
        for (int i = 0; i < s_device_count; i++) {
            if (&s_devices[i] == from) {
                start = i + 1;
                break;
            }
        }
    }

    for (int i = start; i < s_device_count; i++) {
        /* Compare upper 16 bits of class */
        if ((s_devices[i].class_code >> 8) == (class_code >> 8)) {
            return &s_devices[i];
        }
    }
    return 0;
}

int pci_enable_device(struct pci_dev *dev)
{
    uint16_t cmd = pci_read_config_word(dev, PCI_COMMAND);
    cmd |= (PCI_COMMAND_IO | PCI_COMMAND_MEMORY);
    pci_write_config_word(dev, PCI_COMMAND, cmd);
    return 0;
}

void pci_set_master(struct pci_dev *dev)
{
    uint16_t cmd = pci_read_config_word(dev, PCI_COMMAND);
    cmd |= (PCI_COMMAND_MEMORY | PCI_COMMAND_MASTER);
    pci_write_config_word(dev, PCI_COMMAND, cmd);
}

void *pci_iomap(struct pci_dev *dev, int bar, unsigned long maxlen)
{
    (void)maxlen;
    if (bar < 0 || bar >= PCI_BAR_COUNT) {
        return 0;
    }
    return (void *)(uintptr_t)dev->resource[bar];
}
