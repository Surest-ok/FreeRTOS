/*
 * CherryVFS - Virtual File System for FreeRTOS
 *
 * Copyright (c) 2026 ForgeStitch
 * SPDX-License-Identifier: MIT
 *
 * Driver and Bus Infrastructure (Linux-like Device Driver Model)
 */

#ifndef _CHERRY_VFS_DRIVER_H_
#define _CHERRY_VFS_DRIVER_H_

#include "vfs_types.h"
#include "vfs_dev.h"

#ifdef __cplusplus
extern "C" {
#endif

struct vfs_driver;
struct vfs_bus_type;

/* Driver Structure */
struct vfs_driver {
    char                    name[VFS_NAME_MAX]; /* Driver name */
    struct vfs_bus_type    *bus;                /* Associated bus */
    int                   (*probe)(struct vfs_device *dev);
    int                   (*remove)(struct vfs_device *dev);
    void                   *priv_data;
    struct vfs_driver      *next;               /* Linkage in bus driver list */
};

/* Bus Type Structure */
struct vfs_bus_type {
    char                    name[VFS_NAME_MAX]; /* Bus name (e.g. "platform", "spi", "i2c") */
    int                   (*match)(struct vfs_device *dev, struct vfs_driver *drv);
    int                   (*probe)(struct vfs_device *dev);
    int                   (*remove)(struct vfs_device *dev);
    struct vfs_device      *device_list;        /* Devices attached to this bus */
    struct vfs_driver      *driver_list;        /* Drivers registered on this bus */
    struct vfs_bus_type    *next;               /* Global bus list linkage */
};

/* Bus Management APIs */
int vfs_bus_register(struct vfs_bus_type *bus);
int vfs_bus_unregister(struct vfs_bus_type *bus);
struct vfs_bus_type *vfs_bus_find(const char *name);

/* Driver Management APIs */
int vfs_driver_register(struct vfs_driver *drv);
int vfs_driver_unregister(struct vfs_driver *drv);
struct vfs_driver *vfs_driver_find(const char *name);

/* Device-Driver Binding and Matching */
int vfs_driver_attach_device(struct vfs_device *dev);
int vfs_driver_detach_device(struct vfs_device *dev);

/* Bus Traversal Iterators */
int vfs_bus_for_each_dev(struct vfs_bus_type *bus, void *data, int (*fn)(struct vfs_device *dev, void *data));
int vfs_bus_for_each_drv(struct vfs_bus_type *bus, void *data, int (*fn)(struct vfs_driver *drv, void *data));

#ifdef __cplusplus
}
#endif

#endif /* _CHERRY_VFS_DRIVER_H_ */
