/*
 * CherrySDIO - CherryShell Inspection Commands (sd_info, sd_read, sd_write)
 */

#include <stdint.h>
#include <string.h>
#include "cherry_sdio.h"
#include "sdhci.h"
#include "cherry_shell.h"
#include "cherry_printk.h"

static uint32_t simple_strtoul(const char *str)
{
    uint32_t val = 0;
    while (*str >= '0' && *str <= '9') {
        val = val * 10 + (*str - '0');
        str++;
    }
    return val;
}

int cmd_sd_info(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    struct sdhci_host *host = sdio_get_primary_host();
    if (host == 0) {
        printk("sd: no active sdhci host or card detected\n");
        return -1;
    }

    const char *type_str = "unknown";
    switch (host->card.type) {
    case SD_CARD_TYPE_SDSC: type_str = "sdsc (<2gb)"; break;
    case SD_CARD_TYPE_SDHC: type_str = "sdhc (2-32gb)"; break;
    case SD_CARD_TYPE_SDXC: type_str = "sdxc (>32gb)"; break;
    case SD_CARD_TYPE_EMMC: type_str = "emmc"; break;
    default: break;
    }

    printk("=== cherry_sdio: card status ===\n");
    printk("card type   : %s\n", type_str);
    printk("product name: %s\n", host->card.product_name);
    printk("rca address : 0x%04x\n", host->card.rca);
    printk("capacity    : %lu mb (%u sectors)\n",
           (unsigned long)(host->card.capacity_bytes / (1024 * 1024)),
           host->card.sector_count);
    printk("block size  : %u bytes\n", host->card.block_size);
    printk("serial no.  : 0x%08x\n", host->card.serial_number);
    printk("msi / its   : %s\n", host->msi_enabled ? "enabled (gic its)" : "legacy intx");
    return 0;
}

int cmd_sd_read(int argc, char **argv)
{
    if (argc < 2) {
        printk("usage: sd_read <lba> [count]\n");
        return -1;
    }

    uint32_t lba = simple_strtoul(argv[1]);
    uint32_t count = argc >= 3 ? simple_strtoul(argv[2]) : 1;

    struct sdhci_host *host = sdio_get_primary_host();
    if (host == 0) {
        printk("sd: no active host\n");
        return -1;
    }

    uint8_t buffer[512];
    for (uint32_t i = 0; i < count; i++) {
        if (sdhci_read_blocks(host, lba + i, 1, buffer) != 0) {
            printk("sd_read failed at lba %u\n", lba + i);
            return -1;
        }

        printk("--- LBA %u ---\n", lba + i);
        for (int row = 0; row < 64; row += 16) {
            printk("%04x: %02x %02x %02x %02x %02x %02x %02x %02x  %02x %02x %02x %02x %02x %02x %02x %02x\n",
                   row,
                   buffer[row+0], buffer[row+1], buffer[row+2], buffer[row+3],
                   buffer[row+4], buffer[row+5], buffer[row+6], buffer[row+7],
                   buffer[row+8], buffer[row+9], buffer[row+10], buffer[row+11],
                   buffer[row+12], buffer[row+13], buffer[row+14], buffer[row+15]);
        }
    }
    return 0;
}

int cmd_sd_write(int argc, char **argv)
{
    if (argc < 3) {
        printk("usage: sd_write <lba> <string>\n");
        return -1;
    }

    uint32_t lba = simple_strtoul(argv[1]);
    const char *text = argv[2];

    struct sdhci_host *host = sdio_get_primary_host();
    if (host == 0) {
        printk("sd: no active host\n");
        return -1;
    }

    uint8_t buffer[512];
    memset(buffer, 0, sizeof(buffer));
    strncpy((char *)buffer, text, sizeof(buffer) - 1);

    if (sdhci_write_blocks(host, lba, 1, buffer) != 0) {
        printk("sd_write failed at lba %u\n", lba);
        return -1;
    }

    printk("successfully wrote text to lba %u\n", lba);
    return 0;
}

void sd_register_shell_commands(void)
{
    cherry_shell_register_symbol("sd_info", (uintptr_t)cmd_sd_info, CHERRY_SHELL_SYM_CMD, "display sd/emmc card info and status");
    cherry_shell_register_symbol("sd_read", (uintptr_t)cmd_sd_read, CHERRY_SHELL_SYM_CMD, "read and dump sectors from sd card: sd_read <lba> [count]");
    cherry_shell_register_symbol("sd_write", (uintptr_t)cmd_sd_write, CHERRY_SHELL_SYM_CMD, "write text data to sd sector: sd_write <lba> <string>");
}
