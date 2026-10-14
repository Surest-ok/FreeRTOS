/*
 * CherrySDIO - Standard SDHCI Host Controller Driver
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "sdhci.h"
#include "sd_protocol.h"
#include "cherry_printk.h"

static inline uint32_t sdhci_r32(struct sdhci_host *host, int reg)
{
    return *(volatile uint32_t *)(host->mmio_base + reg);
}

static inline uint16_t sdhci_r16(struct sdhci_host *host, int reg)
{
    return *(volatile uint16_t *)(host->mmio_base + reg);
}

static inline uint8_t sdhci_r8(struct sdhci_host *host, int reg)
{
    return *(volatile uint8_t *)(host->mmio_base + reg);
}

static inline void sdhci_w32(struct sdhci_host *host, int reg, uint32_t val)
{
    *(volatile uint32_t *)(host->mmio_base + reg) = val;
}

static inline void sdhci_w16(struct sdhci_host *host, int reg, uint16_t val)
{
    *(volatile uint16_t *)(host->mmio_base + reg) = val;
}

static inline void sdhci_w8(struct sdhci_host *host, int reg, uint8_t val)
{
    *(volatile uint8_t *)(host->mmio_base + reg) = val;
}

int sdhci_reset(struct sdhci_host *host, uint8_t mask)
{
    sdhci_w8(host, SDHCI_SOFTWARE_RESET, mask);
    volatile int timeout = 100000;
    while ((sdhci_r8(host, SDHCI_SOFTWARE_RESET) & mask) && --timeout) {
        __asm__ volatile("yield");
    }
    return timeout > 0 ? 0 : -1;
}

int sdhci_set_clock(struct sdhci_host *host, uint32_t freq_hz)
{
    /* Disable SD Clock */
    sdhci_w16(host, SDHCI_CLOCK_CONTROL, 0);

    /* Get base clock from capabilities (bits 15..8 in MHz) */
    uint32_t caps = sdhci_r32(host, SDHCI_CAPABILITIES);
    uint32_t base_clock_mhz = (caps >> 8) & 0xFF;
    if (base_clock_mhz == 0) {
        base_clock_mhz = 52; /* default QEMU virt base clock */
    }
    uint32_t base_hz = base_clock_mhz * 1000000;

    /* Calculate 10-bit divider */
    uint16_t div = 1;
    if (freq_hz < base_hz) {
        div = (uint16_t)((base_hz + freq_hz - 1) / freq_hz);
        if (div > 256) div = 256;
    }

    uint16_t clk_val = ((div / 2) << 8) | SDHCI_CLOCK_INT_EN;
    sdhci_w16(host, SDHCI_CLOCK_CONTROL, clk_val);

    /* Wait for Internal Clock Stable */
    volatile int timeout = 100000;
    while (!(sdhci_r16(host, SDHCI_CLOCK_CONTROL) & SDHCI_CLOCK_INT_STABLE) && --timeout) {
        __asm__ volatile("yield");
    }
    if (timeout <= 0) {
        return -1;
    }

    /* Enable Card Clock */
    clk_val |= SDHCI_CLOCK_CARD_EN;
    sdhci_w16(host, SDHCI_CLOCK_CONTROL, clk_val);
    return 0;
}

int sdhci_init(struct sdhci_host *host, uintptr_t mmio, uint32_t irq, bool msi_mode)
{
    host->mmio_base = mmio;
    host->irq = irq;
    host->msi_enabled = msi_mode;
    host->last_intr = 0;

    /* 1. Reset all */
    sdhci_reset(host, SDHCI_RESET_ALL);

    /* 2. Power On (3.3V) */
    sdhci_w8(host, SDHCI_POWER_CONTROL, SDHCI_POWER_330 | SDHCI_POWER_ON);

    /* 3. Set identification clock (400 kHz) */
    sdhci_set_clock(host, 400000);

    /* 4. Enable interrupt status & signals */
    sdhci_w32(host, SDHCI_INT_ENABLE, SDHCI_INT_ALL_MASK);
    sdhci_w32(host, SDHCI_SIGNAL_ENABLE, 0);

    /* 5. Set timeout control */
    sdhci_w8(host, SDHCI_TIMEOUT_CONTROL, 0x0E);

    /* Check card presence */
    uint32_t state = sdhci_r32(host, SDHCI_PRESENT_STATE);
    host->card_inserted = (state & SDHCI_CARD_PRESENT) != 0;

    pr_info("cherry_sdio: sdhci controller initialized mmio=0x%lx irq=%d card_present=%d\n",
            (unsigned long)mmio, (int)irq, host->card_inserted);
    return 0;
}

int sdhci_send_cmd(struct sdhci_host *host, uint16_t cmd, uint32_t arg, uint16_t flags, uint32_t *resp)
{
    /* Wait for CMD inhibit to clear */
    volatile int timeout = 100000;
    while ((sdhci_r32(host, SDHCI_PRESENT_STATE) & SDHCI_CMD_INHIBIT) && --timeout) {
        __asm__ volatile("yield");
    }
    if (timeout <= 0) {
        return -1;
    }

    /* If command has data, wait for DATA inhibit */
    if (flags & SDHCI_CMD_DATA_PRESENT) {
        timeout = 100000;
        while ((sdhci_r32(host, SDHCI_PRESENT_STATE) & SDHCI_DATA_INHIBIT) && --timeout) {
            __asm__ volatile("yield");
        }
        if (timeout <= 0) return -2;
    }

    /* Reset host intr status and clear hardware flags */
    host->last_intr = 0;
    sdhci_w32(host, SDHCI_INT_STATUS, SDHCI_INT_ALL_MASK);

    /* Write Argument and Command */
    sdhci_w32(host, SDHCI_ARGUMENT, arg);
    sdhci_w16(host, SDHCI_COMMAND, (cmd << 8) | flags);

    /* Wait for Command Complete or Error */
    timeout = 100000;
    uint32_t stat = 0;
    while (--timeout) {
        stat = sdhci_r32(host, SDHCI_INT_STATUS) | host->last_intr;
        if (stat & (SDHCI_INT_CMD_COMPLETE | SDHCI_INT_ERROR)) {
            break;
        }
        __asm__ volatile("yield");
    }

    if (timeout <= 0 || (stat & SDHCI_INT_ERROR)) {
        sdhci_w32(host, SDHCI_INT_STATUS, SDHCI_INT_ALL_MASK);
        host->last_intr = 0;
        return -3;
    }

    /* Read response before acknowledging command complete */
    if (resp != 0) {
        if ((flags & 0x03) == SDHCI_CMD_RESP_136) {
            resp[0] = sdhci_r32(host, SDHCI_RESPONSE0);
            resp[1] = sdhci_r32(host, SDHCI_RESPONSE1);
            resp[2] = sdhci_r32(host, SDHCI_RESPONSE2);
            resp[3] = sdhci_r32(host, SDHCI_RESPONSE3);
        } else if ((flags & 0x03) != SDHCI_CMD_RESP_NONE) {
            resp[0] = sdhci_r32(host, SDHCI_RESPONSE0);
        }
    }

    /* Clear Command Complete flag */
    sdhci_w32(host, SDHCI_INT_STATUS, SDHCI_INT_CMD_COMPLETE);
    host->last_intr &= ~SDHCI_INT_CMD_COMPLETE;

    return 0;
}

int sdhci_read_blocks(struct sdhci_host *host, uint32_t lba, uint32_t count, void *buf)
{
    if (count == 0 || buf == 0) return 0;

    uint32_t *dst = (uint32_t *)buf;
    sdhci_w16(host, SDHCI_BLOCK_SIZE, 512);
    sdhci_w16(host, SDHCI_BLOCK_COUNT, (uint16_t)count);

    uint16_t transfer_mode = SDHCI_TRNS_READ | SDHCI_TRNS_BLK_CNT_EN;
    uint16_t cmd = CMD17_READ_SINGLE_BLOCK;

    if (count > 1) {
        transfer_mode |= SDHCI_TRNS_MULTI | SDHCI_TRNS_AUTO_CMD12;
        cmd = CMD18_READ_MULTIPLE_BLOCK;
    }

    sdhci_w16(host, SDHCI_TRANSFER_MODE, transfer_mode);

    uint32_t arg = host->card.high_capacity ? lba : (lba * 512);
    uint16_t cmd_flags = (cmd << 8) | SDHCI_CMD_DATA_PRESENT | SDHCI_CMD_RESP_48 | SDHCI_CMD_CRC_CHECK | SDHCI_CMD_INDEX_CHECK;

    host->last_intr = 0;
    sdhci_w32(host, SDHCI_INT_STATUS, SDHCI_INT_ALL_MASK);
    sdhci_w32(host, SDHCI_ARGUMENT, arg);
    sdhci_w16(host, SDHCI_COMMAND, cmd_flags);

    for (uint32_t b = 0; b < count; b++) {
        volatile int timeout = 200000;
        uint32_t stat = 0;
        while (--timeout) {
            stat = sdhci_r32(host, SDHCI_INT_STATUS) | host->last_intr;
            if (stat & (SDHCI_INT_BUF_READ_READY | SDHCI_INT_ERROR)) {
                break;
            }
            __asm__ volatile("yield");
        }
        if (timeout <= 0 || (stat & SDHCI_INT_ERROR)) {
            return -1;
        }

        sdhci_w32(host, SDHCI_INT_STATUS, SDHCI_INT_BUF_READ_READY);
        host->last_intr &= ~SDHCI_INT_BUF_READ_READY;

        /* Read 128 x 32-bit words (512 bytes) from FIFO */
        for (int i = 0; i < 128; i++) {
            *dst++ = sdhci_r32(host, SDHCI_BUFFER);
        }
    }

    /* Wait for Transfer Complete */
    volatile int timeout = 100000;
    while (--timeout) {
        uint32_t stat = sdhci_r32(host, SDHCI_INT_STATUS) | host->last_intr;
        if (stat & (SDHCI_INT_XFER_COMPLETE | SDHCI_INT_ERROR)) {
            break;
        }
        __asm__ volatile("yield");
    }
    sdhci_w32(host, SDHCI_INT_STATUS, SDHCI_INT_ALL_MASK);
    host->last_intr = 0;
    return 0;
}

int sdhci_write_blocks(struct sdhci_host *host, uint32_t lba, uint32_t count, const void *buf)
{
    if (count == 0 || buf == 0) return 0;

    const uint32_t *src = (const uint32_t *)buf;
    sdhci_w16(host, SDHCI_BLOCK_SIZE, 512);
    sdhci_w16(host, SDHCI_BLOCK_COUNT, (uint16_t)count);

    uint16_t transfer_mode = SDHCI_TRNS_BLK_CNT_EN;
    uint16_t cmd = CMD24_WRITE_SINGLE_BLOCK;

    if (count > 1) {
        transfer_mode |= SDHCI_TRNS_MULTI | SDHCI_TRNS_AUTO_CMD12;
        cmd = CMD25_WRITE_MULTIPLE_BLOCK;
    }

    sdhci_w16(host, SDHCI_TRANSFER_MODE, transfer_mode);

    uint32_t arg = host->card.high_capacity ? lba : (lba * 512);
    uint16_t cmd_flags = (cmd << 8) | SDHCI_CMD_DATA_PRESENT | SDHCI_CMD_RESP_48 | SDHCI_CMD_CRC_CHECK | SDHCI_CMD_INDEX_CHECK;

    host->last_intr = 0;
    sdhci_w32(host, SDHCI_INT_STATUS, SDHCI_INT_ALL_MASK);
    sdhci_w32(host, SDHCI_ARGUMENT, arg);
    sdhci_w16(host, SDHCI_COMMAND, cmd_flags);

    for (uint32_t b = 0; b < count; b++) {
        volatile int timeout = 200000;
        uint32_t stat = 0;
        while (--timeout) {
            stat = sdhci_r32(host, SDHCI_INT_STATUS) | host->last_intr;
            if (stat & (SDHCI_INT_BUF_WRITE_READY | SDHCI_INT_ERROR)) {
                break;
            }
            __asm__ volatile("yield");
        }
        if (timeout <= 0 || (stat & SDHCI_INT_ERROR)) {
            return -1;
        }

        sdhci_w32(host, SDHCI_INT_STATUS, SDHCI_INT_BUF_WRITE_READY);
        host->last_intr &= ~SDHCI_INT_BUF_WRITE_READY;

        /* Write 128 x 32-bit words (512 bytes) to FIFO */
        for (int i = 0; i < 128; i++) {
            sdhci_w32(host, SDHCI_BUFFER, *src++);
        }
    }

    /* Wait for Transfer Complete */
    volatile int timeout = 100000;
    while (--timeout) {
        uint32_t stat = sdhci_r32(host, SDHCI_INT_STATUS) | host->last_intr;
        if (stat & (SDHCI_INT_XFER_COMPLETE | SDHCI_INT_ERROR)) {
            break;
        }
        __asm__ volatile("yield");
    }
    sdhci_w32(host, SDHCI_INT_STATUS, SDHCI_INT_ALL_MASK);
    host->last_intr = 0;
    return 0;
}

void sdhci_isr(void *arg)
{
    struct sdhci_host *host = (struct sdhci_host *)arg;
    if (host == 0) return;

    uint32_t stat = sdhci_r32(host, SDHCI_INT_STATUS);
    host->last_intr |= stat;

    /* Acknowledge and clear interrupt */
    sdhci_w32(host, SDHCI_INT_STATUS, stat);
}
