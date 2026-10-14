/*
 * CherryPCI - BAR Probing & Resource Window Allocation (32/64-bit & Prefetchable)
 *
 * Copyright (c) 2026 ForgeStitch
 * SPDX-License-Identifier: MIT
 */

#include <stdint.h>
#include <stdbool.h>
#include "platform.h"
#include "cherry_pci.h"
#include "pci_regs.h"
#include "cherry_printk.h"

/* Resource Allocator Pointers & Limits */
static uint64_t s_mmio32_nonpref_ptr = PCIE_MMIO32_NONPREF_BASE;
static const uint64_t s_mmio32_nonpref_limit = PCIE_MMIO32_NONPREF_BASE + PCIE_MMIO32_NONPREF_SIZE;

static uint64_t s_mmio32_pref_ptr = PCIE_MMIO32_PREF_BASE;
static const uint64_t s_mmio32_pref_limit = PCIE_MMIO32_PREF_BASE + PCIE_MMIO32_PREF_SIZE;

static uint64_t s_mmio64_nonpref_ptr = PCIE_MMIO64_NONPREF_BASE;
static const uint64_t s_mmio64_nonpref_limit = PCIE_MMIO64_NONPREF_BASE + PCIE_MMIO64_NONPREF_SIZE;

static uint64_t s_mmio64_pref_ptr = PCIE_MMIO64_PREF_BASE;
static const uint64_t s_mmio64_pref_limit = PCIE_MMIO64_PREF_BASE + PCIE_MMIO64_PREF_SIZE;

static uint64_t s_io_ptr = PCIE_IO_BASE;
static const uint64_t s_io_limit = PCIE_IO_BASE + PCIE_IO_SIZE;

uint64_t pci_alloc_resource(uint64_t size, uint32_t flags)
{
    if (size == 0) return 0;

    uint64_t *ptr = NULL;
    uint64_t limit = 0;
    uint64_t min_align = 4096;

    if (flags & IORESOURCE_IO) {
        ptr = &s_io_ptr;
        limit = s_io_limit;
        min_align = 16;
    } else if (flags & IORESOURCE_MEM_64) {
        if (flags & IORESOURCE_PREFETCH) {
            ptr = &s_mmio64_pref_ptr;
            limit = s_mmio64_pref_limit;
        } else {
            ptr = &s_mmio64_nonpref_ptr;
            limit = s_mmio64_nonpref_limit;
        }
    } else {
        /* 32-bit Memory */
        if (flags & IORESOURCE_PREFETCH) {
            ptr = &s_mmio32_pref_ptr;
            limit = s_mmio32_pref_limit;
        } else {
            ptr = &s_mmio32_nonpref_ptr;
            limit = s_mmio32_nonpref_limit;
        }
    }

    uint64_t align = size < min_align ? min_align : size;
    uint64_t aligned_base = (*ptr + align - 1) & ~(align - 1);

    if (aligned_base + size > limit) {
        pr_err("cherry_pci: out of resource space for flags 0x%x (size 0x%lx)!\n",
               flags, (unsigned long)size);
        return 0;
    }

    *ptr = aligned_base + size;
    return aligned_base;
}

int pci_probe_bar(struct pci_dev *dev, int bar_idx)
{
    if (!dev || bar_idx < 0 || bar_idx >= PCI_BAR_COUNT) {
        return 0;
    }

    int reg = PCI_BASE_ADDRESS_0 + (bar_idx * 4);
    uint32_t orig_low = pci_read_config_dword(dev, reg);

    /* 1. Sizing: write all 1s to discover BAR requirement */
    pci_write_config_dword(dev, reg, 0xFFFFFFFF);
    uint32_t mask_low = pci_read_config_dword(dev, reg);
    pci_write_config_dword(dev, reg, orig_low);

    if (mask_low == 0 || mask_low == 0xFFFFFFFF) {
        dev->resource[bar_idx] = 0;
        dev->resource_len[bar_idx] = 0;
        dev->resource_flags[bar_idx] = 0;
        return 1; /* Consumed 1 register (unused/disabled BAR) */
    }

    /* 2. Check if I/O space */
    if (mask_low & PCI_BASE_ADDRESS_SPACE_IO) {
        uint32_t flags = IORESOURCE_IO;
        uint32_t size32 = (~(mask_low & 0xFFFFFFFCU) + 1U);
        uint64_t size = (uint64_t)size32;
        uint64_t base = (uint64_t)(orig_low & 0xFFFFFFFCU);

        if (base == 0) {
            base = pci_alloc_resource(size, flags);
            if (base != 0) {
                pci_write_config_dword(dev, reg, (uint32_t)base | PCI_BASE_ADDRESS_SPACE_IO);
            }
        }

        dev->resource[bar_idx] = base;
        dev->resource_len[bar_idx] = size;
        dev->resource_flags[bar_idx] = flags;
        return 1;
    }

    /* 3. Memory space: check 64-bit and prefetchable */
    bool is_64bit = ((mask_low & PCI_BASE_ADDRESS_MEM_TYPE_MASK) == PCI_BASE_ADDRESS_MEM_TYPE_64);
    bool is_pref  = ((mask_low & PCI_BASE_ADDRESS_MEM_PREFETCH) != 0);

    uint32_t flags = IORESOURCE_MEM;
    if (is_64bit) flags |= IORESOURCE_MEM_64;
    if (is_pref)  flags |= IORESOURCE_PREFETCH;

    if (!is_64bit) {
        /* Standard 32-bit MMIO BAR */
        uint32_t size32 = (~(mask_low & 0xFFFFFFF0U) + 1U);
        uint64_t size = (uint64_t)size32;
        uint64_t base = (uint64_t)(orig_low & 0xFFFFFFF0U);

        if (base == 0) {
            base = pci_alloc_resource(size, flags);
            if (base != 0) {
                uint32_t cfg_val = (uint32_t)base | (orig_low & 0x0FU);
                pci_write_config_dword(dev, reg, cfg_val);
            }
        }

        dev->resource[bar_idx] = base;
        dev->resource_len[bar_idx] = size;
        dev->resource_flags[bar_idx] = flags;
        return 1;
    }

    /* 4. 64-bit MMIO BAR: spans across reg and reg+4 */
    if (bar_idx + 1 >= PCI_BAR_COUNT) {
        pr_warn("cherry_pci: [%02x:%02x.%d] 64-bit BAR at last index %d\n",
                dev->bus, dev->dev, dev->fn, bar_idx);
        return 1;
    }

    int reg_hi = reg + 4;
    uint32_t orig_hi = pci_read_config_dword(dev, reg_hi);

    pci_write_config_dword(dev, reg_hi, 0xFFFFFFFF);
    uint32_t mask_hi = pci_read_config_dword(dev, reg_hi);
    pci_write_config_dword(dev, reg_hi, orig_hi);

    uint64_t mask64 = (((uint64_t)mask_hi) << 32) | ((uint64_t)(mask_low & 0xFFFFFFF0U));
    uint64_t size64 = ~mask64 + 1ULL;
    uint64_t base64 = (((uint64_t)orig_hi) << 32) | ((uint64_t)(orig_low & 0xFFFFFFF0U));

    if (base64 == 0) {
        base64 = pci_alloc_resource(size64, flags);
        if (base64 != 0) {
            uint32_t low_cfg = ((uint32_t)base64 & 0xFFFFFFF0U) | (orig_low & 0x0FU);
            uint32_t hi_cfg  = (uint32_t)(base64 >> 32);

            pci_write_config_dword(dev, reg, low_cfg);
            pci_write_config_dword(dev, reg_hi, hi_cfg);
        }
    }

    dev->resource[bar_idx] = base64;
    dev->resource_len[bar_idx] = size64;
    dev->resource_flags[bar_idx] = flags;

    /* Mark high half as secondary */
    dev->resource[bar_idx + 1] = 0;
    dev->resource_len[bar_idx + 1] = 0;
    dev->resource_flags[bar_idx + 1] = flags | IORESOURCE_MEM_64;

    return 2; /* Consumed 2 registers */
}

int pci_enable_device(struct pci_dev *dev)
{
    if (!dev) return -1;

    uint16_t cmd = pci_read_config_word(dev, PCI_COMMAND);
    cmd |= (PCI_COMMAND_IO | PCI_COMMAND_MEMORY);
    pci_write_config_word(dev, PCI_COMMAND, cmd);

    dev->is_enabled = true;
    return 0;
}

void pci_disable_device(struct pci_dev *dev)
{
    if (!dev) return;

    uint16_t cmd = pci_read_config_word(dev, PCI_COMMAND);
    cmd &= ~(PCI_COMMAND_IO | PCI_COMMAND_MEMORY);
    pci_write_config_word(dev, PCI_COMMAND, cmd);

    dev->is_enabled = false;
}

void pci_set_master(struct pci_dev *dev)
{
    if (!dev) return;

    uint16_t cmd = pci_read_config_word(dev, PCI_COMMAND);
    cmd |= PCI_COMMAND_MASTER;
    pci_write_config_word(dev, PCI_COMMAND, cmd);

    dev->is_bus_master = true;
}

void pci_clear_master(struct pci_dev *dev)
{
    if (!dev) return;

    uint16_t cmd = pci_read_config_word(dev, PCI_COMMAND);
    cmd &= ~PCI_COMMAND_MASTER;
    pci_write_config_word(dev, PCI_COMMAND, cmd);

    dev->is_bus_master = false;
}

void *pci_iomap(struct pci_dev *dev, int bar, unsigned long maxlen)
{
    (void)maxlen;
    if (!dev || bar < 0 || bar >= PCI_BAR_COUNT) {
        return NULL;
    }
    return (void *)(uintptr_t)dev->resource[bar];
}

void pci_iounmap(struct pci_dev *dev, void *addr)
{
    (void)dev;
    (void)addr;
}
