/*
 * CherrySDIO - Acceptance Test Suite
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "cherry_sdio.h"
#include "sdhci.h"
#include "cherry_vfs.h"
#include "vfs_posix.h"
#include "cherry_printk.h"

void cherry_sdio_run_tests(void)
{
    pr_info("cherry_sdio: ==> starting cherrysdio acceptance test suite...\n");

    struct sdhci_host *host = sdio_get_primary_host();
    bool sdhci_init_pass = (host != 0);
    bool card_detect_pass = false;
    bool ocr_pass = false;
    bool cid_pass = false;
    bool csd_pass = false;
    bool block_read_pass = false;
    bool block_write_pass = false;
    bool vfs_bdev_pass = false;

    if (host != 0) {
        card_detect_pass = host->card_inserted;
        ocr_pass = (host->card.type != SD_CARD_TYPE_UNKNOWN);
        cid_pass = (host->card.product_name[0] != 0);
        csd_pass = (host->card.sector_count > 0);

        /* 1. Direct block write test */
        uint8_t write_buf[512];
        for (int i = 0; i < 512; i++) {
            write_buf[i] = (uint8_t)(0xA5 ^ (i & 0xFF));
        }
        int w_ret = sdhci_write_blocks(host, 10, 1, write_buf);
        block_write_pass = (w_ret == 0);

        /* 2. Direct block read test and comparison */
        uint8_t read_buf[512];
        memset(read_buf, 0, sizeof(read_buf));
        int r_ret = sdhci_read_blocks(host, 10, 1, read_buf);
        block_read_pass = (r_ret == 0) && (memcmp(write_buf, read_buf, 512) == 0);

        /* 3. POSIX CherryVFS Block Device test (/dev/mmcblk0) */
        int fd = open("/dev/mmcblk0", O_RDWR, 0666);
        if (fd >= 0) {
            uint8_t vfs_w[512];
            for (int i = 0; i < 512; i++) vfs_w[i] = (uint8_t)(0x5A + i);
            lseek(fd, 20 * 512, SEEK_SET);
            write(fd, vfs_w, 512);

            uint8_t vfs_r[512];
            lseek(fd, 20 * 512, SEEK_SET);
            read(fd, vfs_r, 512);
            close(fd);

            vfs_bdev_pass = (memcmp(vfs_w, vfs_r, 512) == 0);
        }
    }

    pr_info("cherry_sdio: [sdio-verify] sdhci_init=%s card_detect=%s ocr=%s cid=%s csd=%s block_read=%s block_write=%s vfs_bdev=%s all=%s\n",
            sdhci_init_pass ? "pass" : "fail",
            card_detect_pass ? "pass" : "fail",
            ocr_pass ? "pass" : "fail",
            cid_pass ? "pass" : "fail",
            csd_pass ? "pass" : "fail",
            block_read_pass ? "pass" : "fail",
            block_write_pass ? "pass" : "fail",
            vfs_bdev_pass ? "pass" : "fail",
            (sdhci_init_pass && block_read_pass && block_write_pass && vfs_bdev_pass) ? "pass" : "fail");
}
