/*
 * CherryPCI - Linux-style PCI Driver Registration & Probing
 */

#include <stdint.h>
#include <stdbool.h>
#include "cherry_pci.h"
#include "cherry_printk.h"

#define MAX_PCI_DRIVERS 16

static struct pci_driver *s_drivers[MAX_PCI_DRIVERS];
static int s_driver_count = 0;

static const struct pci_device_id *pci_match_one_device(const struct pci_device_id *id,
                                                        const struct pci_dev *dev)
{
    if ((id->vendor == PCI_ANY_ID || id->vendor == dev->vendor) &&
        (id->device == PCI_ANY_ID || id->device == dev->device) &&
        (id->subvendor == PCI_ANY_ID || id->subvendor == dev->subsystem_vendor) &&
        (id->subdevice == PCI_ANY_ID || id->subdevice == dev->subsystem_device) &&
        !((id->class_code ^ dev->class_code) & id->class_mask)) {
        return id;
    }
    return 0;
}

static const struct pci_device_id *pci_match_device(struct pci_driver *drv, struct pci_dev *dev)
{
    const struct pci_device_id *ids = drv->id_table;
    if (ids == 0) {
        return 0;
    }

    while (ids->vendor || ids->subvendor || ids->class_code) {
        if (pci_match_one_device(ids, dev)) {
            return ids;
        }
        ids++;
    }
    return 0;
}

int pci_register_driver(struct pci_driver *driver)
{
    if (s_driver_count >= MAX_PCI_DRIVERS) {
        return -1;
    }

    s_drivers[s_driver_count++] = driver;
    pr_info("cherry_pci: registered pci driver [%s]\n", driver->name ? driver->name : "unnamed");

    /* Probe matching devices */
    int num_devs = pci_get_device_count();
    for (int i = 0; i < num_devs; i++) {
        struct pci_dev *pdev = pci_get_device_by_index(i);
        if (pdev != 0 && pdev->drv_data == 0) {
            const struct pci_device_id *matched = pci_match_device(driver, pdev);
            if (matched != 0) {
                pr_info("cherry_pci: driver [%s] binding to [%02x:%02x.%d] %04x:%04x\n",
                        driver->name, pdev->bus, pdev->dev, pdev->fn, pdev->vendor, pdev->device);
                if (driver->probe(pdev, matched) == 0) {
                    pr_info("cherry_pci: driver [%s] probe successful\n", driver->name);
                }
            }
        }
    }

    return 0;
}

void pci_unregister_driver(struct pci_driver *driver)
{
    for (int i = 0; i < s_driver_count; i++) {
        if (s_drivers[i] == driver) {
            if (driver->remove != 0) {
                int num_devs = pci_get_device_count();
                for (int d = 0; d < num_devs; d++) {
                    struct pci_dev *pdev = pci_get_device_by_index(d);
                    if (pdev != 0 && pdev->drv_data != 0) {
                        driver->remove(pdev);
                    }
                }
            }
            s_drivers[i] = s_drivers[--s_driver_count];
            break;
        }
    }
}
