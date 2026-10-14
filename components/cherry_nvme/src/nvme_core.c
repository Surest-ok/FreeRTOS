/*
 * CherryNVMe - Subsystem Core and PCIe Controller Driver
 *
 * Copyright (c) 2026 ForgeStitch
 * SPDX-License-Identifier: MIT
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "platform.h"
#include "cherry_pci.h"
#include "cherry_nvme.h"
#include "nvme_spec.h"
#include "cherry_printk.h"

static struct nvme_dev s_primary_nvme;
static bool s_primary_nvme_active = false;

/* Aligned Queue Memory */
static __attribute__((aligned(4096))) struct nvme_sqe s_admin_sq[NVME_AQ_DEPTH];
static __attribute__((aligned(4096))) struct nvme_cqe s_admin_cq[NVME_AQ_DEPTH];

static __attribute__((aligned(4096))) struct nvme_sqe s_io_sq[NVME_IO_DEPTH];
static __attribute__((aligned(4096))) struct nvme_cqe s_io_cq[NVME_IO_DEPTH];

static __attribute__((aligned(4096))) struct nvme_id_ctrl s_id_ctrl;
static __attribute__((aligned(4096))) struct nvme_id_ns   s_id_ns;

extern int  nvme_cmd_identify_ctrl(struct nvme_dev *dev, struct nvme_id_ctrl *id_ctrl);
extern int  nvme_cmd_create_io_cq(struct nvme_dev *dev, struct nvme_queue *q);
extern int  nvme_cmd_create_io_sq(struct nvme_dev *dev, struct nvme_queue *q);
extern int  nvme_cmd_identify_ns(struct nvme_dev *dev, uint32_t nsid, struct nvme_id_ns *id_ns);
extern int  nvme_bdev_register(struct nvme_dev *dev);

static void trim_trailing_spaces(char *str, size_t len)
{
    while (len > 0 && (str[len - 1] == ' ' || str[len - 1] == '\0')) {
        str[len - 1] = '\0';
        len--;
    }
}

static int nvme_pci_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
    (void)id;
    pr_info("cherry_nvme: probing nvme controller at [%02x:%02x.%d]...\n",
            pdev->bus, pdev->dev, pdev->fn);

    /* Enable device MMIO and bus master */
    pci_enable_device(pdev);
    pci_set_master(pdev);

    uintptr_t bar0 = (uintptr_t)pdev->resource[0];
    if (bar0 == 0) {
        pr_err("cherry_nvme: invalid bar0 address\n");
        return -1;
    }

    struct nvme_dev *dev = &s_primary_nvme;
    memset(dev, 0, sizeof(*dev));
    dev->pdev = pdev;
    dev->bar0 = bar0;

    /* Read Capabilities */
    uint32_t cap_lo = *(volatile uint32_t *)(bar0 + NVME_REG_CAP);
    uint32_t cap_hi = *(volatile uint32_t *)(bar0 + NVME_REG_CAP + 4);
    uint64_t cap = ((uint64_t)cap_hi << 32) | cap_lo;

    uint8_t dstrd = NVME_CAP_DSTRD(cap);
    dev->db_stride = 4 << dstrd;

    pr_info("cherry_nvme: cap=0x%llx dstrd=%u db_stride=%u mqes=%u\n",
            (unsigned long long)cap, dstrd, dev->db_stride, NVME_CAP_MQES(cap) + 1);

    /* 1. Reset / Disable Controller if currently active */
    uint32_t cc = *(volatile uint32_t *)(bar0 + NVME_REG_CC);
    if (cc & NVME_CC_EN) {
        *(volatile uint32_t *)(bar0 + NVME_REG_CC) = 0;
        uint32_t timeout = 100000;
        while ((*(volatile uint32_t *)(bar0 + NVME_REG_CSTS) & NVME_CSTS_RDY) && --timeout) {
            __asm__ volatile("yield");
        }
        if (timeout == 0) {
            pr_err("cherry_nvme: failed to disable controller (csts timeout)\n");
            return -1;
        }
    }

    /* 2. Configure Admin Queues */
    memset(s_admin_sq, 0, sizeof(s_admin_sq));
    memset(s_admin_cq, 0, sizeof(s_admin_cq));

    dev->admin_q.sq = s_admin_sq;
    dev->admin_q.cq = s_admin_cq;
    dev->admin_q.depth = NVME_AQ_DEPTH;
    dev->admin_q.sq_tail = 0;
    dev->admin_q.cq_head = 0;
    dev->admin_q.cq_phase = 1;
    dev->admin_q.qid = 0;
    dev->admin_q.sq_db = (volatile uint32_t *)(bar0 + NVME_REG_DBS + (2 * 0) * dev->db_stride);
    dev->admin_q.cq_db = (volatile uint32_t *)(bar0 + NVME_REG_DBS + (2 * 0 + 1) * dev->db_stride);

    *(volatile uint32_t *)(bar0 + NVME_REG_AQA) = ((uint32_t)(NVME_AQ_DEPTH - 1) << 16) | (NVME_AQ_DEPTH - 1);
    *(volatile uint32_t *)(bar0 + NVME_REG_ASQ) = (uint32_t)((uintptr_t)s_admin_sq & 0xFFFFFFFF);
    *(volatile uint32_t *)(bar0 + NVME_REG_ASQ + 4) = (uint32_t)((uint64_t)(uintptr_t)s_admin_sq >> 32);
    *(volatile uint32_t *)(bar0 + NVME_REG_ACQ) = (uint32_t)((uintptr_t)s_admin_cq & 0xFFFFFFFF);
    *(volatile uint32_t *)(bar0 + NVME_REG_ACQ + 4) = (uint32_t)((uint64_t)(uintptr_t)s_admin_cq >> 32);

    /* 3. Enable Controller */
    cc = NVME_CC_IOSQES(6) | NVME_CC_IOCQES(4) | NVME_CC_EN;
    *(volatile uint32_t *)(bar0 + NVME_REG_CC) = cc;

    uint32_t timeout = 100000;
    while (!(*(volatile uint32_t *)(bar0 + NVME_REG_CSTS) & NVME_CSTS_RDY) && --timeout) {
        __asm__ volatile("yield");
    }
    if (timeout == 0) {
        pr_err("cherry_nvme: controller rdy timeout\n");
        return -1;
    }

    /* 4. Identify Controller */
    memset(&s_id_ctrl, 0, sizeof(s_id_ctrl));
    if (nvme_cmd_identify_ctrl(dev, &s_id_ctrl) != 0) {
        pr_err("cherry_nvme: identify controller failed\n");
        return -1;
    }

    memcpy(dev->serial, s_id_ctrl.sn, 20);
    dev->serial[20] = '\0';
    trim_trailing_spaces(dev->serial, 20);

    memcpy(dev->model, s_id_ctrl.mn, 40);
    dev->model[40] = '\0';
    trim_trailing_spaces(dev->model, 40);

    memcpy(dev->firmware, s_id_ctrl.fr, 8);
    dev->firmware[8] = '\0';
    trim_trailing_spaces(dev->firmware, 8);

    dev->num_namespaces = s_id_ctrl.nn;

    pr_info("cherry_nvme: identified nvme device (model='%s', serial='%s', fw='%s', nn=%u)\n",
            dev->model, dev->serial, dev->firmware, dev->num_namespaces);

    /* 5. Create I/O Completion Queue (QID = 1) */
    memset(s_io_cq, 0, sizeof(s_io_cq));
    dev->io_q.cq = s_io_cq;
    dev->io_q.depth = NVME_IO_DEPTH;
    dev->io_q.cq_head = 0;
    dev->io_q.cq_phase = 1;
    dev->io_q.qid = 1;
    dev->io_q.cq_db = (volatile uint32_t *)(bar0 + NVME_REG_DBS + (2 * 1 + 1) * dev->db_stride);

    if (nvme_cmd_create_io_cq(dev, &dev->io_q) != 0) {
        pr_err("cherry_nvme: create io cq failed\n");
        return -1;
    }

    /* 6. Create I/O Submission Queue (QID = 1) */
    memset(s_io_sq, 0, sizeof(s_io_sq));
    dev->io_q.sq = s_io_sq;
    dev->io_q.sq_tail = 0;
    dev->io_q.sq_db = (volatile uint32_t *)(bar0 + NVME_REG_DBS + (2 * 1) * dev->db_stride);

    if (nvme_cmd_create_io_sq(dev, &dev->io_q) != 0) {
        pr_err("cherry_nvme: create io sq failed\n");
        return -1;
    }

    /* 7. Identify Namespace 1 */
    memset(&s_id_ns, 0, sizeof(s_id_ns));
    if (nvme_cmd_identify_ns(dev, 1, &s_id_ns) != 0) {
        pr_err("cherry_nvme: identify namespace 1 failed\n");
        return -1;
    }

    uint8_t lbads = s_id_ns.lbaf[s_id_ns.flbas & 0x0F].lbads;
    dev->ns.dev = dev;
    dev->ns.nsid = 1;
    dev->ns.block_size = 1U << lbads;
    dev->ns.block_count = s_id_ns.nsze;

    dev->is_ready = true;
    s_primary_nvme_active = true;

    /* 8. Register CherryVFS Block Device */
    nvme_bdev_register(dev);

    pr_info("cherry_nvme: initialized namespace 1 (%llu blocks, %u bytes/block, %llu mb)\n",
            (unsigned long long)dev->ns.block_count, dev->ns.block_size,
            (unsigned long long)(dev->ns.block_count * dev->ns.block_size / (1024 * 1024)));

    pdev->drv_data = dev;
    return 0;
}

static void nvme_pci_remove(struct pci_dev *pdev)
{
    (void)pdev;
    s_primary_nvme_active = false;
}

static const struct pci_device_id nvme_pci_tbl[] = {
    { PCI_VENDOR_ID_REDHAT, 0x0010, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0 },
    { PCI_ANY_ID, PCI_ANY_ID, PCI_ANY_ID, PCI_ANY_ID, 0x010802, 0xFFFF00, 0 },
    { 0, 0, 0, 0, 0, 0, 0 }
};

static struct pci_driver nvme_pci_driver = {
    .name = "nvme-pci",
    .id_table = nvme_pci_tbl,
    .probe = nvme_pci_probe,
    .remove = nvme_pci_remove,
};

int cherry_nvme_init(void)
{
    pr_info("cherry_nvme: initializing pci nvme subsystem...\n");

    /* Register NVMe PCIe Driver */
    pci_register_driver(&nvme_pci_driver);

    /* Register Shell Commands */
    nvme_register_shell_commands();

    pr_info("cherry_nvme: subsystem initialized successfully\n");
    return 0;
}

struct nvme_dev *nvme_get_primary_dev(void)
{
    return s_primary_nvme_active ? &s_primary_nvme : NULL;
}
