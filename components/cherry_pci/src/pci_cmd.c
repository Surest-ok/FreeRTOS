/*
 * CherryPCI - CherryShell Diagnostics Commands (pci_ls, its_info)
 */

#include <stdint.h>
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
    printk("bdf       vendor:device  class    bar0_base   irq  mode\n");
    printk("----------------------------------------------------------\n");

    for (int i = 0; i < total; i++) {
        struct pci_dev *pdev = pci_get_device_by_index(i);
        if (pdev == 0) continue;

        const char *mode = pdev->msi_enabled ? "msi/its" : (pdev->irq_pin ? "intx" : "none");
        printk("%02x:%02x.%d   %04x:%04x       %06x 0x%08lx  %d  %s\n",
               pdev->bus, pdev->dev, pdev->fn,
               pdev->vendor, pdev->device, pdev->class_code,
               (unsigned long)pdev->resource[0], (int)pdev->irq, mode);
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
    cherry_shell_register_symbol("pci_ls", (uintptr_t)cmd_pci_ls, CHERRY_SHELL_SYM_CMD, "list all enumerated pci devices");
    cherry_shell_register_symbol("its_info", (uintptr_t)cmd_its_info, CHERRY_SHELL_SYM_CMD, "display gicv3 its status and msi vectors");
}
