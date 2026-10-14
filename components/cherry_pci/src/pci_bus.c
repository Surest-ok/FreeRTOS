/*
 * CherryPCI - PCIe Bus Enumeration, Device Management & Discovery
 *
 * Copyright (c) 2026 ForgeStitch
 * SPDX-License-Identifier: MIT
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "platform.h"
#include "cherry_pci.h"
#include "cherry_printk.h"

static struct pci_dev s_devices[PCI_MAX_DEVICES];
static int s_device_count = 0;

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
                    pr_warn("cherry_pci: device table full (max %d)\n", PCI_MAX_DEVICES);
                    break;
                }

                struct pci_dev *pdev = &s_devices[s_device_count++];
                memset(pdev, 0, sizeof(*pdev));

                pdev->bdf = fn_bdf;
                pdev->bus = (uint8_t)bus;
                pdev->dev = (uint8_t)dev;
                pdev->fn  = (uint8_t)fn;
                pdev->vendor = fn_vid;
                pdev->device = pci_raw_read16(fn_bdf, PCI_DEVICE_ID);
                pdev->subsystem_vendor = pci_raw_read16(fn_bdf, PCI_SUBSYSTEM_VENDOR_ID);
                pdev->subsystem_device = pci_raw_read16(fn_bdf, PCI_SUBSYSTEM_ID);
                pdev->class_code = pci_raw_read32(fn_bdf, PCI_CLASS_REVISION) >> 8;
                pdev->revision = pci_raw_read8(fn_bdf, PCI_REVISION_ID);
                pdev->header_type = header_type & ~PCI_HEADER_TYPE_MFD;

                /* 1. Probe & Allocate BARs (handles 32, 64-bit, pref & non-pref) */
                for (int b = 0; b < PCI_BAR_COUNT; ) {
                    int consumed = pci_probe_bar(pdev, b);
                    if (consumed <= 0) {
                        b++;
                    } else {
                        b += consumed;
                    }
                }

                /* 2. Traverse and parse Capabilities */
                pci_parse_capabilities(pdev);

                /* 3. Assign Default Interrupt Line/Pin */
                pci_assign_irq(pdev);

                /* 4. Default enable Bus Mastering */
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
    return NULL;
}

struct pci_dev *pci_get_device(uint16_t vendor, uint16_t device, struct pci_dev *from)
{
    int start = 0;
    if (from != NULL) {
        for (int i = 0; i < s_device_count; i++) {
            if (&s_devices[i] == from) {
                start = i + 1;
                break;
            }
        }
    }

    for (int i = start; i < s_device_count; i++) {
        if ((vendor == (uint16_t)PCI_ANY_ID || s_devices[i].vendor == vendor) &&
            (device == (uint16_t)PCI_ANY_ID || s_devices[i].device == device)) {
            return &s_devices[i];
        }
    }
    return NULL;
}

struct pci_dev *pci_get_class(uint32_t class_code, struct pci_dev *from)
{
    int start = 0;
    if (from != NULL) {
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
    return NULL;
}
