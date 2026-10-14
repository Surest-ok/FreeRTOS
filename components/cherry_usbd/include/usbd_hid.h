/*
 * CherryUSBD - USB HID (Human Interface Device) Definitions
 *
 * Copyright (c) 2026 ForgeStitch
 * SPDX-License-Identifier: MIT
 */

#ifndef _USBD_HID_H_
#define _USBD_HID_H_

#include <stdint.h>
#include <stdbool.h>
#include "usb_spec.h"

/* HID Requests */
#define HID_REQ_GET_REPORT          0x01
#define HID_REQ_GET_IDLE            0x02
#define HID_REQ_GET_PROTOCOL        0x03
#define HID_REQ_SET_REPORT          0x09
#define HID_REQ_SET_IDLE            0x0A
#define HID_REQ_SET_PROTOCOL        0x0B

/* HID Report Types */
#define HID_REPORT_INPUT            0x01
#define HID_REPORT_OUTPUT           0x02
#define HID_REPORT_FEATURE          0x03

/* HID Keyboard Input Report (8 bytes) */
struct hid_keyboard_report {
    uint8_t modifier;
    uint8_t reserved;
    uint8_t keycodes[6];
} __attribute__((packed));

/* HID Mouse Input Report (4 bytes) */
struct hid_mouse_report {
    uint8_t buttons;
    int8_t  x;
    int8_t  y;
    int8_t  wheel;
} __attribute__((packed));

int  usbd_hid_init(void);
int  usbd_hid_send_keyboard(uint8_t modifier, uint8_t keycode);
int  usbd_hid_send_mouse(uint8_t buttons, int8_t dx, int8_t dy);
void usbd_hid_get_report_desc(const uint8_t **desc, uint32_t *len);

#endif /* _USBD_HID_H_ */
