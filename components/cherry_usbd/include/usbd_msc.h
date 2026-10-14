/*
 * CherryUSBD - USB Mass Storage Class (BOT / SCSI) Definitions
 *
 * Copyright (c) 2026 ForgeStitch
 * SPDX-License-Identifier: MIT
 */

#ifndef _USBD_MSC_H_
#define _USBD_MSC_H_

#include <stdint.h>
#include <stdbool.h>
#include "usb_spec.h"

/* MSC Subclass Codes */
#define MSC_SUBCLASS_RBC            0x01
#define MSC_SUBCLASS_ATAPI          0x02
#define MSC_SUBCLASS_UFI            0x04
#define MSC_SUBCLASS_SCSI           0x06

/* MSC Protocol Codes */
#define MSC_PROTOCOL_CBI_INT        0x00
#define MSC_PROTOCOL_CBI_NOINT      0x01
#define MSC_PROTOCOL_BOT            0x50

/* BOT Class-specific Requests */
#define MSC_REQ_RESET               0xFF
#define MSC_REQ_GET_MAX_LUN         0xFE

/* BOT Signatures */
#define MSC_CBW_SIGNATURE           0x43425355  /* "USBC" */
#define MSC_CSW_SIGNATURE           0x53425355  /* "USBS" */

/* CSW Status Codes */
#define MSC_CSW_STATUS_PASSED       0x00
#define MSC_CSW_STATUS_FAILED       0x01
#define MSC_CSW_STATUS_PHASE_ERR    0x02

/* Command Block Wrapper (CBW) - 31 bytes */
struct msc_cbw {
    uint32_t dCBWSignature;
    uint32_t dCBWTag;
    uint32_t dCBWDataTransferLength;
    uint8_t  bmCBWFlags;
    uint8_t  bCBWLUN;
    uint8_t  bCBWCBLength;
    uint8_t  CBWCB[16];
} __attribute__((packed));

/* Command Status Wrapper (CSW) - 13 bytes */
struct msc_csw {
    uint32_t dCSWSignature;
    uint32_t dCSWTag;
    uint32_t dCSWDataResidue;
    uint8_t  bCSWStatus;
} __attribute__((packed));

/* SCSI Command Opcodes */
#define SCSI_CMD_TEST_UNIT_READY            0x00
#define SCSI_CMD_REQUEST_SENSE              0x03
#define SCSI_CMD_INQUIRY                    0x12
#define SCSI_CMD_MODE_SELECT_6              0x15
#define SCSI_CMD_MODE_SENSE_6               0x1A
#define SCSI_CMD_START_STOP_UNIT            0x1B
#define SCSI_CMD_PREVENT_ALLOW_REMOVAL      0x1E
#define SCSI_CMD_READ_CAPACITY_10           0x25
#define SCSI_CMD_READ_10                    0x28
#define SCSI_CMD_WRITE_10                   0x2A
#define SCSI_CMD_VERIFY_10                  0x2F
#define SCSI_CMD_SYNCHRONIZE_CACHE_10       0x35

int  usbd_msc_init(void);
int  usbd_msc_process_cbw(const struct msc_cbw *cbw, void *data_buf, uint32_t *data_len, struct msc_csw *csw);
void usbd_msc_get_capacity(uint32_t *block_count, uint32_t *block_size);

#endif /* _USBD_MSC_H_ */
