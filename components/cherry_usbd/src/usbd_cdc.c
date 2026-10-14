/*
 * CherryUSBD - USB CDC-ACM Class Driver & CherryVFS Character Device (/dev/ttyUSB0)
 *
 * Copyright (c) 2026 ForgeStitch
 * SPDX-License-Identifier: MIT
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "usbd_cdc.h"
#include "cherry_usbd.h"
#include "cherry_vfs.h"
#include "vfs_dev.h"
#include "cherry_printk.h"

#define CDC_BUFFER_SIZE 2048

static struct cdc_line_coding s_line_coding = {
    .dwDTERate   = 115200,
    .bCharFormat = 0,   /* 1 stop bit */
    .bParityType = 0,   /* None */
    .bDataBits   = 8,   /* 8 bits */
};

static uint16_t s_control_line_state = 0x03; /* DTR | RTS */

/* Ring Buffer for Serial Stream */
static uint8_t  s_rx_buf[CDC_BUFFER_SIZE];
static uint32_t s_rx_head = 0;
static uint32_t s_rx_tail = 0;

static struct vfs_cdev s_cdc_cdev;

void usbd_cdc_get_line_coding(struct cdc_line_coding *coding)
{
    if (coding) *coding = s_line_coding;
}

void usbd_cdc_set_line_coding(const struct cdc_line_coding *coding)
{
    if (coding) {
        s_line_coding = *coding;
        pr_info("cherry_usbd: cdc-acm line coding updated: %u baud, %u-%c-%u\n",
                s_line_coding.dwDTERate,
                s_line_coding.bDataBits,
                s_line_coding.bParityType == 0 ? 'N' : (s_line_coding.bParityType == 1 ? 'O' : 'E'),
                s_line_coding.bCharFormat == 0 ? 1 : 2);
    }
}

uint16_t usbd_cdc_get_line_state(void)
{
    return s_control_line_state;
}

void usbd_cdc_set_line_state(uint16_t state)
{
    s_control_line_state = state;
}

int usbd_cdc_send(const void *buf, uint32_t len)
{
    if (!buf || len == 0) return 0;
    const uint8_t *src = (const uint8_t *)buf;

    for (uint32_t i = 0; i < len; i++) {
        uint32_t next = (s_rx_head + 1) % CDC_BUFFER_SIZE;
        if (next == s_rx_tail) {
            /* Full */
            return (int)i;
        }
        s_rx_buf[s_rx_head] = src[i];
        s_rx_head = next;
    }
    return (int)len;
}

int usbd_cdc_receive(void *buf, uint32_t len)
{
    if (!buf || len == 0) return 0;
    uint8_t *dst = (uint8_t *)buf;
    uint32_t copied = 0;

    while (s_rx_tail != s_rx_head && copied < len) {
        dst[copied++] = s_rx_buf[s_rx_tail];
        s_rx_tail = (s_rx_tail + 1) % CDC_BUFFER_SIZE;
    }
    return (int)copied;
}

/* VFS Character Device Operations for /dev/ttyUSB0 */
static int cdc_cdev_open(struct vfs_cdev *cdev, int flags)
{
    (void)cdev;
    (void)flags;
    return 0;
}

static int cdc_cdev_close(struct vfs_cdev *cdev)
{
    (void)cdev;
    return 0;
}

static ssize_t cdc_cdev_read(struct vfs_cdev *cdev, void *buf, size_t count)
{
    (void)cdev;
    int ret = usbd_cdc_receive(buf, (uint32_t)count);
    return ret >= 0 ? (ssize_t)ret : -1;
}

static ssize_t cdc_cdev_write(struct vfs_cdev *cdev, const void *buf, size_t count)
{
    (void)cdev;
    int ret = usbd_cdc_send(buf, (uint32_t)count);
    return ret >= 0 ? (ssize_t)ret : -1;
}

static int cdc_cdev_ioctl(struct vfs_cdev *cdev, unsigned int cmd, unsigned long arg)
{
    (void)cdev;
    (void)cmd;
    (void)arg;
    return 0;
}

static const struct vfs_cdev_ops s_cdc_cdev_ops = {
    .open  = cdc_cdev_open,
    .close = cdc_cdev_close,
    .read  = cdc_cdev_read,
    .write = cdc_cdev_write,
    .ioctl = cdc_cdev_ioctl,
    .poll  = NULL,
};

int usbd_cdc_init(void)
{
    s_cdc_cdev.ops = &s_cdc_cdev_ops;
    vfs_cdev_register(&s_cdc_cdev, "ttyUSB0");
    pr_info("cherry_usbd: registered cdc-acm virtual com port /dev/ttyUSB0\n");
    return 0;
}
