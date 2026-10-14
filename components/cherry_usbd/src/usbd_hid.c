/*
 * CherryUSBD - USB HID (Human Interface Device) Driver
 *
 * Copyright (c) 2026 ForgeStitch
 * SPDX-License-Identifier: MIT
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "usbd_hid.h"
#include "cherry_usbd.h"
#include "cherry_printk.h"

/* Standard 63-byte HID Keyboard Report Descriptor */
static const uint8_t s_hid_keyboard_report_desc[63] = {
    0x05, 0x01,     /* Usage Page (Generic Desktop) */
    0x09, 0x06,     /* Usage (Keyboard) */
    0xA1, 0x01,     /* Collection (Application) */
    0x05, 0x07,     /* Usage Page (Key Codes) */
    0x19, 0xE0,     /* Usage Minimum (224) */
    0x29, 0xE7,     /* Usage Maximum (231) */
    0x15, 0x00,     /* Logical Minimum (0) */
    0x25, 0x01,     /* Logical Maximum (1) */
    0x75, 0x01,     /* Report Size (1) */
    0x95, 0x08,     /* Report Count (8) */
    0x81, 0x02,     /* Input (Data, Variable, Absolute) - Modifier keys */
    0x95, 0x01,     /* Report Count (1) */
    0x75, 0x08,     /* Report Size (8) */
    0x81, 0x01,     /* Input (Constant) - Reserved byte */
    0x95, 0x05,     /* Report Count (5) */
    0x75, 0x01,     /* Report Size (1) */
    0x05, 0x08,     /* Usage Page (Page# for LEDs) */
    0x19, 0x01,     /* Usage Minimum (1) */
    0x29, 0x05,     /* Usage Maximum (5) */
    0x91, 0x02,     /* Output (Data, Variable, Absolute) - LED report */
    0x95, 0x01,     /* Report Count (1) */
    0x75, 0x03,     /* Report Size (3) */
    0x91, 0x01,     /* Output (Constant) - LED report padding */
    0x95, 0x06,     /* Report Count (6) */
    0x75, 0x08,     /* Report Size (8) */
    0x15, 0x00,     /* Logical Minimum (0) */
    0x25, 0x65,     /* Logical Maximum (101) */
    0x05, 0x07,     /* Usage Page (Key Codes) */
    0x19, 0x00,     /* Usage Minimum (0) */
    0x29, 0x65,     /* Usage Maximum (101) */
    0x81, 0x00,     /* Input (Data, Array) - Key arrays (6 keys) */
    0xC0            /* End Collection */
};

static struct hid_keyboard_report s_last_kbd_report;
static struct hid_mouse_report    s_last_mouse_report;

void usbd_hid_get_report_desc(const uint8_t **desc, uint32_t *len)
{
    if (desc) *desc = s_hid_keyboard_report_desc;
    if (len) *len = sizeof(s_hid_keyboard_report_desc);
}

int usbd_hid_send_keyboard(uint8_t modifier, uint8_t keycode)
{
    memset(&s_last_kbd_report, 0, sizeof(s_last_kbd_report));
    s_last_kbd_report.modifier = modifier;
    s_last_kbd_report.keycodes[0] = keycode;
    return 0;
}

int usbd_hid_send_mouse(uint8_t buttons, int8_t dx, int8_t dy)
{
    s_last_mouse_report.buttons = buttons;
    s_last_mouse_report.x = dx;
    s_last_mouse_report.y = dy;
    s_last_mouse_report.wheel = 0;
    return 0;
}

int usbd_hid_init(void)
{
    memset(&s_last_kbd_report, 0, sizeof(s_last_kbd_report));
    memset(&s_last_mouse_report, 0, sizeof(s_last_mouse_report));
    pr_info("cherry_usbd: initialized hid keyboard/mouse class engine\n");
    return 0;
}
