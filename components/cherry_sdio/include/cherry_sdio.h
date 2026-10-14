/*
 * CherrySDIO - Modern SD / SDIO / MMC / eMMC Subsystem for CherryOS / FreeRTOS
 */

#ifndef CHERRY_SDIO_H
#define CHERRY_SDIO_H

#include <stdint.h>
#include <stdbool.h>
#include "sd_protocol.h"
#include "sdhci.h"

int  cherry_sdio_init(void);
void cherry_sdio_run_tests(void);
struct sdhci_host *sdio_get_primary_host(void);

/* Block Transfer APIs */
int  sdio_read(uint32_t lba, uint32_t count, void *buf);
int  sdio_write(uint32_t lba, uint32_t count, const void *buf);
const struct sd_card_info *sdio_get_card_info(void);

#endif /* CHERRY_SDIO_H */
