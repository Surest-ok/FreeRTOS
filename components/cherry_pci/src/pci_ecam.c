/*
 * CherryPCI - PCIe ECAM (Enhanced Configuration Access Mechanism) Host Driver
 */

#include <stdint.h>
#include <stdbool.h>
#include "platform.h"
#include "cherry_pci.h"
#include "cherry_printk.h"

static uintptr_t s_ecam_base = 0;
static bool s_ecam_initialized = false;

static void ecam_detect_base(void)
{
    if (s_ecam_initialized) {
        return;
    }

    /* Probe high ECAM (QEMU virt default on AArch64) */
    uintptr_t candidate_high = (uintptr_t)PCIE_ECAM_BASE_HIGH;
    uintptr_t candidate_low  = (uintptr_t)PCIE_ECAM_BASE_LOW;

    /* Check if devices exist at high ECAM (or valid Host Bridge / endpoint) */
    bool found_high = false;
    for (int d = 0; d < 32; d++) {
        uintptr_t reg = candidate_high + ((uintptr_t)d << 15);
        uint32_t val = *(volatile uint32_t *)reg;
        uint16_t vid = (uint16_t)(val & 0xFFFF);
        if (vid != 0xFFFF && vid != 0) {
            found_high = true;
            break;
        }
    }

    if (found_high) {
        s_ecam_base = candidate_high;
    } else {
        s_ecam_base = candidate_low;
    }

    s_ecam_initialized = true;
    pr_info("cherry_pci: pcie ecam host bridge detected at 0x%lx\n", (unsigned long)s_ecam_base);
}

static inline uintptr_t ecam_get_addr(uint32_t bdf, int where)
{
    if (!s_ecam_initialized) {
        ecam_detect_base();
    }
    uint32_t bus = (bdf >> 8) & 0xFF;
    uint32_t dev = (bdf >> 3) & 0x1F;
    uint32_t fn  = bdf & 0x07;

    return s_ecam_base + ((((uintptr_t)bus) << 20)
                       | (((uintptr_t)dev) << 15)
                       | (((uintptr_t)fn)  << 12)
                       | ((uintptr_t)(where & 0xFFF)));
}

uint32_t pci_raw_read32(uint32_t bdf, int where)
{
    uintptr_t addr = ecam_get_addr(bdf, where);
    return *(volatile uint32_t *)addr;
}

uint16_t pci_raw_read16(uint32_t bdf, int where)
{
    uintptr_t addr = ecam_get_addr(bdf, where);
    return *(volatile uint16_t *)addr;
}

uint8_t pci_raw_read8(uint32_t bdf, int where)
{
    uintptr_t addr = ecam_get_addr(bdf, where);
    return *(volatile uint8_t *)addr;
}

void pci_raw_write32(uint32_t bdf, int where, uint32_t val)
{
    uintptr_t addr = ecam_get_addr(bdf, where);
    *(volatile uint32_t *)addr = val;
}

void pci_raw_write16(uint32_t bdf, int where, uint16_t val)
{
    uintptr_t addr = ecam_get_addr(bdf, where);
    *(volatile uint16_t *)addr = val;
}

void pci_raw_write8(uint32_t bdf, int where, uint8_t val)
{
    uintptr_t addr = ecam_get_addr(bdf, where);
    *(volatile uint8_t *)addr = val;
}

uint8_t pci_read_config_byte(struct pci_dev *dev, int where)
{
    return pci_raw_read8(dev->bdf, where);
}

uint16_t pci_read_config_word(struct pci_dev *dev, int where)
{
    return pci_raw_read16(dev->bdf, where);
}

uint32_t pci_read_config_dword(struct pci_dev *dev, int where)
{
    return pci_raw_read32(dev->bdf, where);
}

void pci_write_config_byte(struct pci_dev *dev, int where, uint8_t val)
{
    pci_raw_write8(dev->bdf, where, val);
}

void pci_write_config_word(struct pci_dev *dev, int where, uint16_t val)
{
    pci_raw_write16(dev->bdf, where, val);
}

void pci_write_config_dword(struct pci_dev *dev, int where, uint32_t val)
{
    pci_raw_write32(dev->bdf, where, val);
}
