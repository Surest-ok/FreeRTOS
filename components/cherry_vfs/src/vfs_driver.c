/*
 * CherryVFS - Virtual File System for FreeRTOS
 *
 * Copyright (c) 2026 ForgeStitch
 * SPDX-License-Identifier: MIT
 *
 * Driver and Bus Infrastructure: Device-Driver Matching and Probing
 */

#include "cherry_vfs.h"
#include <string.h>

static struct vfs_bus_type *g_bus_list = NULL;

int vfs_bus_register(struct vfs_bus_type *bus)
{
    if (!bus || bus->name[0] == '\0') {
        return -EINVAL;
    }

    vfs_lock();
    struct vfs_bus_type *cur = g_bus_list;
    while (cur) {
        if (strcmp(cur->name, bus->name) == 0) {
            vfs_unlock();
            return -EEXIST;
        }
        cur = cur->next;
    }

    bus->next = g_bus_list;
    g_bus_list = bus;
    vfs_unlock();

    return VFS_OK;
}

int vfs_bus_unregister(struct vfs_bus_type *bus)
{
    if (!bus) return -EINVAL;

    vfs_lock();
    struct vfs_bus_type **curr = &g_bus_list;
    while (*curr) {
        if (*curr == bus) {
            *curr = bus->next;
            vfs_unlock();
            return VFS_OK;
        }
        curr = &(*curr)->next;
    }
    vfs_unlock();
    return -ENOENT;
}

struct vfs_bus_type *vfs_bus_find(const char *name)
{
    if (!name) return NULL;

    vfs_lock();
    struct vfs_bus_type *cur = g_bus_list;
    while (cur) {
        if (strcmp(cur->name, name) == 0) {
            vfs_unlock();
            return cur;
        }
        cur = cur->next;
    }
    vfs_unlock();
    return NULL;
}

int vfs_driver_attach_device(struct vfs_device *dev)
{
    if (!dev || !dev->bus) {
        return -EINVAL;
    }
    struct vfs_bus_type *bus = dev->bus;
    struct vfs_driver *drv = bus->driver_list;

    while (drv) {
        int matched = 0;
        if (bus->match) {
            matched = bus->match(dev, drv);
        } else {
            matched = (strcmp(dev->name, drv->name) == 0);
        }

        if (matched) {
            dev->driver = drv;
            int ret = 0;
            if (bus->probe) {
                ret = bus->probe(dev);
            } else if (drv->probe) {
                ret = drv->probe(dev);
            }

            if (ret != 0) {
                dev->driver = NULL;
                return ret;
            }
            return VFS_OK;
        }
        drv = drv->next;
    }
    return -ENODEV;
}

int vfs_driver_detach_device(struct vfs_device *dev)
{
    if (!dev || !dev->driver) {
        return VFS_OK;
    }
    struct vfs_bus_type *bus = dev->bus;
    struct vfs_driver *drv = dev->driver;

    if (bus && bus->remove) {
        bus->remove(dev);
    } else if (drv->remove) {
        drv->remove(dev);
    }
    dev->driver = NULL;
    return VFS_OK;
}

int vfs_driver_register(struct vfs_driver *drv)
{
    if (!drv || drv->name[0] == '\0' || !drv->bus) {
        return -EINVAL;
    }

    struct vfs_bus_type *bus = drv->bus;

    vfs_lock();
    /* Check driver duplicate */
    struct vfs_driver *cur = bus->driver_list;
    while (cur) {
        if (strcmp(cur->name, drv->name) == 0) {
            vfs_unlock();
            return -EEXIST;
        }
        cur = cur->next;
    }

    drv->next = bus->driver_list;
    bus->driver_list = drv;

    /* Probe matching devices on this bus */
    struct vfs_device *dev = bus->device_list;
    while (dev) {
        if (!dev->driver) {
            vfs_driver_attach_device(dev);
        }
        dev = dev->bus_next;
    }
    vfs_unlock();

    return VFS_OK;
}

int vfs_driver_unregister(struct vfs_driver *drv)
{
    if (!drv || !drv->bus) return -EINVAL;

    struct vfs_bus_type *bus = drv->bus;

    vfs_lock();
    /* Detach all devices using this driver */
    struct vfs_device *dev = bus->device_list;
    while (dev) {
        if (dev->driver == drv) {
            vfs_driver_detach_device(dev);
        }
        dev = dev->bus_next;
    }


    /* Unlink driver */
    struct vfs_driver **curr = &bus->driver_list;
    while (*curr) {
        if (*curr == drv) {
            *curr = drv->next;
            vfs_unlock();
            return VFS_OK;
        }
        curr = &(*curr)->next;
    }
    vfs_unlock();

    return -ENOENT;
}

struct vfs_driver *vfs_driver_find(const char *name)
{
    if (!name) return NULL;

    vfs_lock();
    struct vfs_bus_type *b = g_bus_list;
    while (b) {
        struct vfs_driver *d = b->driver_list;
        while (d) {
            if (strcmp(d->name, name) == 0) {
                vfs_unlock();
                return d;
            }
            d = d->next;
        }
        b = b->next;
    }
    vfs_unlock();
    return NULL;
}

int vfs_bus_for_each_dev(struct vfs_bus_type *bus, void *data, int (*fn)(struct vfs_device *dev, void *data))
{
    if (!bus || !fn) return -EINVAL;

    vfs_lock();
    struct vfs_device *cur = bus->device_list;
    while (cur) {
        int ret = fn(cur, data);
        if (ret != 0) {
            vfs_unlock();
            return ret;
        }
        cur = cur->bus_next;
    }
    vfs_unlock();
    return VFS_OK;
}

int vfs_bus_for_each_drv(struct vfs_bus_type *bus, void *data, int (*fn)(struct vfs_driver *drv, void *data))
{
    if (!bus || !fn) return -EINVAL;

    vfs_lock();
    struct vfs_driver *cur = bus->driver_list;
    while (cur) {
        int ret = fn(cur, data);
        if (ret != 0) {
            vfs_unlock();
            return ret;
        }
        cur = cur->next;
    }
    vfs_unlock();
    return VFS_OK;
}
