/*
 * CherrySDIO - SD / SDIO / MMC / eMMC Protocol Definitions
 */

#ifndef CHERRY_SD_PROTOCOL_H
#define CHERRY_SD_PROTOCOL_H

#include <stdint.h>
#include <stdbool.h>

/* Standard SD / MMC Commands */
#define CMD0_GO_IDLE_STATE              0
#define CMD1_SEND_OP_COND               1   /* eMMC */
#define CMD2_ALL_SEND_CID               2
#define CMD3_SET_RELATIVE_ADDR          3
#define CMD7_SELECT_CARD                7
#define CMD8_SEND_IF_COND               8
#define CMD9_SEND_CSD                   9
#define CMD12_STOP_TRANSMISSION         12
#define CMD13_SEND_STATUS               13
#define CMD16_SET_BLOCKLEN              16
#define CMD17_READ_SINGLE_BLOCK         17
#define CMD18_READ_MULTIPLE_BLOCK       18
#define CMD24_WRITE_SINGLE_BLOCK        24
#define CMD25_WRITE_MULTIPLE_BLOCK      25
#define CMD55_APP_CMD                   55
#define ACMD41_SD_SEND_OP_COND          41
#define ACMD51_SEND_SCR                 51

/* OCR (Operation Conditions Register) Flags */
#define SD_OCR_BUSY                     (1U << 31)  /* 1 = Card finished powerup */
#define SD_OCR_CCS                      (1U << 30)  /* 1 = Card Capacity Status (SDHC/SDXC) */
#define SD_OCR_HCS                      (1U << 30)  /* 1 = Host Capacity Support (SDHC/SDXC) */
#define SD_OCR_VOLTAGE_WINDOW           0x00FF8000U /* 2.7V - 3.6V */

/* CMD8 Check Pattern */
#define SD_CMD8_VOLTAGE_27_36           (1U << 8)
#define SD_CMD8_CHECK_PATTERN           0x000000AAU

/* Card Types */
enum sd_card_type {
    SD_CARD_TYPE_UNKNOWN = 0,
    SD_CARD_TYPE_SDSC,          /* Standard Capacity (<2GB) */
    SD_CARD_TYPE_SDHC,          /* High Capacity (2GB-32GB) */
    SD_CARD_TYPE_SDXC,          /* Extended Capacity (>32GB) */
    SD_CARD_TYPE_EMMC,          /* eMMC chip */
};

/* Parsed Card Information */
struct sd_card_info {
    enum sd_card_type type;
    uint16_t rca;
    uint32_t raw_cid[4];
    uint32_t raw_csd[4];
    uint64_t capacity_bytes;
    uint32_t sector_count;
    uint32_t block_size;
    bool     high_capacity;
    char     product_name[8];
    uint32_t serial_number;
};

#endif /* CHERRY_SD_PROTOCOL_H */
