/*
 * CherryUSBD - Chapter 9 Standard Device Framework and Request Engine
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
#include "usbd_hid.h"
#include "cherry_printk.h"

static struct usbd_core s_usbd_core;

/* Device Descriptor (18 bytes) */
static const struct usb_device_desc s_device_desc = {
    .bLength            = sizeof(struct usb_device_desc),
    .bDescriptorType    = USB_DESC_TYPE_DEVICE,
    .bcdUSB             = 0x0200,   /* USB 2.0 */
    .bDeviceClass       = 0xEF,     /* Miscellaneous Device */
    .bDeviceSubClass    = 0x02,     /* Common Class */
    .bDeviceProtocol    = 0x01,     /* Interface Association Descriptor */
    .bMaxPacketSize0    = 64,
    .idVendor           = 0x1D6B,   /* Linux Foundation */
    .idProduct          = 0x0104,   /* Multifunction Composite Gadget */
    .bcdDevice          = 0x0100,   /* Device Revision 1.0.0 */
    .iManufacturer      = 1,
    .iProduct           = 2,
    .iSerialNumber      = 3,
    .bNumConfigurations = 1,
};

/* Composite Configuration Descriptor:
 *   Interface 0: CDC Communication
 *   Interface 1: CDC Data
 *   Interface 2: MSC Bulk-Only Transport
 *   Interface 3: HID (Keyboard/Mouse)
 */
static const uint8_t s_config_desc_composite[] = {
    /* 1. Configuration Descriptor (9 bytes) */
    0x09, USB_DESC_TYPE_CONFIGURATION,
    0x6B, 0x00, /* wTotalLength: 107 bytes */
    0x04,       /* bNumInterfaces: 4 */
    0x01,       /* bConfigurationValue: 1 */
    0x00,       /* iConfiguration */
    0xC0,       /* bmAttributes: Self-powered */
    0x32,       /* bMaxPower: 100mA */

    /* 2. CDC Interface 0 (Communication Class, ACM) */
    0x09, USB_DESC_TYPE_INTERFACE,
    0x00,       /* bInterfaceNumber: 0 */
    0x00,       /* bAlternateSetting: 0 */
    0x01,       /* bNumEndpoints: 1 (Notification IN) */
    USB_CLASS_CDC,
    CDC_SUBCLASS_ACM,
    CDC_PROTOCOL_AT,
    0x00,

    /* CDC Header Functional Descriptor */
    0x05, 0x24, 0x00, 0x10, 0x01,
    /* CDC ACM Functional Descriptor */
    0x04, 0x24, 0x02, 0x02,
    /* CDC Union Functional Descriptor */
    0x05, 0x24, 0x06, 0x00, 0x01,
    /* CDC Call Management Functional Descriptor */
    0x05, 0x24, 0x01, 0x00, 0x01,

    /* Endpoint 1 IN (Interrupt Notification, 64B) */
    0x07, USB_DESC_TYPE_ENDPOINT,
    0x81,       /* EP1 IN */
    USB_EP_TYPE_INTERRUPT,
    0x40, 0x00, /* 64 bytes */
    0x10,       /* 16ms */

    /* 3. CDC Interface 1 (Data Class) */
    0x09, USB_DESC_TYPE_INTERFACE,
    0x01,       /* bInterfaceNumber: 1 */
    0x00,       /* bAlternateSetting: 0 */
    0x02,       /* bNumEndpoints: 2 (Bulk IN, Bulk OUT) */
    USB_CLASS_CDC_DATA,
    0x00,
    0x00,
    0x00,

    /* Endpoint 2 OUT (Bulk Data, 64B) */
    0x07, USB_DESC_TYPE_ENDPOINT,
    0x02,       /* EP2 OUT */
    USB_EP_TYPE_BULK,
    0x40, 0x00,
    0x00,

    /* Endpoint 2 IN (Bulk Data, 64B) */
    0x07, USB_DESC_TYPE_ENDPOINT,
    0x82,       /* EP2 IN */
    USB_EP_TYPE_BULK,
    0x40, 0x00,
    0x00,

    /* 4. MSC Interface 2 (Mass Storage, BOT) */
    0x09, USB_DESC_TYPE_INTERFACE,
    0x02,       /* bInterfaceNumber: 2 */
    0x00,       /* bAlternateSetting: 0 */
    0x02,       /* bNumEndpoints: 2 (Bulk IN, Bulk OUT) */
    USB_CLASS_MSC,
    MSC_SUBCLASS_SCSI,
    MSC_PROTOCOL_BOT,
    0x00,

    /* Endpoint 3 OUT (MSC Bulk OUT, 64B) */
    0x07, USB_DESC_TYPE_ENDPOINT,
    0x03,       /* EP3 OUT */
    USB_EP_TYPE_BULK,
    0x40, 0x00,
    0x00,

    /* Endpoint 3 IN (MSC Bulk IN, 64B) */
    0x07, USB_DESC_TYPE_ENDPOINT,
    0x83,       /* EP3 IN */
    USB_EP_TYPE_BULK,
    0x40, 0x00,
    0x00,

    /* 5. HID Interface 3 (Human Interface Device) */
    0x09, USB_DESC_TYPE_INTERFACE,
    0x03,       /* bInterfaceNumber: 3 */
    0x00,       /* bAlternateSetting: 0 */
    0x01,       /* bNumEndpoints: 1 (Interrupt IN) */
    USB_CLASS_HID,
    0x01,       /* Boot Interface */
    0x01,       /* Keyboard */
    0x00,

    /* HID Functional Descriptor */
    0x09, USB_DESC_TYPE_HID,
    0x11, 0x01, /* bcdHID: 1.11 */
    0x00,       /* bCountryCode */
    0x01,       /* bNumDescriptors */
    USB_DESC_TYPE_HID_REPORT,
    0x3F, 0x00, /* wItemLength: 63 bytes */

    /* Endpoint 4 IN (HID Interrupt IN, 64B) */
    0x07, USB_DESC_TYPE_ENDPOINT,
    0x84,       /* EP4 IN */
    USB_EP_TYPE_INTERRUPT,
    0x40, 0x00,
    0x0A,       /* 10ms */
};

/* String Descriptors in UTF-16LE */
static const uint8_t s_str_langid[] = {
    0x04, USB_DESC_TYPE_STRING,
    0x09, 0x04 /* English US (0x0409) */
};

/* "CherryOS Foundation" */
static const uint8_t s_str_mfg[] = {
    0x28, USB_DESC_TYPE_STRING,
    'C',0,'h',0,'e',0,'r',0,'r',0,'y',0,'O',0,'S',0,' ',0,
    'F',0,'o',0,'u',0,'n',0,'d',0,'a',0,'t',0,'i',0,'o',0,'n',0
};

/* "CherryUSB Composite Device" */
static const uint8_t s_str_product[] = {
    0x34, USB_DESC_TYPE_STRING,
    'C',0,'h',0,'e',0,'r',0,'r',0,'y',0,'U',0,'S',0,'B',0,' ',0,
    'C',0,'o',0,'m',0,'p',0,'o',0,'s',0,'i',0,'t',0,'e',0,' ',0,
    'D',0,'e',0,'v',0,'i',0,'c',0,'e',0
};

/* "CHERRY-UDC-001" */
static const uint8_t s_str_serial[] = {
    0x20, USB_DESC_TYPE_STRING,
    'C',0,'H',0,'E',0,'R',0,'R',0,'Y',0,'-',0,
    'U',0,'D',0,'C',0,'-',0,'0',0,'0',0,'1',0
};

int usbd_handle_standard_request(const struct usb_setup_packet *setup, void *data_buf, uint32_t *data_len)
{
    struct usbd_core *core = &s_usbd_core;
    uint32_t req_len = setup->wLength;
    uint32_t resp_len = 0;

    switch (setup->bRequest) {
    case USB_REQ_GET_DESCRIPTOR: {
        uint8_t desc_type = (uint8_t)(setup->wValue >> 8);
        uint8_t desc_idx  = (uint8_t)(setup->wValue & 0xFF);
        const void *src = NULL;

        if (desc_type == USB_DESC_TYPE_DEVICE) {
            src = &s_device_desc;
            resp_len = sizeof(s_device_desc);
        } else if (desc_type == USB_DESC_TYPE_CONFIGURATION) {
            src = s_config_desc_composite;
            resp_len = sizeof(s_config_desc_composite);
        } else if (desc_type == USB_DESC_TYPE_STRING) {
            if (desc_idx == 0) {
                src = s_str_langid;
                resp_len = sizeof(s_str_langid);
            } else if (desc_idx == 1) {
                src = s_str_mfg;
                resp_len = sizeof(s_str_mfg);
            } else if (desc_idx == 2) {
                src = s_str_product;
                resp_len = sizeof(s_str_product);
            } else if (desc_idx == 3) {
                src = s_str_serial;
                resp_len = sizeof(s_str_serial);
            }
        } else if (desc_type == USB_DESC_TYPE_HID_REPORT) {
            const uint8_t *hid_rep = NULL;
            uint32_t hid_rep_len = 0;
            usbd_hid_get_report_desc(&hid_rep, &hid_rep_len);
            src = hid_rep;
            resp_len = hid_rep_len;
        }

        if (src && resp_len > 0) {
            uint32_t actual = req_len < resp_len ? req_len : resp_len;
            if (data_buf) {
                memcpy(data_buf, src, actual);
            }
            if (data_len) *data_len = actual;
            return 0;
        }
        return -1;
    }

    case USB_REQ_SET_ADDRESS: {
        core->address = (uint8_t)(setup->wValue & 0x7F);
        core->state = USBD_STATE_ADDRESSED;
        if (core->udc && core->udc->set_address) {
            core->udc->set_address(core->address);
        }
        if (data_len) *data_len = 0;
        return 0;
    }

    case USB_REQ_GET_CONFIGURATION: {
        if (data_buf && req_len >= 1) {
            *(uint8_t *)data_buf = core->config_val;
            if (data_len) *data_len = 1;
            return 0;
        }
        return -1;
    }

    case USB_REQ_SET_CONFIGURATION: {
        core->config_val = (uint8_t)(setup->wValue & 0xFF);
        core->state = (core->config_val > 0) ? USBD_STATE_CONFIGURED : USBD_STATE_ADDRESSED;
        if (data_len) *data_len = 0;
        return 0;
    }

    case USB_REQ_GET_STATUS: {
        if (data_buf && req_len >= 2) {
            *(uint16_t *)data_buf = 0x0001; /* Self-powered */
            if (data_len) *data_len = 2;
            return 0;
        }
        return -1;
    }

    case USB_REQ_CLEAR_FEATURE:
    case USB_REQ_SET_FEATURE: {
        if (data_len) *data_len = 0;
        return 0;
    }

    case USB_REQ_GET_INTERFACE: {
        if (data_buf && req_len >= 1) {
            *(uint8_t *)data_buf = 0;
            if (data_len) *data_len = 1;
            return 0;
        }
        return -1;
    }

    case USB_REQ_SET_INTERFACE: {
        if (data_len) *data_len = 0;
        return 0;
    }

    default:
        break;
    }

    return -1;
}

int usbd_handle_setup(const struct usb_setup_packet *setup, void *data_buf, uint32_t *data_len)
{
    if (!setup) return -1;
    s_usbd_core.setup_count++;

    uint8_t req_type = setup->bmRequestType & USB_REQ_TYPE_MASK;

    /* 1. Standard Chapter 9 Requests */
    if (req_type == USB_REQ_TYPE_STANDARD) {
        return usbd_handle_standard_request(setup, data_buf, data_len);
    }

    /* 2. Class-specific Requests */
    if (req_type == USB_REQ_TYPE_CLASS) {
        uint8_t recipient = setup->bmRequestType & USB_REQ_RECIPIENT_MASK;
        uint8_t interface = (uint8_t)(setup->wIndex & 0xFF);

        if (recipient == USB_REQ_RECIPIENT_INTERFACE) {
            /* CDC ACM Management (Interface 0) */
            if (interface == 0) {
                if (setup->bRequest == CDC_REQ_SET_LINE_CODING) {
                    if (data_buf && setup->wLength >= sizeof(struct cdc_line_coding)) {
                        usbd_cdc_set_line_coding((const struct cdc_line_coding *)data_buf);
                    }
                    if (data_len) *data_len = 0;
                    return 0;
                } else if (setup->bRequest == CDC_REQ_GET_LINE_CODING) {
                    if (data_buf && setup->wLength >= sizeof(struct cdc_line_coding)) {
                        usbd_cdc_get_line_coding((struct cdc_line_coding *)data_buf);
                    }
                    if (data_len) *data_len = sizeof(struct cdc_line_coding);
                    return 0;
                } else if (setup->bRequest == CDC_REQ_SET_CONTROL_LINE_STATE) {
                    usbd_cdc_set_line_state(setup->wValue);
                    if (data_len) *data_len = 0;
                    return 0;
                }
            }
            /* MSC Bulk-Only Transport (Interface 2) */
            else if (interface == 2) {
                if (setup->bRequest == MSC_REQ_RESET) {
                    if (data_len) *data_len = 0;
                    return 0;
                } else if (setup->bRequest == MSC_REQ_GET_MAX_LUN) {
                    if (data_buf && setup->wLength >= 1) {
                        *(uint8_t *)data_buf = 0; /* Single LUN 0 */
                    }
                    if (data_len) *data_len = 1;
                    return 0;
                }
            }
            /* HID Class (Interface 3) */
            else if (interface == 3) {
                if (setup->bRequest == HID_REQ_SET_IDLE || setup->bRequest == HID_REQ_SET_PROTOCOL) {
                    if (data_len) *data_len = 0;
                    return 0;
                } else if (setup->bRequest == HID_REQ_GET_PROTOCOL) {
                    if (data_buf && setup->wLength >= 1) {
                        *(uint8_t *)data_buf = 1; /* Report Protocol */
                    }
                    if (data_len) *data_len = 1;
                    return 0;
                }
            }
        }
    }

    return -1;
}

struct usbd_core *usbd_get_core(void)
{
    return &s_usbd_core;
}
