/*
 * CherryVFS - Virtual File System for FreeRTOS
 *
 * Copyright (c) 2026 ForgeStitch
 * SPDX-License-Identifier: MIT
 *
 * PL011 UART Platform Device and Platform Driver
 */

#include "cherry_vfs.h"
#include <string.h>

extern void uart_putc(char c);

static const struct platform_resource pl011_resources[] = {
    {
        .flags = IORESOURCE_MEM,
        .start = 0x09000000,
        .end   = 0x09000FFF,
        .name  = "pl011_mmio",
    },
    {
        .flags = IORESOURCE_IRQ,
        .start = 33,
        .end   = 33,
        .name  = "pl011_irq",
    },
};

static struct platform_device g_pl011_pdev = {
    .name          = "pl011_uart",
    .id            = 0,
    .resources     = pl011_resources,
    .num_resources = sizeof(pl011_resources) / sizeof(pl011_resources[0]),
    .platform_data = NULL,
};

/* Character device operations for PL011 UART */
static int pl011_cdev_open(struct vfs_cdev *cdev, int flags)
{
    (void)cdev;
    (void)flags;
    return VFS_OK;
}

static int pl011_cdev_close(struct vfs_cdev *cdev)
{
    (void)cdev;
    return VFS_OK;
}

static ssize_t pl011_cdev_read(struct vfs_cdev *cdev, void *buf, size_t count)
{
    (void)cdev;
    (void)buf;
    (void)count;
    return 0;
}

static ssize_t pl011_cdev_write(struct vfs_cdev *cdev, const void *buf, size_t count)
{
    (void)cdev;
    if (!buf || count == 0) return 0;

    const char *p = (const char *)buf;
    for (size_t i = 0; i < count; i++) {
        uart_putc(p[i]);
    }
    return (ssize_t)count;
}

static const struct vfs_cdev_ops g_pl011_cdev_ops = {
    .open  = pl011_cdev_open,
    .close = pl011_cdev_close,
    .read  = pl011_cdev_read,
    .write = pl011_cdev_write,
    .ioctl = NULL,
    .poll  = NULL,
};

static struct vfs_cdev g_uart0_cdev = {
    .ops = &g_pl011_cdev_ops,
};

static struct vfs_cdev g_console_cdev = {
    .ops = &g_pl011_cdev_ops,
};

static int pl011_driver_probe(struct platform_device *pdev)
{
    const struct platform_resource *mem_res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
    if (!mem_res || mem_res->start != 0x09000000) {
        return -ENODEV;
    }

    int irq = platform_get_irq(pdev, 0);
    if (irq != 33) {
        return -ENODEV;
    }

    /* Register /dev/uart0 and /dev/console */
    vfs_cdev_register(&g_uart0_cdev, "uart0");
    vfs_cdev_register(&g_console_cdev, "console");

    return VFS_OK;
}

static int pl011_driver_remove(struct platform_device *pdev)
{
    (void)pdev;
    vfs_cdev_unregister(&g_console_cdev);
    vfs_cdev_unregister(&g_uart0_cdev);
    return VFS_OK;
}

static struct platform_driver g_pl011_pdrv = {
    .name   = "pl011_uart",
    .probe  = pl011_driver_probe,
    .remove = pl011_driver_remove,
};

int dev_pl011_platform_init(void)
{
    int ret;
    /* 1. Register driver on platform bus */
    ret = platform_driver_register(&g_pl011_pdrv);
    if (ret != VFS_OK) return ret;

    /* 2. Register device on platform bus (triggers driver match and probe) */
    ret = platform_device_register(&g_pl011_pdev);
    if (ret != VFS_OK) return ret;

    return VFS_OK;
}
