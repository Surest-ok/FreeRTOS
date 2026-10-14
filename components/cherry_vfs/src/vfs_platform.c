/*
 * CherryVFS - Virtual File System for FreeRTOS
 *
 * Copyright (c) 2026 ForgeStitch
 * SPDX-License-Identifier: MIT
 *
 * Platform Bus and Platform Device/Driver Infrastructure
 */

#include "cherry_vfs.h"
#include <string.h>

static int platform_match(struct vfs_device *dev, struct vfs_driver *drv)
{
    if (!dev || !drv) return 0;
    return (strcmp(dev->name, drv->name) == 0);
}

static int platform_probe(struct vfs_device *dev)
{
    if (!dev || !dev->driver) return -EINVAL;
    struct platform_device *pdev = to_platform_device(dev);
    struct platform_driver *pdrv = to_platform_driver(dev->driver);

    if (pdrv->probe) {
        return pdrv->probe(pdev);
    }
    return VFS_OK;
}

static int platform_remove(struct vfs_device *dev)
{
    if (!dev || !dev->driver) return -EINVAL;
    struct platform_device *pdev = to_platform_device(dev);
    struct platform_driver *pdrv = to_platform_driver(dev->driver);

    if (pdrv->remove) {
        return pdrv->remove(pdev);
    }
    return VFS_OK;
}

/* Global Platform Bus Type */
struct vfs_bus_type platform_bus_type = {
    .name = "platform",
    .match = platform_match,
    .probe = platform_probe,
    .remove = platform_remove,
    .device_list = NULL,
    .driver_list = NULL,
    .next = NULL,
};

int platform_bus_init(void)
{
    return vfs_bus_register(&platform_bus_type);
}

int platform_device_register(struct platform_device *pdev)
{
    if (!pdev || !pdev->name) {
        return -EINVAL;
    }

    strncpy(pdev->dev.name, pdev->name, VFS_NAME_MAX - 1);
    pdev->dev.name[VFS_NAME_MAX - 1] = '\0';
    pdev->dev.type = VFS_DEV_TYPE_BUS;
    pdev->dev.bus = &platform_bus_type;
    pdev->dev.priv_data = pdev;

    /* Register to global device model */
    int ret = vfs_device_register(&pdev->dev);
    if (ret != VFS_OK) {
        return ret;
    }

    vfs_lock();
    /* Add to platform bus device list */
    pdev->dev.bus_next = platform_bus_type.device_list;
    platform_bus_type.device_list = &pdev->dev;

    /* Attempt to attach driver */
    vfs_driver_attach_device(&pdev->dev);
    vfs_unlock();

    return VFS_OK;
}

int platform_device_unregister(struct platform_device *pdev)
{
    if (!pdev) return -EINVAL;

    vfs_lock();
    vfs_driver_detach_device(&pdev->dev);

    struct vfs_device **curr = &platform_bus_type.device_list;
    while (*curr) {
        if (*curr == &pdev->dev) {
            *curr = pdev->dev.bus_next;
            break;
        }
        curr = &(*curr)->bus_next;
    }
    vfs_unlock();

    vfs_device_unregister(&pdev->dev);
    return VFS_OK;
}


int platform_driver_register(struct platform_driver *pdrv)
{
    if (!pdrv || !pdrv->name) {
        return -EINVAL;
    }

    strncpy(pdrv->drv.name, pdrv->name, VFS_NAME_MAX - 1);
    pdrv->drv.name[VFS_NAME_MAX - 1] = '\0';
    pdrv->drv.bus = &platform_bus_type;
    pdrv->drv.priv_data = pdrv;

    return vfs_driver_register(&pdrv->drv);
}

int platform_driver_unregister(struct platform_driver *pdrv)
{
    if (!pdrv) return -EINVAL;
    return vfs_driver_unregister(&pdrv->drv);
}

const struct platform_resource *platform_get_resource(struct platform_device *pdev,
                                                      uint32_t flags,
                                                      unsigned int index)
{
    if (!pdev || !pdev->resources) return NULL;

    unsigned int match_count = 0;
    for (size_t i = 0; i < pdev->num_resources; i++) {
        if (pdev->resources[i].flags == flags) {
            if (match_count == index) {
                return &pdev->resources[i];
            }
            match_count++;
        }
    }
    return NULL;
}

const struct platform_resource *platform_get_resource_byname(struct platform_device *pdev,
                                                            uint32_t flags,
                                                            const char *name)
{
    if (!pdev || !pdev->resources || !name) return NULL;

    for (size_t i = 0; i < pdev->num_resources; i++) {
        if (pdev->resources[i].flags == flags && pdev->resources[i].name) {
            if (strcmp(pdev->resources[i].name, name) == 0) {
                return &pdev->resources[i];
            }
        }
    }
    return NULL;
}

int platform_get_irq(struct platform_device *pdev, unsigned int index)
{
    const struct platform_resource *res = platform_get_resource(pdev, IORESOURCE_IRQ, index);
    if (!res) {
        return -ENXIO;
    }
    return (int)res->start;
}
