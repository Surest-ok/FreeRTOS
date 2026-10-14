/*
 * CherryUSBD - CherryShell Diagnostic Commands (usb_info, usb_cdc_echo)
 *
 * Copyright (c) 2026 ForgeStitch
 * SPDX-License-Identifier: MIT
 */

#include <stdint.h>
#include <string.h>
#include "cherry_usbd.h"
#include "usbd_cdc.h"
#include "usbd_msc.h"
#include "cherry_shell.h"
#include "cherry_vfs.h"
#include "vfs_dev.h"
#include "cherry_printk.h"

int cmd_usb_info(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    struct usbd_core *core = usbd_get_core();
    if (!core || !core->is_initialized) {
        printk("usb: subsystem not initialized\n");
        return -1;
    }

    const char *state_str = "default";
    switch (core->state) {
    case USBD_STATE_DEFAULT:    state_str = "default"; break;
    case USBD_STATE_ADDRESSED:  state_str = "addressed"; break;
    case USBD_STATE_CONFIGURED: state_str = "configured"; break;
    case USBD_STATE_SUSPENDED:  state_str = "suspended"; break;
    default: break;
    }

    uint32_t msc_blocks = 0, msc_bsize = 0;
    usbd_msc_get_capacity(&msc_blocks, &msc_bsize);

    struct cdc_line_coding coding;
    usbd_cdc_get_line_coding(&coding);

    printk("=== cherry_usbd: device status ===\n");
    printk("udc driver  : %s\n", core->udc ? core->udc->name : "none");
    printk("device state: %s\n", state_str);
    printk("address     : 0x%02x\n", core->address);
    printk("config value: %u\n", core->config_val);
    printk("classes     : cdc-acm (/dev/ttyUSB0, %u baud), msc-bot (%u kb), hid-keyboard\n",
           coding.dwDTERate, (msc_blocks * msc_bsize) / 1024);
    printk("setup pkts  : %u\n", core->setup_count);
    return 0;
}

int cmd_usb_cdc_echo(int argc, char **argv)
{
    const char *msg = (argc >= 2) ? argv[1] : "hello cherry_usbd!";
    uint32_t len = (uint32_t)strlen(msg);

    struct vfs_cdev *cdev = vfs_cdev_find("ttyUSB0");
    if (!cdev || !cdev->ops || !cdev->ops->write || !cdev->ops->read) {
        printk("usb_cdc_echo: /dev/ttyUSB0 device not found\n");
        return -1;
    }

    ssize_t written = cdev->ops->write(cdev, msg, len);
    if (written != (ssize_t)len) {
        printk("usb_cdc_echo: write failed\n");
        return -1;
    }

    char rx_buf[128];
    memset(rx_buf, 0, sizeof(rx_buf));
    ssize_t read_bytes = cdev->ops->read(cdev, rx_buf, sizeof(rx_buf) - 1);

    if (read_bytes == (ssize_t)len && strcmp(rx_buf, msg) == 0) {
        printk("usb_cdc_echo: success, echoed '%s' (%u bytes)\n", rx_buf, (uint32_t)read_bytes);
        return 0;
    }

    printk("usb_cdc_echo: mismatch (got '%s', len=%ld)\n", rx_buf, (long)read_bytes);
    return -1;
}

void usbd_register_shell_commands(void)
{
    cherry_shell_register_symbol("usb_info", (uintptr_t)cmd_usb_info, CHERRY_SHELL_SYM_CMD, "display usb device state, endpoints, and classes");
    cherry_shell_register_symbol("usb_cdc_echo", (uintptr_t)cmd_usb_cdc_echo, CHERRY_SHELL_SYM_CMD, "test /dev/ttyUSB0 loopback echo: usb_cdc_echo [message]");
}
