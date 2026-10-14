/*
 * CherryPCI - CherryShell Diagnostics Commands (pci_ls, pci_res, pci_cap, its_info)
 *
 * Copyright (c) 2026 ForgeStitch
 * SPDX-License-Identifier: MIT
 */

#include <stdint.h>
#include <stdbool.h>
#include "uart.h"
#include "cherry_pci.h"
#include "cherry_its.h"
#include "cherry_shell.h"
#include "cherry_printk.h"

int cmd_pci_ls(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    int total = pci_get_device_count();
    printk("=== cherry_pci: %d pci devices enumerated ===\n", total);
    printk("  %-7s  %-9s  %-6s  %-10s  %-12s  %-4s  %s\n",
           "BDF", "Vendor:Dev", "Class", "Driver", "BAR0 Base", "IRQ", "Mode");
    printk("  ----------------------------------------------------------------------\n");

    for (int i = 0; i < total; i++) {
        struct pci_dev *pdev = pci_get_device_by_index(i);
        if (pdev == NULL) continue;

        char bdf_str[16];
        snprintf(bdf_str, sizeof(bdf_str), "%02x:%02x.%d", pdev->bus, pdev->dev, pdev->fn);

        char id_str[16];
        snprintf(id_str, sizeof(id_str), "%04x:%04x", pdev->vendor, pdev->device);

        const char *drv_name = pdev->driver ? pdev->driver->name : "[none]";
        const char *mode = pdev->msi_enabled ? "msi/its" : (pdev->irq_pin ? "intx" : "none");

        printk("  %-7s  %-9s  %06x  %-10s  0x%010lx  %-4d  %s\n",
               bdf_str, id_str, pdev->class_code, drv_name,
               (unsigned long)pdev->resource[0], (int)pdev->irq, mode);
    }
    return 0;
}

int cmd_pci_res(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    int total = pci_get_device_count();
    printk("=== cherry_pci: device resource allocation table ===\n");

    for (int i = 0; i < total; i++) {
        struct pci_dev *pdev = pci_get_device_by_index(i);
        if (pdev == NULL) continue;

        printk("[%02x:%02x.%d] %04x:%04x (%s):\n",
               pdev->bus, pdev->dev, pdev->fn, pdev->vendor, pdev->device,
               pdev->driver ? pdev->driver->name : "unbound");

        for (int b = 0; b < PCI_BAR_COUNT; b++) {
            if (pdev->resource_len[b] == 0) continue;

            const char *type_str = "mmio32";
            if (pdev->resource_flags[b] & IORESOURCE_IO) {
                type_str = "io    ";
            } else if (pdev->resource_flags[b] & IORESOURCE_MEM_64) {
                type_str = (pdev->resource_flags[b] & IORESOURCE_PREFETCH) ? "pref64" : "mmio64";
            } else if (pdev->resource_flags[b] & IORESOURCE_PREFETCH) {
                type_str = "pref32";
            }

            printk("  BAR%d: [%s] base=0x%010lx size=0x%lx (%lu KB)\n",
                   b, type_str, (unsigned long)pdev->resource[b],
                   (unsigned long)pdev->resource_len[b],
                   (unsigned long)(pdev->resource_len[b] / 1024));
        }
    }
    return 0;
}

int cmd_pci_cap(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    int total = pci_get_device_count();
    printk("=== cherry_pci: capabilities summary ===\n");

    for (int i = 0; i < total; i++) {
        struct pci_dev *pdev = pci_get_device_by_index(i);
        if (pdev == NULL) continue;

        printk("[%02x:%02x.%d] caps: pm=0x%02x msi=0x%02x msix=0x%02x pcie=0x%02x aer=0x%03x\n",
               pdev->bus, pdev->dev, pdev->fn,
               pdev->pm_cap, pdev->msi_cap, pdev->msix_cap, pdev->pcie_cap, pdev->aer_cap);
    }
    return 0;
}

int cmd_its_info(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    its_dump_info();
    return 0;
}

void pci_register_shell_commands(void)
{
    cherry_shell_register_symbol("pci_ls",   (uintptr_t)cmd_pci_ls,   CHERRY_SHELL_SYM_CMD, "list all enumerated pci devices and drivers");
    cherry_shell_register_symbol("pci_res",  (uintptr_t)cmd_pci_res,  CHERRY_SHELL_SYM_CMD, "display pci bar allocation table (32/64/pref)");
    cherry_shell_register_symbol("pci_cap",  (uintptr_t)cmd_pci_cap,  CHERRY_SHELL_SYM_CMD, "list pci capabilities (standard & extended)");
    cherry_shell_register_symbol("its_info", (uintptr_t)cmd_its_info, CHERRY_SHELL_SYM_CMD, "display gicv3 its status and msi vectors");
}
