/*
 * CherryPCI - Acceptance & Verification Test Suite
 *
 * Copyright (c) 2026 ForgeStitch
 * SPDX-License-Identifier: MIT
 */

#include <stdint.h>
#include <stdbool.h>
#include "platform.h"
#include "cherry_pci.h"
#include "cherry_its.h"
#include "cherry_printk.h"

static bool s_test_probe_called = false;
static bool s_test_remove_called = false;

static int test_driver_probe(struct pci_dev *dev, const struct pci_device_id *id)
{
    (void)id;
    s_test_probe_called = true;
    pci_set_drvdata(dev, (void *)0xDEADBEEF);
    return 0;
}

static void test_driver_remove(struct pci_dev *dev)
{
    (void)dev;
    s_test_remove_called = true;
}

static const struct pci_device_id s_test_pci_ids[] = {
    { PCI_VENDOR_ID_REDHAT, 0x0008, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0 }, /* Host bridge */
    { 0, 0, 0, 0, 0, 0, 0 }
};

static struct pci_driver s_test_pci_driver = {
    .name = "test-host-bridge",
    .id_table = s_test_pci_ids,
    .probe = test_driver_probe,
    .remove = test_driver_remove,
};

void cherry_pci_run_tests(void)
{
    pr_info("cherry_pci: ==> starting cherrypci acceptance test suite...\n");

    /* 1. ECAM verification */
    uint32_t bdf0 = 0;
    uint16_t v0 = pci_raw_read16(bdf0, PCI_VENDOR_ID);
    bool ecam_pass = (v0 != 0 && v0 != 0xFFFF);

    /* 2. Bus scan verification */
    int count = pci_get_device_count();
    bool bus_scan_pass = (count > 0);

    /* 3. Device detection (Host bridge or endpoints) */
    bool dev_found_pass = false;
    struct pci_dev *sd_dev = NULL;
    struct pci_dev *host_dev = NULL;

    for (int i = 0; i < count; i++) {
        struct pci_dev *p = pci_get_device_by_index(i);
        if (p != NULL) {
            if (p->vendor == PCI_VENDOR_ID_REDHAT && p->device == 0x0008) {
                host_dev = p;
            }
            if (p->vendor == PCI_VENDOR_ID_REDHAT && p->device == PCI_DEVICE_ID_REDHAT_SDHCI) {
                sd_dev = p;
            }
        }
    }
    dev_found_pass = (count >= 2);

    /* 4. Capability Traversal & Access Verification */
    bool cap_access_pass = false;
    struct pci_dev *cap_dev = NULL;
    for (int i = 0; i < count; i++) {
        struct pci_dev *p = pci_get_device_by_index(i);
        if (p != NULL && (pci_read_config_word(p, PCI_STATUS) & PCI_STATUS_CAP_LIST)) {
            cap_dev = p;
            break;
        }
    }

    if (cap_dev != NULL) {
        int cap_msix = pci_find_capability(cap_dev, PCI_CAP_ID_MSIX);
        int cap_vndr = pci_find_capability(cap_dev, PCI_CAP_ID_VNDR);
        int cap_nonexist = pci_find_capability(cap_dev, 0xFE); /* Must return 0 */

        if ((cap_msix > 0 || cap_vndr > 0) && cap_nonexist == 0) {
            uint32_t cap_val = 0;
            int target_cap = (cap_msix > 0) ? PCI_CAP_ID_MSIX : PCI_CAP_ID_VNDR;
            if (pci_read_cap_dword(cap_dev, target_cap, 0, &cap_val) == 0 && cap_val != 0) {
                cap_access_pass = true;
            }
        }
    }

    /* 5. BAR Allocation Verification (32-bit, 64-bit, and Prefetchable windows) */
    bool bar_alloc_pass = true;

    /* Verify device hardware BAR (SDHCI controller BAR0) */
    if (sd_dev != NULL) {
        if (pci_resource_start(sd_dev, 0) == 0 || pci_resource_len(sd_dev, 0) == 0) {
            bar_alloc_pass = false;
        }
    }

    /* Test 32-bit Non-prefetchable allocator */
    uint64_t a32 = pci_alloc_resource(0x1000, IORESOURCE_MEM);
    if (a32 < PCIE_MMIO32_NONPREF_BASE ||
        a32 + 0x1000 > PCIE_MMIO32_NONPREF_BASE + PCIE_MMIO32_NONPREF_SIZE) {
        bar_alloc_pass = false;
    }

    /* Test 32-bit Prefetchable allocator */
    uint64_t a32_p = pci_alloc_resource(0x2000, IORESOURCE_MEM | IORESOURCE_PREFETCH);
    if (a32_p < PCIE_MMIO32_PREF_BASE ||
        a32_p + 0x2000 > PCIE_MMIO32_PREF_BASE + PCIE_MMIO32_PREF_SIZE) {
        bar_alloc_pass = false;
    }

    /* Test 64-bit Non-prefetchable allocator */
    uint64_t a64 = pci_alloc_resource(0x4000, IORESOURCE_MEM | IORESOURCE_MEM_64);
    if (a64 < PCIE_MMIO64_NONPREF_BASE ||
        a64 + 0x4000 > PCIE_MMIO64_NONPREF_BASE + PCIE_MMIO64_NONPREF_SIZE) {
        bar_alloc_pass = false;
    }

    /* Test 64-bit Prefetchable ("perf") allocator */
    uint64_t a64_p = pci_alloc_resource(0x8000, IORESOURCE_MEM | IORESOURCE_MEM_64 | IORESOURCE_PREFETCH);
    if (a64_p < PCIE_MMIO64_PREF_BASE ||
        a64_p + 0x8000 > PCIE_MMIO64_PREF_BASE + PCIE_MMIO64_PREF_SIZE) {
        bar_alloc_pass = false;
    }

    /* Test I/O Space allocator */
    uint64_t a_io = pci_alloc_resource(0x20, IORESOURCE_IO);
    if (a_io < PCIE_IO_BASE || a_io + 0x20 > PCIE_IO_BASE + PCIE_IO_SIZE) {
        bar_alloc_pass = false;
    }

    /* 6. Driver Registration, Matching & Device Binding Verification */
    bool driver_bind_pass = false;
    if (host_dev != NULL) {
        s_test_probe_called = false;
        s_test_remove_called = false;

        pci_register_driver(&s_test_pci_driver);

        if (s_test_probe_called &&
            host_dev->driver == &s_test_pci_driver &&
            pci_get_drvdata(host_dev) == (void *)0xDEADBEEF) {

            /* Test unregistration */
            pci_unregister_driver(&s_test_pci_driver);
            if (s_test_remove_called && host_dev->driver == NULL) {
                driver_bind_pass = true;
            }
        }
    }

    /* 7. Interrupt Allocation Verification (INTx & MSI vectors) */
    bool irq_alloc_pass = false;
    if (sd_dev != NULL) {
        int vecs = pci_alloc_irq_vectors(sd_dev, 1, 1, PCI_IRQ_MSI | PCI_IRQ_LEGACY);
        if (vecs > 0 && sd_dev->irq != 0) {
            irq_alloc_pass = true;
        }
    }

    /* 8. GIC ITS / MSI engine availability */
    bool its_pass = its_is_available();

    bool all_pass = (ecam_pass && bus_scan_pass && dev_found_pass &&
                     cap_access_pass && bar_alloc_pass && driver_bind_pass &&
                     irq_alloc_pass && its_pass);

    pr_info("cherry_pci: [pci-verify] ecam=%s bus_scan=%s dev_found=%s cap_access=%s bar_alloc=%s driver_bind=%s irq_alloc=%s its_msi=%s all=%s\n",
            ecam_pass ? "pass" : "fail",
            bus_scan_pass ? "pass" : "fail",
            dev_found_pass ? "pass" : "fail",
            cap_access_pass ? "pass" : "fail",
            bar_alloc_pass ? "pass" : "fail",
            driver_bind_pass ? "pass" : "fail",
            irq_alloc_pass ? "pass" : "fail",
            its_pass ? "pass" : "fail",
            all_pass ? "pass" : "fail");
}
