/*
 * CherryVFS - Virtual File System for FreeRTOS
 *
 * Copyright (c) 2026 ForgeStitch
 * SPDX-License-Identifier: MIT
 *
 * Platform Device and Platform Driver Framework
 */

#ifndef _CHERRY_VFS_PLATFORM_H_
#define _CHERRY_VFS_PLATFORM_H_

#include "vfs_types.h"
#include "vfs_dev.h"
#include "vfs_driver.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Platform Resource Flags */
#define IORESOURCE_MEM          0x00000001  /* Memory-mapped I/O resource */
#define IORESOURCE_IRQ          0x00000002  /* Interrupt line resource */
#define IORESOURCE_DMA          0x00000004  /* DMA channel resource */

struct platform_resource {
    uint32_t                    flags;      /* Resource type (IORESOURCE_*) */
    uint64_t                    start;      /* Physical start address or IRQ number */
    uint64_t                    end;        /* Physical end address or IRQ number */
    const char                 *name;       /* Resource name */
};

struct platform_device;
struct platform_driver;

/* Platform Device Representation */
struct platform_device {
    const char                 *name;           /* Device name matching driver */
    int                         id;             /* Device instance ID (-1 if single) */
    struct vfs_device           dev;            /* Inherited base device */
    const struct platform_resource *resources;  /* Array of hardware resources */
    size_t                      num_resources;  /* Count of resources */
    void                       *platform_data;  /* Board-specific configuration data */
};

/* Platform Driver Representation */
struct platform_driver {
    const char                 *name;           /* Driver name matching device */
    struct vfs_driver           drv;            /* Inherited base driver */
    int                       (*probe)(struct platform_device *pdev);
    int                       (*remove)(struct platform_device *pdev);
};

/* Helper Macros for Upcasting */
#define to_platform_device(d) container_of(d, struct platform_device, dev)
#define to_platform_driver(d) container_of(d, struct platform_driver, drv)

#ifndef container_of
#define container_of(ptr, type, member) \
    ((type *)((char *)(ptr) - offsetof(type, member)))
#endif

/* Global Platform Bus Type Declaration */
extern struct vfs_bus_type platform_bus_type;

/* Platform Framework APIs */
int platform_bus_init(void);

int platform_device_register(struct platform_device *pdev);
int platform_device_unregister(struct platform_device *pdev);

int platform_driver_register(struct platform_driver *pdrv);
int platform_driver_unregister(struct platform_driver *pdrv);

const struct platform_resource *platform_get_resource(struct platform_device *pdev,
                                                      uint32_t flags,
                                                      unsigned int index);

const struct platform_resource *platform_get_resource_byname(struct platform_device *pdev,
                                                            uint32_t flags,
                                                            const char *name);

int platform_get_irq(struct platform_device *pdev, unsigned int index);

#ifdef __cplusplus
}
#endif

#endif /* _CHERRY_VFS_PLATFORM_H_ */
