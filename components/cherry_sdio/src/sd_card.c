/*
 * CherrySDIO - SD / SDIO / MMC / eMMC Core State Machine & Initialization
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "sdhci.h"
#include "sd_protocol.h"
#include "cherry_printk.h"

int sd_card_detect_and_init(struct sdhci_host *host)
{
    uint32_t resp[4] = {0};

    pr_info("cherry_sdio: starting card identification state machine...\n");

    /* 1. CMD0: Reset card to Idle State */
    sdhci_send_cmd(host, CMD0_GO_IDLE_STATE, 0, SDHCI_CMD_RESP_NONE, 0);

    /* 2. CMD8: Voltage check and SD 2.0+ pattern verification (0x1AA) */
    int ret = sdhci_send_cmd(host, CMD8_SEND_IF_COND,
                             SD_CMD8_VOLTAGE_27_36 | SD_CMD8_CHECK_PATTERN,
                             SDHCI_CMD_RESP_48 | SDHCI_CMD_CRC_CHECK | SDHCI_CMD_INDEX_CHECK,
                             resp);

    bool is_sd_v2 = false;
    if (ret == 0 && (resp[0] & 0xFF) == 0xAA) {
        is_sd_v2 = true;
        pr_info("cherry_sdio: detected sd specification v2.0+ card\n");
    } else {
        pr_info("cherry_sdio: cmd8 rejected or legacy card, probing mmc/emmc...\n");
    }

    /* 3. OCR Negotiation loop (ACMD41 for SD, CMD1 for eMMC) */
    uint32_t ocr = 0;
    bool card_ready = false;

    if (is_sd_v2) {
        for (int i = 0; i < 200; i++) {
            /* CMD55: APP_CMD */
            sdhci_send_cmd(host, CMD55_APP_CMD, 0, SDHCI_CMD_RESP_48 | SDHCI_CMD_CRC_CHECK, resp);

            /* ACMD41: HCS=1, 3.3V */
            uint32_t acmd41_arg = SD_OCR_HCS | SD_OCR_VOLTAGE_WINDOW;
            ret = sdhci_send_cmd(host, ACMD41_SD_SEND_OP_COND, acmd41_arg, SDHCI_CMD_RESP_48, resp);
            if (ret == 0) {
                ocr = resp[0];
                if (ocr & SD_OCR_BUSY) {
                    card_ready = true;
                    break;
                }
            }
        }
    }

    if (!card_ready) {
        /* Probe eMMC with CMD1 */
        for (int i = 0; i < 200; i++) {
            ret = sdhci_send_cmd(host, CMD1_SEND_OP_COND, 0x40FF8000U, SDHCI_CMD_RESP_48, resp);
            if (ret == 0) {
                ocr = resp[0];
                if (ocr & SD_OCR_BUSY) {
                    card_ready = true;
                    host->card.type = SD_CARD_TYPE_EMMC;
                    pr_info("cherry_sdio: detected emmc device via cmd1\n");
                    break;
                }
            }
        }
    }

    if (!card_ready) {
        pr_err("cherry_sdio: card power-up timeout or no card present\n");
        return -1;
    }

    if (host->card.type != SD_CARD_TYPE_EMMC) {
        host->card.high_capacity = (ocr & SD_OCR_CCS) != 0;
        host->card.type = host->card.high_capacity ? SD_CARD_TYPE_SDHC : SD_CARD_TYPE_SDSC;
    }

    pr_info("cherry_sdio: ocr=0x%08x high_capacity=%d\n", ocr, host->card.high_capacity);

    /* 4. CMD2: ALL_SEND_CID (retrieve 128-bit CID) */
    ret = sdhci_send_cmd(host, CMD2_ALL_SEND_CID, 0, SDHCI_CMD_RESP_136 | SDHCI_CMD_CRC_CHECK, resp);
    if (ret == 0) {
        memcpy(host->card.raw_cid, resp, sizeof(resp));
        /* Extract Product Name (4 chars in resp[0]/resp[1]) */
        host->card.product_name[0] = (resp[0] >> 24) & 0xFF;
        host->card.product_name[1] = (resp[0] >> 16) & 0xFF;
        host->card.product_name[2] = (resp[0] >> 8) & 0xFF;
        host->card.product_name[3] = (resp[0]) & 0xFF;
        host->card.product_name[4] = (resp[1] >> 24) & 0xFF;
        host->card.product_name[5] = 0;
        host->card.serial_number = resp[2];
        pr_info("cherry_sdio: cid product='%s' serial=0x%08x\n",
                host->card.product_name, host->card.serial_number);
    }

    /* 5. CMD3: SET_RELATIVE_ADDR / SEND_RELATIVE_ADDR */
    uint16_t rca = 1;
    if (host->card.type == SD_CARD_TYPE_EMMC) {
        sdhci_send_cmd(host, CMD3_SET_RELATIVE_ADDR, (uint32_t)rca << 16,
                       SDHCI_CMD_RESP_48 | SDHCI_CMD_CRC_CHECK, resp);
    } else {
        sdhci_send_cmd(host, CMD3_SET_RELATIVE_ADDR, 0,
                       SDHCI_CMD_RESP_48 | SDHCI_CMD_CRC_CHECK, resp);
        rca = (uint16_t)(resp[0] >> 16);
    }
    host->card.rca = rca;
    pr_info("cherry_sdio: assigned rca=0x%04x\n", rca);

    /* 6. CMD9: SEND_CSD (retrieve Card Specific Data) */
    ret = sdhci_send_cmd(host, CMD9_SEND_CSD, (uint32_t)rca << 16,
                         SDHCI_CMD_RESP_136 | SDHCI_CMD_CRC_CHECK, resp);
    if (ret == 0) {
        memcpy(host->card.raw_csd, resp, sizeof(resp));
        uint8_t csd_ver = (resp[0] >> 30) & 0x03;
        uint64_t capacity = 0;

        if (csd_ver == 1) {
            /* CSD Version 2.0 (SDHC / SDXC) */
            uint32_t c_size = ((resp[1] >> 16) & 0xFFFF) | ((resp[2] & 0x3F) << 16);
            capacity = (uint64_t)(c_size + 1) * 512 * 1024ULL;
        } else {
            /* CSD Version 1.0 (Standard Capacity or default) */
            capacity = 64ULL * 1024ULL * 1024ULL;
        }

        host->card.capacity_bytes = capacity;
        host->card.sector_count = (uint32_t)(capacity / 512);
        host->card.block_size = 512;

        pr_info("cherry_sdio: csd parsed: capacity=%lu mb, sectors=%u, blksize=512\n",
                (unsigned long)(capacity / (1024 * 1024)), host->card.sector_count);
    }

    /* 7. CMD7: SELECT_CARD (enter Transfer State) */
    sdhci_send_cmd(host, CMD7_SELECT_CARD, (uint32_t)rca << 16,
                   SDHCI_CMD_RESP_48_BUSY | SDHCI_CMD_CRC_CHECK, resp);

    /* 8. CMD16: SET_BLOCKLEN to 512 bytes */
    sdhci_send_cmd(host, CMD16_SET_BLOCKLEN, 512,
                   SDHCI_CMD_RESP_48 | SDHCI_CMD_CRC_CHECK, resp);

    /* 9. Switch clock to 25 MHz for full speed transfer */
    sdhci_set_clock(host, 25000000);

    pr_info("cherry_sdio: card fully configured and in transfer state\n");
    return 0;
}
