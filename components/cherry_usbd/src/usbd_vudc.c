/*
 * CherryUSBD - Virtual UDC (Software Loopback USB Device Controller)
 *
 * Copyright (c) 2026 ForgeStitch
 * SPDX-License-Identifier: MIT
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "cherry_usbd.h"
#include "usb_spec.h"
#include "usbd_cdc.h"
#include "usbd_msc.h"
#include "cherry_printk.h"

#define VUDC_MAX_EP 8

struct vudc_ep_state {
    uint8_t  ep_addr;
    uint8_t  ep_type;
    uint16_t max_packet;
    bool     is_open;
    bool     is_stalled;
};

static struct vudc_ep_state s_eps[VUDC_MAX_EP];

static int vudc_init(void)
{
    memset(s_eps, 0, sizeof(s_eps));
    pr_info("cherry_usbd: virtual udc hardware layer initialized\n");
    return 0;
}

static int vudc_set_address(uint8_t address)
{
    pr_info("cherry_usbd: vudc address set to 0x%02x\n", address);
    return 0;
}

static int vudc_ep_open(uint8_t ep_addr, uint8_t ep_type, uint16_t max_packet)
{
    uint8_t ep_num = ep_addr & 0x0F;
    if (ep_num >= VUDC_MAX_EP) return -1;

    s_eps[ep_num].ep_addr = ep_addr;
    s_eps[ep_num].ep_type = ep_type;
    s_eps[ep_num].max_packet = max_packet;
    s_eps[ep_num].is_open = true;
    s_eps[ep_num].is_stalled = false;
    return 0;
}

static int vudc_ep_close(uint8_t ep_addr)
{
    uint8_t ep_num = ep_addr & 0x0F;
    if (ep_num >= VUDC_MAX_EP) return -1;
    s_eps[ep_num].is_open = false;
    return 0;
}

static int vudc_ep_read(uint8_t ep_addr, void *buf, uint32_t len)
{
    uint8_t ep_num = ep_addr & 0x0F;
    if (ep_num == 2) {
        /* CDC Data OUT */
        return usbd_cdc_receive(buf, len);
    }
    return 0;
}

static int vudc_ep_write(uint8_t ep_addr, const void *buf, uint32_t len)
{
    uint8_t ep_num = ep_addr & 0x0F;
    if (ep_num == 2) {
        /* CDC Data IN */
        return usbd_cdc_send(buf, len);
    }
    return 0;
}

static int vudc_ep_set_stall(uint8_t ep_addr)
{
    uint8_t ep_num = ep_addr & 0x0F;
    if (ep_num < VUDC_MAX_EP) {
        s_eps[ep_num].is_stalled = true;
    }
    return 0;
}

static int vudc_ep_clear_stall(uint8_t ep_addr)
{
    uint8_t ep_num = ep_addr & 0x0F;
    if (ep_num < VUDC_MAX_EP) {
        s_eps[ep_num].is_stalled = false;
    }
    return 0;
}

static const struct usbd_udc_driver s_vudc_driver = {
    .name            = "vudc-loopback",
    .init            = vudc_init,
    .set_address     = vudc_set_address,
    .ep_open         = vudc_ep_open,
    .ep_close        = vudc_ep_close,
    .ep_read         = vudc_ep_read,
    .ep_write        = vudc_ep_write,
    .ep_set_stall    = vudc_ep_set_stall,
    .ep_clear_stall  = vudc_ep_clear_stall,
};

int usbd_vudc_inject_setup(const struct usb_setup_packet *setup, void *data_buf, uint32_t *data_len)
{
    return usbd_handle_setup(setup, data_buf, data_len);
}

int usbd_vudc_bulk_out(uint8_t ep, const void *buf, uint32_t len)
{
    if ((ep & 0x0F) == 2) {
        /* CDC Data */
        return usbd_cdc_send(buf, len);
    }
    return 0;
}

int usbd_vudc_bulk_in(uint8_t ep, void *buf, uint32_t len)
{
    if ((ep & 0x0F) == 2) {
        /* CDC Data */
        return usbd_cdc_receive(buf, len);
    }
    return 0;
}

int cherry_usbd_init(void)
{
    pr_info("cherry_usbd: initializing usb device stack subsystem...\n");

    struct usbd_core *core = usbd_get_core();
    memset(core, 0, sizeof(*core));
    core->udc = &s_vudc_driver;
    core->state = USBD_STATE_DEFAULT;

    /* Initialize UDC Hardware */
    core->udc->init();

    /* Initialize Class Drivers */
    usbd_cdc_init();
    usbd_msc_init();
    usbd_hid_init();

    core->is_initialized = true;

    /* Register Shell Commands */
    usbd_register_shell_commands();

    pr_info("cherry_usbd: subsystem initialized successfully\n");
    return 0;
}
