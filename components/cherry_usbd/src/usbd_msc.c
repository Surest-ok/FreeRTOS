/*
 * CherryUSBD - USB Mass Storage Class (BOT / SCSI) Engine
 *
 * Copyright (c) 2026 ForgeStitch
 * SPDX-License-Identifier: MIT
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "usbd_msc.h"
#include "cherry_usbd.h"
#include "cherry_printk.h"

#define MSC_SECTOR_SIZE     512
#define MSC_SECTOR_COUNT    128 /* 64 KB Virtual RAM Disk */

static uint8_t s_ram_disk[MSC_SECTOR_COUNT * MSC_SECTOR_SIZE];

static uint32_t be32_to_cpu(const uint8_t *b)
{
    return ((uint32_t)b[0] << 24) | ((uint32_t)b[1] << 16) | ((uint32_t)b[2] << 8) | b[3];
}

static uint16_t be16_to_cpu(const uint8_t *b)
{
    return ((uint16_t)b[0] << 8) | b[1];
}

static void cpu_to_be32(uint8_t *b, uint32_t v)
{
    b[0] = (uint8_t)(v >> 24);
    b[1] = (uint8_t)(v >> 16);
    b[2] = (uint8_t)(v >> 8);
    b[3] = (uint8_t)(v & 0xFF);
}

void usbd_msc_get_capacity(uint32_t *block_count, uint32_t *block_size)
{
    if (block_count) *block_count = MSC_SECTOR_COUNT;
    if (block_size) *block_size = MSC_SECTOR_SIZE;
}

int usbd_msc_process_cbw(const struct msc_cbw *cbw, void *data_buf, uint32_t *data_len, struct msc_csw *csw)
{
    if (!cbw || !csw) return -1;

    if (cbw->dCBWSignature != MSC_CBW_SIGNATURE) {
        pr_err("cherry_usbd: invalid cbw signature 0x%08x\n", cbw->dCBWSignature);
        return -1;
    }

    csw->dCSWSignature = MSC_CSW_SIGNATURE;
    csw->dCSWTag = cbw->dCBWTag;
    csw->dCSWDataResidue = 0;
    csw->bCSWStatus = MSC_CSW_STATUS_PASSED;

    uint8_t opcode = cbw->CBWCB[0];

    switch (opcode) {
    case SCSI_CMD_TEST_UNIT_READY:
        if (data_len) *data_len = 0;
        break;

    case SCSI_CMD_INQUIRY: {
        uint8_t inq[36];
        memset(inq, 0, sizeof(inq));
        inq[0] = 0x00; /* Direct-access block device */
        inq[1] = 0x80; /* Removable */
        inq[2] = 0x02; /* ANSI SCSI-2 */
        inq[3] = 0x02; /* Response data format */
        inq[4] = 31;   /* Additional length (36 - 5) */
        memcpy(&inq[8],  "CherryOS", 8);
        memcpy(&inq[16], "Virtual USB Disk", 16);
        memcpy(&inq[32], "1.00", 4);

        uint32_t copy_len = cbw->dCBWDataTransferLength < 36 ? cbw->dCBWDataTransferLength : 36;
        if (data_buf) {
            memcpy(data_buf, inq, copy_len);
        }
        if (data_len) *data_len = copy_len;
        csw->dCSWDataResidue = cbw->dCBWDataTransferLength - copy_len;
        break;
    }

    case SCSI_CMD_READ_CAPACITY_10: {
        uint8_t cap[8];
        cpu_to_be32(&cap[0], MSC_SECTOR_COUNT - 1); /* Last logical block address */
        cpu_to_be32(&cap[4], MSC_SECTOR_SIZE);       /* Block size */

        uint32_t copy_len = cbw->dCBWDataTransferLength < 8 ? cbw->dCBWDataTransferLength : 8;
        if (data_buf) {
            memcpy(data_buf, cap, copy_len);
        }
        if (data_len) *data_len = copy_len;
        csw->dCSWDataResidue = cbw->dCBWDataTransferLength - copy_len;
        break;
    }

    case SCSI_CMD_REQUEST_SENSE: {
        uint8_t sense[18];
        memset(sense, 0, sizeof(sense));
        sense[0] = 0x70; /* Response code */
        sense[2] = 0x00; /* Sense key: NO SENSE */
        sense[7] = 10;   /* Additional sense length */

        uint32_t copy_len = cbw->dCBWDataTransferLength < 18 ? cbw->dCBWDataTransferLength : 18;
        if (data_buf) {
            memcpy(data_buf, sense, copy_len);
        }
        if (data_len) *data_len = copy_len;
        csw->dCSWDataResidue = cbw->dCBWDataTransferLength - copy_len;
        break;
    }

    case SCSI_CMD_MODE_SENSE_6: {
        uint8_t mode[4] = { 0x03, 0x00, 0x00, 0x00 };
        uint32_t copy_len = cbw->dCBWDataTransferLength < 4 ? cbw->dCBWDataTransferLength : 4;
        if (data_buf) {
            memcpy(data_buf, mode, copy_len);
        }
        if (data_len) *data_len = copy_len;
        csw->dCSWDataResidue = cbw->dCBWDataTransferLength - copy_len;
        break;
    }

    case SCSI_CMD_READ_10: {
        uint32_t lba = be32_to_cpu(&cbw->CBWCB[2]);
        uint16_t sectors = be16_to_cpu(&cbw->CBWCB[7]);
        uint32_t total_bytes = (uint32_t)sectors * MSC_SECTOR_SIZE;

        if (lba + sectors > MSC_SECTOR_COUNT) {
            csw->bCSWStatus = MSC_CSW_STATUS_FAILED;
            break;
        }

        if (data_buf) {
            memcpy(data_buf, &s_ram_disk[lba * MSC_SECTOR_SIZE], total_bytes);
        }
        if (data_len) *data_len = total_bytes;
        csw->dCSWDataResidue = cbw->dCBWDataTransferLength > total_bytes ?
                               cbw->dCBWDataTransferLength - total_bytes : 0;
        break;
    }

    case SCSI_CMD_WRITE_10: {
        uint32_t lba = be32_to_cpu(&cbw->CBWCB[2]);
        uint16_t sectors = be16_to_cpu(&cbw->CBWCB[7]);
        uint32_t total_bytes = (uint32_t)sectors * MSC_SECTOR_SIZE;

        if (lba + sectors > MSC_SECTOR_COUNT) {
            csw->bCSWStatus = MSC_CSW_STATUS_FAILED;
            break;
        }

        if (data_buf) {
            memcpy(&s_ram_disk[lba * MSC_SECTOR_SIZE], data_buf, total_bytes);
        }
        if (data_len) *data_len = 0;
        csw->dCSWDataResidue = 0;
        break;
    }

    case SCSI_CMD_PREVENT_ALLOW_REMOVAL:
    case SCSI_CMD_START_STOP_UNIT:
    case SCSI_CMD_VERIFY_10:
    case SCSI_CMD_SYNCHRONIZE_CACHE_10:
        if (data_len) *data_len = 0;
        break;

    default:
        pr_warn("cherry_usbd: unhandled scsi command opcode 0x%02x\n", opcode);
        csw->bCSWStatus = MSC_CSW_STATUS_FAILED;
        break;
    }

    return 0;
}

int usbd_msc_init(void)
{
    memset(s_ram_disk, 0, sizeof(s_ram_disk));
    pr_info("cherry_usbd: initialized msc bot engine (capacity=%u sectors, %u kb)\n",
            MSC_SECTOR_COUNT, (MSC_SECTOR_COUNT * MSC_SECTOR_SIZE) / 1024);
    return 0;
}
