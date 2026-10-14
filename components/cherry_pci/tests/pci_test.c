/*
 * CherryPCI - Acceptance Verification Test Suite
 */

#include <stdint.h>
#include <stdbool.h>
#include "cherry_pci.h"
#include "cherry_its.h"
#include "cherry_printk.h"

void cherry_pci_run_tests(void)
{
    pr_info("cherry_pci: ==> starting cherrypci acceptance test suite...\n");

    /* 1. ECAM verification */
    uint32_t bdf0 = 0;
    uint16_t v0 = pci_raw_read16(bdf0, PCI_VENDOR_ID);
    bool ecam_pass = (v0 != 0 && v0 != 0x0001);

    /* 2. Bus scan verification */
    int count = pci_get_device_count();
    bool bus_scan_pass = (count >= 0);

    /* 3. Device detection (Host bridge or endpoints) */
    bool dev_found_pass = (count > 0);

    /* 4. BAR allocation verification (check memory BARs) */
    bool bar_alloc_pass = true;
    for (int i = 0; i < count; i++) {
        struct pci_dev *p = pci_get_device_by_index(i);
        if (p != 0 && (p->resource_flags[0] == PCI_BASE_ADDRESS_SPACE_MEMORY) && p->resource_len[0] > 0) {
            if (p->resource[0] == 0) {
                bar_alloc_pass = false;
            }
        }
    }

    /* 5. GIC ITS / MSI engine verification */
    bool its_pass = its_is_available();

    /* Standard assertion probe */
    pr_info("cherry_pci: [pci-verify] ecam=%s bus_scan=%s dev_found=%s bar_alloc=%s its_msi=%s all=%s\n",
            ecam_pass ? "pass" : "fail",
            bus_scan_pass ? "pass" : "fail",
            dev_found_pass ? "pass" : "fail",
            bar_alloc_pass ? "pass" : "fail",
            its_pass ? "pass" : "fail",
            (ecam_pass && bus_scan_pass && dev_found_pass && bar_alloc_pass && its_pass) ? "pass" : "fail");
}
