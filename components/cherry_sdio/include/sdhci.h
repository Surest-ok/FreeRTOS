/*
 * CherrySDIO - Standard SDHCI (SD Host Controller Interface) Registers
 */

#ifndef CHERRY_SDHCI_H
#define CHERRY_SDHCI_H

#include <stdint.h>
#include <stdbool.h>
#include "sd_protocol.h"

/* SDHCI Register Map */
#define SDHCI_DMA_ADDRESS       0x00
#define SDHCI_BLOCK_SIZE        0x04
#define SDHCI_BLOCK_COUNT       0x06
#define SDHCI_ARGUMENT          0x08
#define SDHCI_TRANSFER_MODE     0x0C
#define SDHCI_COMMAND           0x0E
#define SDHCI_RESPONSE0         0x10
#define SDHCI_RESPONSE1         0x14
#define SDHCI_RESPONSE2         0x18
#define SDHCI_RESPONSE3         0x1C
#define SDHCI_BUFFER            0x20
#define SDHCI_PRESENT_STATE     0x24
#define SDHCI_HOST_CONTROL      0x28
#define SDHCI_POWER_CONTROL     0x29
#define SDHCI_BLOCK_GAP_CONTROL 0x2A
#define SDHCI_WAKE_UP_CONTROL   0x2B
#define SDHCI_CLOCK_CONTROL     0x2C
#define SDHCI_TIMEOUT_CONTROL   0x2E
#define SDHCI_SOFTWARE_RESET    0x2F
#define SDHCI_INT_STATUS        0x30
#define SDHCI_INT_ENABLE        0x34
#define SDHCI_SIGNAL_ENABLE     0x38
#define SDHCI_ACMD12_ERR_STATUS 0x3C
#define SDHCI_CAPABILITIES      0x40
#define SDHCI_MAX_CURRENT       0x48
#define SDHCI_FORCE_INT         0x50
#define SDHCI_ADMA_ERR_STATUS   0x54
#define SDHCI_ADMA_ADDRESS      0x58
#define SDHCI_HOST_VERSION      0xFE

/* Present State Bits */
#define SDHCI_CMD_INHIBIT       (1U << 0)
#define SDHCI_DATA_INHIBIT      (1U << 1)
#define SDHCI_DAT_ACTIVE        (1U << 2)
#define SDHCI_WRITE_PROTECT     (1U << 19)
#define SDHCI_CARD_PRESENT      (1U << 16)
#define SDHCI_CARD_STABLE       (1U << 17)
#define SDHCI_BUFFER_READ_ENABLE (1U << 11)
#define SDHCI_BUFFER_WRITE_ENABLE (1U << 10)

/* Software Reset Bits */
#define SDHCI_RESET_ALL         0x01
#define SDHCI_RESET_CMD         0x02
#define SDHCI_RESET_DATA        0x04

/* Power Control Bits */
#define SDHCI_POWER_ON          0x01
#define SDHCI_POWER_330         (0x07 << 1)
#define SDHCI_POWER_300         (0x06 << 1)
#define SDHCI_POWER_180         (0x05 << 1)

/* Clock Control Bits */
#define SDHCI_CLOCK_INT_EN      (1U << 0)
#define SDHCI_CLOCK_INT_STABLE  (1U << 1)
#define SDHCI_CLOCK_CARD_EN     (1U << 2)

/* Interrupt Status Bits */
#define SDHCI_INT_CMD_COMPLETE      (1U << 0)
#define SDHCI_INT_XFER_COMPLETE     (1U << 1)
#define SDHCI_INT_BLOCK_GAP         (1U << 2)
#define SDHCI_INT_DMA_END           (1U << 3)
#define SDHCI_INT_BUF_WRITE_READY   (1U << 4)
#define SDHCI_INT_BUF_READ_READY    (1U << 5)
#define SDHCI_INT_CARD_INSERT       (1U << 6)
#define SDHCI_INT_CARD_REMOVE       (1U << 7)
#define SDHCI_INT_CARD_INT          (1U << 8)
#define SDHCI_INT_ERROR             (1U << 15)
#define SDHCI_INT_TIMEOUT           (1U << 16)
#define SDHCI_INT_CRC               (1U << 17)
#define SDHCI_INT_END_BIT           (1U << 18)
#define SDHCI_INT_INDEX             (1U << 19)
#define SDHCI_INT_DATA_TIMEOUT      (1U << 20)
#define SDHCI_INT_DATA_CRC          (1U << 21)
#define SDHCI_INT_DATA_END_BIT      (1U << 22)

#define SDHCI_INT_ALL_MASK          0xFFFFFFFFU

/* Transfer Mode Bits */
#define SDHCI_TRNS_DMA              (1U << 0)
#define SDHCI_TRNS_BLK_CNT_EN       (1U << 1)
#define SDHCI_TRNS_AUTO_CMD12       (1U << 2)
#define SDHCI_TRNS_READ             (1U << 4)
#define SDHCI_TRNS_MULTI            (1U << 5)

/* Command Flags */
#define SDHCI_CMD_RESP_NONE         0x00
#define SDHCI_CMD_RESP_136          0x01
#define SDHCI_CMD_RESP_48           0x02
#define SDHCI_CMD_RESP_48_BUSY      0x03
#define SDHCI_CMD_CRC_CHECK         (1U << 3)
#define SDHCI_CMD_INDEX_CHECK       (1U << 4)
#define SDHCI_CMD_DATA_PRESENT      (1U << 5)

/* Host Controller Context */
struct sdhci_host {
    uintptr_t mmio_base;
    uint32_t  irq;
    bool      msi_enabled;
    bool      card_inserted;
    struct sd_card_info card;
    volatile uint32_t last_intr;
    void     *pci_dev;
};

/* Host Controller API */
int  sdhci_init(struct sdhci_host *host, uintptr_t mmio, uint32_t irq, bool msi_mode);
int  sdhci_reset(struct sdhci_host *host, uint8_t mask);
int  sdhci_set_clock(struct sdhci_host *host, uint32_t freq_hz);
int  sdhci_send_cmd(struct sdhci_host *host, uint16_t cmd, uint32_t arg, uint16_t flags, uint32_t *resp);
int  sdhci_read_blocks(struct sdhci_host *host, uint32_t lba, uint32_t count, void *buf);
int  sdhci_write_blocks(struct sdhci_host *host, uint32_t lba, uint32_t count, const void *buf);
void sdhci_isr(void *arg);

#endif /* CHERRY_SDHCI_H */
