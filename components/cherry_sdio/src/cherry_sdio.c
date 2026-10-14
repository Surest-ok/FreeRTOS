/*
 * CherrySDIO - Subsystem Initializer
 */

#include <stdint.h>
#include "cherry_sdio.h"
#include "sdhci.h"
#include "cherry_printk.h"

extern int  sdhci_pci_register(void);
extern void sd_register_shell_commands(void);

int cherry_sdio_init(void)
{
    pr_info("cherry_sdio: initializing sd/emmc subsystem...\n");

    /* 1. Register SDHCI PCIe driver (triggers probe on discovery) */
    sdhci_pci_register();

    /* 2. Register Shell commands */
    sd_register_shell_commands();

    pr_info("cherry_sdio: subsystem initialized successfully\n");
    return 0;
}

int sdio_read(uint32_t lba, uint32_t count, void *buf)
{
    struct sdhci_host *host = sdio_get_primary_host();
    if (host == 0) return -1;
    return sdhci_read_blocks(host, lba, count, buf);
}

int sdio_write(uint32_t lba, uint32_t count, const void *buf)
{
    struct sdhci_host *host = sdio_get_primary_host();
    if (host == 0) return -1;
    return sdhci_write_blocks(host, lba, count, buf);
}

const struct sd_card_info *sdio_get_card_info(void)
{
    struct sdhci_host *host = sdio_get_primary_host();
    return host ? &host->card : 0;
}
