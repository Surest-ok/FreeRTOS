/*
 * CherryUSBD - USB CDC-ACM (Virtual COM Port) Definitions
 *
 * Copyright (c) 2026 ForgeStitch
 * SPDX-License-Identifier: MIT
 */

#ifndef _USBD_CDC_H_
#define _USBD_CDC_H_

#include <stdint.h>
#include <stdbool.h>
#include "usb_spec.h"

/* CDC Subclass Codes */
#define CDC_SUBCLASS_ACM            0x02

/* CDC Protocol Codes */
#define CDC_PROTOCOL_NONE           0x00
#define CDC_PROTOCOL_AT             0x01

/* CDC Management Element Requests */
#define CDC_REQ_SEND_ENCAPSULATED_CMD       0x00
#define CDC_REQ_GET_ENCAPSULATED_RESP       0x01
#define CDC_REQ_SET_COMM_FEATURE            0x02
#define CDC_REQ_GET_COMM_FEATURE            0x03
#define CDC_REQ_CLEAR_COMM_FEATURE          0x04
#define CDC_REQ_SET_LINE_CODING             0x20
#define CDC_REQ_GET_LINE_CODING             0x21
#define CDC_REQ_SET_CONTROL_LINE_STATE      0x22
#define CDC_REQ_SEND_BREAK                  0x23

/* CDC Line Coding Structure (7 bytes) */
struct cdc_line_coding {
    uint32_t dwDTERate;     /* Data terminal rate, in bits per second */
    uint8_t  bCharFormat;   /* Stop bits (0: 1 stop bit, 1: 1.5 stop bits, 2: 2 stop bits) */
    uint8_t  bParityType;   /* Parity (0: None, 1: Odd, 2: Even, 3: Mark, 4: Space) */
    uint8_t  bDataBits;     /* Data bits (5, 6, 7, 8 or 16) */
} __attribute__((packed));

/* CDC Notification Codes */
#define CDC_NOTIFY_NETWORK_CONNECTION       0x00
#define CDC_NOTIFY_RESPONSE_AVAILABLE       0x01
#define CDC_NOTIFY_SERIAL_STATE             0x20

int  usbd_cdc_init(void);
int  usbd_cdc_send(const void *buf, uint32_t len);
int  usbd_cdc_receive(void *buf, uint32_t len);
void usbd_cdc_get_line_coding(struct cdc_line_coding *coding);
void usbd_cdc_set_line_coding(const struct cdc_line_coding *coding);
uint16_t usbd_cdc_get_line_state(void);
void usbd_cdc_set_line_state(uint16_t state);

#endif /* _USBD_CDC_H_ */
