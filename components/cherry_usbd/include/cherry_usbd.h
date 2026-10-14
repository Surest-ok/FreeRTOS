/*
 * CherryUSBD - USB Device Stack Subsystem for CherryOS / FreeRTOS
 *
 * Copyright (c) 2026 ForgeStitch
 * SPDX-License-Identifier: MIT
 */

#ifndef _CHERRY_USBD_H_
#define _CHERRY_USBD_H_

#include <stdint.h>
#include <stdbool.h>
#include "usb_spec.h"
#include "usbd_cdc.h"
#include "usbd_msc.h"
#include "usbd_hid.h"
#include "cherry_vfs.h"
#include "vfs_dev.h"

/* USB Device States */
typedef enum {
    USBD_STATE_DEFAULT = 0,
    USBD_STATE_ADDRESSED,
    USBD_STATE_CONFIGURED,
    USBD_STATE_SUSPENDED,
} usbd_state_t;

/* Endpoint Representation */
struct usbd_ep {
    uint8_t  ep_addr;
    uint8_t  ep_type;
    uint16_t max_packet;
    bool     is_stalled;
};

/* UDC Hardware Abstraction Driver */
struct usbd_udc_driver {
    const char *name;
    int  (*init)(void);
    int  (*set_address)(uint8_t address);
    int  (*ep_open)(uint8_t ep_addr, uint8_t ep_type, uint16_t max_packet);
    int  (*ep_close)(uint8_t ep_addr);
    int  (*ep_read)(uint8_t ep_addr, void *buf, uint32_t len);
    int  (*ep_write)(uint8_t ep_addr, const void *buf, uint32_t len);
    int  (*ep_set_stall)(uint8_t ep_addr);
    int  (*ep_clear_stall)(uint8_t ep_addr);
};

/* Device Core Context */
struct usbd_core {
    const struct usbd_udc_driver *udc;
    usbd_state_t                  state;
    uint8_t                       address;
    uint8_t                       config_val;

    /* Traffic Statistics */
    uint32_t                      setup_count;
    uint32_t                      rx_bytes;
    uint32_t                      tx_bytes;
    uint32_t                      error_count;
    bool                          is_initialized;
};

/* Subsystem Management */
int  cherry_usbd_init(void);
void cherry_usbd_run_tests(void);
struct usbd_core *usbd_get_core(void);

/* Chapter 9 Request Dispatcher */
int  usbd_handle_setup(const struct usb_setup_packet *setup, void *data_buf, uint32_t *data_len);

/* Virtual UDC Loopback Simulation API */
int  usbd_vudc_inject_setup(const struct usb_setup_packet *setup, void *data_buf, uint32_t *data_len);
int  usbd_vudc_bulk_out(uint8_t ep, const void *buf, uint32_t len);
int  usbd_vudc_bulk_in(uint8_t ep, void *buf, uint32_t len);

/* Shell Diagnostic Commands */
void usbd_register_shell_commands(void);

#endif /* _CHERRY_USBD_H_ */
