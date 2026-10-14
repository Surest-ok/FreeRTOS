/*
 * CherryUSBD - USB 2.0 / 3.0 Standard Specifications
 *
 * Copyright (c) 2026 ForgeStitch
 * SPDX-License-Identifier: MIT
 */

#ifndef _USB_SPEC_H_
#define _USB_SPEC_H_

#include <stdint.h>

/* Standard USB Request Types (bmRequestType) */
#define USB_REQ_DIR_MASK            0x80
#define USB_REQ_DIR_OUT             0x00
#define USB_REQ_DIR_IN              0x80

#define USB_REQ_TYPE_MASK           0x60
#define USB_REQ_TYPE_STANDARD       0x00
#define USB_REQ_TYPE_CLASS          0x20
#define USB_REQ_TYPE_VENDOR         0x40

#define USB_REQ_RECIPIENT_MASK      0x1F
#define USB_REQ_RECIPIENT_DEVICE    0x00
#define USB_REQ_RECIPIENT_INTERFACE 0x01
#define USB_REQ_RECIPIENT_ENDPOINT  0x02
#define USB_REQ_RECIPIENT_OTHER     0x03

/* Standard USB Requests (bRequest) */
#define USB_REQ_GET_STATUS          0x00
#define USB_REQ_CLEAR_FEATURE       0x01
#define USB_REQ_SET_FEATURE         0x03
#define USB_REQ_SET_ADDRESS         0x05
#define USB_REQ_GET_DESCRIPTOR      0x06
#define USB_REQ_SET_DESCRIPTOR      0x07
#define USB_REQ_GET_CONFIGURATION   0x08
#define USB_REQ_SET_CONFIGURATION   0x09
#define USB_REQ_GET_INTERFACE       0x0A
#define USB_REQ_SET_INTERFACE       0x0B
#define USB_REQ_SYNCH_FRAME         0x0C

/* Standard Descriptor Types */
#define USB_DESC_TYPE_DEVICE        0x01
#define USB_DESC_TYPE_CONFIGURATION 0x02
#define USB_DESC_TYPE_STRING        0x03
#define USB_DESC_TYPE_INTERFACE     0x04
#define USB_DESC_TYPE_ENDPOINT      0x05
#define USB_DESC_TYPE_DEVICE_QUAL   0x06
#define USB_DESC_TYPE_OTHER_SPEED   0x07
#define USB_DESC_TYPE_INTERFACE_PWR 0x08
#define USB_DESC_TYPE_OTG           0x09
#define USB_DESC_TYPE_DEBUG         0x0A
#define USB_DESC_TYPE_INTERFACE_ASS 0x0B
#define USB_DESC_TYPE_BOS           0x0F
#define USB_DESC_TYPE_DEVICE_CAP    0x10
#define USB_DESC_TYPE_HID           0x21
#define USB_DESC_TYPE_HID_REPORT    0x22

/* USB Class Codes */
#define USB_CLASS_PER_INTERFACE     0x00
#define USB_CLASS_AUDIO             0x01
#define USB_CLASS_CDC               0x02
#define USB_CLASS_HID               0x03
#define USB_CLASS_PHYSICAL          0x05
#define USB_CLASS_IMAGE             0x06
#define USB_CLASS_PRINTER           0x07
#define USB_CLASS_MSC               0x08
#define USB_CLASS_HUB               0x09
#define USB_CLASS_CDC_DATA          0x0A
#define USB_CLASS_SMART_CARD        0x0B
#define USB_CLASS_VIDEO             0x0E
#define USB_CLASS_WIRELESS          0xE0
#define USB_CLASS_MISC              0xEF
#define USB_CLASS_VENDOR_SPEC       0xFF

/* Endpoint Attributes */
#define USB_EP_TYPE_MASK            0x03
#define USB_EP_TYPE_CONTROL         0x00
#define USB_EP_TYPE_ISOCHRONOUS     0x01
#define USB_EP_TYPE_BULK            0x02
#define USB_EP_TYPE_INTERRUPT       0x03

#define USB_EP_DIR_MASK             0x80
#define USB_EP_DIR_OUT              0x00
#define USB_EP_DIR_IN               0x80

/* Standard Setup Packet (8 bytes) */
struct usb_setup_packet {
    uint8_t  bmRequestType;
    uint8_t  bRequest;
    uint16_t wValue;
    uint16_t wIndex;
    uint16_t wLength;
} __attribute__((packed));

/* Device Descriptor (18 bytes) */
struct usb_device_desc {
    uint8_t  bLength;
    uint8_t  bDescriptorType;
    uint16_t bcdUSB;
    uint8_t  bDeviceClass;
    uint8_t  bDeviceSubClass;
    uint8_t  bDeviceProtocol;
    uint8_t  bMaxPacketSize0;
    uint16_t idVendor;
    uint16_t idProduct;
    uint16_t bcdDevice;
    uint8_t  iManufacturer;
    uint8_t  iProduct;
    uint8_t  iSerialNumber;
    uint8_t  bNumConfigurations;
} __attribute__((packed));

/* Configuration Descriptor (9 bytes) */
struct usb_config_desc {
    uint8_t  bLength;
    uint8_t  bDescriptorType;
    uint16_t wTotalLength;
    uint8_t  bNumInterfaces;
    uint8_t  bConfigurationValue;
    uint8_t  iConfiguration;
    uint8_t  bmAttributes;
    uint8_t  bMaxPower;
} __attribute__((packed));

/* Interface Descriptor (9 bytes) */
struct usb_interface_desc {
    uint8_t  bLength;
    uint8_t  bDescriptorType;
    uint8_t  bInterfaceNumber;
    uint8_t  bAlternateSetting;
    uint8_t  bNumEndpoints;
    uint8_t  bInterfaceClass;
    uint8_t  bInterfaceSubClass;
    uint8_t  bInterfaceProtocol;
    uint8_t  iInterface;
} __attribute__((packed));

/* Endpoint Descriptor (7 bytes) */
struct usb_endpoint_desc {
    uint8_t  bLength;
    uint8_t  bDescriptorType;
    uint8_t  bEndpointAddress;
    uint8_t  bmAttributes;
    uint16_t wMaxPacketSize;
    uint8_t  bInterval;
} __attribute__((packed));

/* HID Descriptor (9 bytes) */
struct usb_hid_desc {
    uint8_t  bLength;
    uint8_t  bDescriptorType;
    uint16_t bcdHID;
    uint8_t  bCountryCode;
    uint8_t  bNumDescriptors;
    uint8_t  bReportDescriptorType;
    uint16_t wItemLength;
} __attribute__((packed));

#endif /* _USB_SPEC_H_ */
