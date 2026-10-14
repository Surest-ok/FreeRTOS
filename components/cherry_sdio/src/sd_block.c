/*
 * CherrySDIO - CherryVFS Block Device Bridge (/dev/mmcblk0)
 */

#include <stdint.h>
#include "sdhci.h"
#include "cherry_sdio.h"
#include "cherry_vfs.h"
#include "vfs_dev.h"
#include "cherry_printk.h"

static int sd_bdev_open(struct vfs_bdev *bdev, int flags)
{
    (void)bdev;
    (void)flags;
    return 0;
}

static int sd_bdev_close(struct vfs_bdev *bdev)
{
    (void)bdev;
    return 0;
}

static int sd_bdev_read(struct vfs_bdev *bdev, void *buf, uint64_t lba, size_t num_blocks)
{
    struct sdhci_host *host = (struct sdhci_host *)bdev->dev.priv_data;
    if (host == 0) return -1;
    return sdhci_read_blocks(host, (uint32_t)lba, (uint32_t)num_blocks, buf);
}

static int sd_bdev_write(struct vfs_bdev *bdev, const void *buf, uint64_t lba, size_t num_blocks)
{
    struct sdhci_host *host = (struct sdhci_host *)bdev->dev.priv_data;
    if (host == 0) return -1;
    return sdhci_write_blocks(host, (uint32_t)lba, (uint32_t)num_blocks, buf);
}

static int sd_bdev_sync(struct vfs_bdev *bdev)
{
    (void)bdev;
    return 0;
}

static int sd_bdev_ioctl(struct vfs_bdev *bdev, unsigned int cmd, unsigned long arg)
{
    struct sdhci_host *host = (struct sdhci_host *)bdev->dev.priv_data;
    if (host == 0) return -1;

    switch (cmd) {
    case BLOCK_IOCTL_GET_SECTOR_SIZE:
    case BLOCK_IOCTL_GET_BLOCK_SIZE:
        if (arg) *(uint32_t *)arg = 512;
        return 0;
    case BLOCK_IOCTL_GET_SECTOR_COUNT:
        if (arg) *(uint64_t *)arg = host->card.sector_count;
        return 0;
    case BLOCK_IOCTL_SYNC:
        return 0;
    default:
        return -22;
    }
}

static const struct vfs_bdev_ops s_sd_bdev_ops = {
    .open         = sd_bdev_open,
    .close        = sd_bdev_close,
    .read_blocks  = sd_bdev_read,
    .write_blocks = sd_bdev_write,
    .sync         = sd_bdev_sync,
    .ioctl        = sd_bdev_ioctl,
};

static struct vfs_bdev s_sd_bdev;
static struct vfs_bdev s_sd_alias_bdev;

void sd_block_register(struct sdhci_host *host)
{
    uint32_t sectors = host->card.sector_count ? host->card.sector_count : 131072; /* 64MB default */

    s_sd_bdev.ops = &s_sd_bdev_ops;
    s_sd_bdev.dev.priv_data = host;
    vfs_bdev_register(&s_sd_bdev, "mmcblk0", 512, sectors);

    s_sd_alias_bdev.ops = &s_sd_bdev_ops;
    s_sd_alias_bdev.dev.priv_data = host;
    vfs_bdev_register(&s_sd_alias_bdev, "sd0", 512, sectors);

    pr_info("cherry_sdio: registered block device /dev/mmcblk0 (%u sectors, 512b/sector)\n", sectors);
}
