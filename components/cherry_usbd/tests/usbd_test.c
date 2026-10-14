/*
 * CherryUSBD - Acceptance Test Suite
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
#include "cherry_vfs.h"
#include "vfs_dev.h"
#include "cherry_printk.h"

void cherry_usbd_run_tests(void)
{
    pr_info("cherry_usbd: ==> starting cherryusbd acceptance test suite...\n");

    struct usbd_core *core = usbd_get_core();
    bool init_pass = (core != NULL && core->is_initialized);
    bool enum_desc_pass = false;
    bool set_addr_cfg_pass = false;
    bool cdc_acm_pass = false;
    bool cdc_vfs_pass = false;
    bool msc_inquiry_pass = false;
    bool msc_rw_pass = false;
    bool hid_report_pass = false;

    if (!init_pass) {
        pr_err("cherry_usbd: subsystem not initialized!\n");
        pr_info("cherry_usbd: [usbd-verify] init=fail all=fail\n");
        return;
    }

    /* 1. Chapter 9 GET_DESCRIPTOR Verification */
    struct usb_setup_packet setup;
    uint8_t buffer[256];
    uint32_t len = 0;

    /* Get Device Descriptor */
    memset(&setup, 0, sizeof(setup));
    setup.bmRequestType = USB_REQ_DIR_IN | USB_REQ_TYPE_STANDARD | USB_REQ_RECIPIENT_DEVICE;
    setup.bRequest = USB_REQ_GET_DESCRIPTOR;
    setup.wValue = (USB_DESC_TYPE_DEVICE << 8);
    setup.wLength = 18;

    if (usbd_vudc_inject_setup(&setup, buffer, &len) == 0 && len == 18) {
        struct usb_device_desc *dev_desc = (struct usb_device_desc *)buffer;
        if (dev_desc->bcdUSB == 0x0200 && dev_desc->idVendor == 0x1D6B && dev_desc->idProduct == 0x0104) {
            /* Get Configuration Descriptor */
            setup.wValue = (USB_DESC_TYPE_CONFIGURATION << 8);
            setup.wLength = 107;
            if (usbd_vudc_inject_setup(&setup, buffer, &len) == 0 && len == 107) {
                struct usb_config_desc *cfg_desc = (struct usb_config_desc *)buffer;
                if (cfg_desc->bNumInterfaces == 4) {
                    enum_desc_pass = true;
                }
            }
        }
    }

    /* 2. SET_ADDRESS and SET_CONFIGURATION */
    memset(&setup, 0, sizeof(setup));
    setup.bmRequestType = USB_REQ_DIR_OUT | USB_REQ_TYPE_STANDARD | USB_REQ_RECIPIENT_DEVICE;
    setup.bRequest = USB_REQ_SET_ADDRESS;
    setup.wValue = 0x15;
    setup.wLength = 0;

    if (usbd_vudc_inject_setup(&setup, NULL, &len) == 0 && core->address == 0x15 &&
        core->state == USBD_STATE_ADDRESSED) {
        setup.bRequest = USB_REQ_SET_CONFIGURATION;
        setup.wValue = 1;
        if (usbd_vudc_inject_setup(&setup, NULL, &len) == 0 && core->config_val == 1 &&
            core->state == USBD_STATE_CONFIGURED) {
            set_addr_cfg_pass = true;
        }
    }

    /* 3. CDC-ACM Management Requests */
    struct cdc_line_coding test_coding = {
        .dwDTERate = 921600,
        .bCharFormat = 0,
        .bParityType = 0,
        .bDataBits = 8,
    };
    memset(&setup, 0, sizeof(setup));
    setup.bmRequestType = USB_REQ_DIR_OUT | USB_REQ_TYPE_CLASS | USB_REQ_RECIPIENT_INTERFACE;
    setup.bRequest = CDC_REQ_SET_LINE_CODING;
    setup.wIndex = 0; /* Interface 0 */
    setup.wLength = sizeof(test_coding);

    if (usbd_vudc_inject_setup(&setup, &test_coding, &len) == 0) {
        struct cdc_line_coding read_coding;
        setup.bmRequestType = USB_REQ_DIR_IN | USB_REQ_TYPE_CLASS | USB_REQ_RECIPIENT_INTERFACE;
        setup.bRequest = CDC_REQ_GET_LINE_CODING;
        if (usbd_vudc_inject_setup(&setup, &read_coding, &len) == 0 && len == sizeof(read_coding)) {
            if (read_coding.dwDTERate == 921600 && read_coding.bDataBits == 8) {
                cdc_acm_pass = true;
            }
        }
    }

    /* 4. CDC Character Device (/dev/ttyUSB0) */
    struct vfs_cdev *cdev = vfs_cdev_find("ttyUSB0");
    if (cdev && cdev->ops && cdev->ops->write && cdev->ops->read) {
        const char *tx_msg = "cherry_usbd_loopback_ok";
        uint32_t msg_len = (uint32_t)strlen(tx_msg);
        if (cdev->ops->write(cdev, tx_msg, msg_len) == (ssize_t)msg_len) {
            char rx_buf[64];
            memset(rx_buf, 0, sizeof(rx_buf));
            if (cdev->ops->read(cdev, rx_buf, sizeof(rx_buf)) == (ssize_t)msg_len) {
                if (strcmp(rx_buf, tx_msg) == 0) {
                    cdc_vfs_pass = true;
                }
            }
        }
    }

    /* 5. MSC SCSI INQUIRY & READ_CAPACITY_10 */
    struct msc_cbw cbw;
    struct msc_csw csw;
    memset(&cbw, 0, sizeof(cbw));
    cbw.dCBWSignature = MSC_CBW_SIGNATURE;
    cbw.dCBWTag = 0x12345678;
    cbw.dCBWDataTransferLength = 36;
    cbw.bmCBWFlags = 0x80; /* IN */
    cbw.bCBWCBLength = 6;
    cbw.CBWCB[0] = SCSI_CMD_INQUIRY;
    cbw.CBWCB[4] = 36;

    uint8_t msc_buf[512];
    len = 0;
    if (usbd_msc_process_cbw(&cbw, msc_buf, &len, &csw) == 0 && csw.bCSWStatus == MSC_CSW_STATUS_PASSED) {
        if (memcmp(&msc_buf[8], "CherryOS", 8) == 0) {
            /* Now Test READ_CAPACITY_10 */
            memset(&cbw, 0, sizeof(cbw));
            cbw.dCBWSignature = MSC_CBW_SIGNATURE;
            cbw.dCBWTag = 0x12345679;
            cbw.dCBWDataTransferLength = 8;
            cbw.bmCBWFlags = 0x80;
            cbw.bCBWCBLength = 10;
            cbw.CBWCB[0] = SCSI_CMD_READ_CAPACITY_10;

            if (usbd_msc_process_cbw(&cbw, msc_buf, &len, &csw) == 0 && csw.bCSWStatus == MSC_CSW_STATUS_PASSED) {
                msc_inquiry_pass = true;
            }
        }
    }

    /* 6. MSC SCSI WRITE_10 and READ_10 */
    static uint8_t sector_wdata[512];
    static uint8_t sector_rdata[512];
    for (int i = 0; i < 512; i++) {
        sector_wdata[i] = (uint8_t)(0x77 ^ (i & 0xFF));
    }

    /* WRITE_10 LBA 5, 1 sector */
    memset(&cbw, 0, sizeof(cbw));
    cbw.dCBWSignature = MSC_CBW_SIGNATURE;
    cbw.dCBWTag = 0x20000001;
    cbw.dCBWDataTransferLength = 512;
    cbw.bmCBWFlags = 0x00; /* OUT */
    cbw.bCBWCBLength = 10;
    cbw.CBWCB[0] = SCSI_CMD_WRITE_10;
    cbw.CBWCB[5] = 5; /* LBA 5 */
    cbw.CBWCB[8] = 1; /* 1 sector */

    if (usbd_msc_process_cbw(&cbw, sector_wdata, &len, &csw) == 0 && csw.bCSWStatus == MSC_CSW_STATUS_PASSED) {
        /* READ_10 LBA 5, 1 sector */
        memset(&cbw, 0, sizeof(cbw));
        cbw.dCBWSignature = MSC_CBW_SIGNATURE;
        cbw.dCBWTag = 0x20000002;
        cbw.dCBWDataTransferLength = 512;
        cbw.bmCBWFlags = 0x80; /* IN */
        cbw.bCBWCBLength = 10;
        cbw.CBWCB[0] = SCSI_CMD_READ_10;
        cbw.CBWCB[5] = 5; /* LBA 5 */
        cbw.CBWCB[8] = 1; /* 1 sector */

        memset(sector_rdata, 0, sizeof(sector_rdata));
        if (usbd_msc_process_cbw(&cbw, sector_rdata, &len, &csw) == 0 && csw.bCSWStatus == MSC_CSW_STATUS_PASSED) {
            if (memcmp(sector_wdata, sector_rdata, 512) == 0) {
                msc_rw_pass = true;
            }
        }
    }

    /* 7. HID Report Descriptor & Keyboard Event */
    const uint8_t *hid_desc = NULL;
    uint32_t hid_len = 0;
    usbd_hid_get_report_desc(&hid_desc, &hid_len);
    if (hid_desc != NULL && hid_len == 63) {
        if (usbd_hid_send_keyboard(0x02 /* Left Shift */, 0x04 /* 'A' */) == 0 &&
            usbd_hid_send_mouse(0x01, 10, -5) == 0) {
            hid_report_pass = true;
        }
    }

    bool all_pass = init_pass && enum_desc_pass && set_addr_cfg_pass && cdc_acm_pass &&
                    cdc_vfs_pass && msc_inquiry_pass && msc_rw_pass && hid_report_pass;

    pr_info("cherry_usbd: [usbd-verify] init=%s enum_desc=%s set_addr_cfg=%s cdc_acm=%s cdc_vfs=%s msc_inquiry=%s msc_rw=%s hid_report=%s all=%s\n",
            init_pass ? "pass" : "fail",
            enum_desc_pass ? "pass" : "fail",
            set_addr_cfg_pass ? "pass" : "fail",
            cdc_acm_pass ? "pass" : "fail",
            cdc_vfs_pass ? "pass" : "fail",
            msc_inquiry_pass ? "pass" : "fail",
            msc_rw_pass ? "pass" : "fail",
            hid_report_pass ? "pass" : "fail",
            all_pass ? "pass" : "fail");
}
