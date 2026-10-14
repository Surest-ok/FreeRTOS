/*
 * CherryPCI - Core Subsystem Initializer
 */

#include <stdint.h>
#include "cherry_pci.h"
#include "cherry_its.h"
#include "cherry_printk.h"

extern void pci_register_shell_commands(void);

int cherry_pci_init(void)
{
    pr_info("cherry_pci: initializing pcie subsystem...\n");

    /* 1. Initialize ARM GICv3 ITS (Interrupt Translation Service) */
    its_init();

    /* 2. Scan PCIe bus and assign resources */
    pci_scan_bus();

    /* 3. Register Shell commands */
    pci_register_shell_commands();

    pr_info("cherry_pci: subsystem initialized successfully\n");
    return 0;
}
