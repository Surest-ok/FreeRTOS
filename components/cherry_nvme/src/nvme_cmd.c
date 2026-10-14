/*
 * CherryNVMe - NVMe Command Submission and Completion Engine
 *
 * Copyright (c) 2026 ForgeStitch
 * SPDX-License-Identifier: MIT
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "cherry_nvme.h"
#include "nvme_spec.h"
#include "cherry_printk.h"

static uint16_t s_admin_cid = 0;
static uint16_t s_io_cid = 0;

static inline void nvme_dmb(void)
{
    __asm__ volatile("dmb sy" ::: "memory");
}

int nvme_submit_admin_cmd(struct nvme_dev *dev, struct nvme_sqe *cmd, struct nvme_cqe *cqe_out)
{
    struct nvme_queue *q = &dev->admin_q;
    if (!q->sq || !q->cq) return -1;

    cmd->cid = ++s_admin_cid;
    q->sq[q->sq_tail] = *cmd;

    nvme_dmb();

    q->sq_tail = (q->sq_tail + 1) % q->depth;
    *q->sq_db = q->sq_tail;

    /* Poll completion queue */
    volatile struct nvme_cqe *cqe = &q->cq[q->cq_head];
    uint32_t timeout = 5000000;

    while (((cqe->status & 0x1) != q->cq_phase) && --timeout) {
        __asm__ volatile("yield");
    }

    if (timeout == 0) {
        pr_err("cherry_nvme: admin command 0x%02x timed out!\n", cmd->opcode);
        return -1;
    }

    if (cqe_out) {
        *cqe_out = *cqe;
    }

    uint16_t status = cqe->status >> 1;

    /* Advance CQ Head */
    q->cq_head = (q->cq_head + 1) % q->depth;
    if (q->cq_head == 0) {
        q->cq_phase ^= 1;
    }

    nvme_dmb();
    *q->cq_db = q->cq_head;

    if (status != 0) {
        pr_err("cherry_nvme: admin cmd 0x%02x failed with status 0x%04x\n", cmd->opcode, status);
        return -1;
    }

    return 0;
}

int nvme_submit_io_cmd(struct nvme_dev *dev, struct nvme_sqe *cmd, struct nvme_cqe *cqe_out)
{
    struct nvme_queue *q = &dev->io_q;
    if (!q->sq || !q->cq) return -1;

    cmd->cid = ++s_io_cid;
    q->sq[q->sq_tail] = *cmd;

    nvme_dmb();

    q->sq_tail = (q->sq_tail + 1) % q->depth;
    *q->sq_db = q->sq_tail;

    /* Poll completion queue */
    volatile struct nvme_cqe *cqe = &q->cq[q->cq_head];
    uint32_t timeout = 5000000;

    while (((cqe->status & 0x1) != q->cq_phase) && --timeout) {
        __asm__ volatile("yield");
    }

    if (timeout == 0) {
        pr_err("cherry_nvme: io command 0x%02x timed out!\n", cmd->opcode);
        return -1;
    }

    if (cqe_out) {
        *cqe_out = *cqe;
    }

    uint16_t status = cqe->status >> 1;

    /* Advance CQ Head */
    q->cq_head = (q->cq_head + 1) % q->depth;
    if (q->cq_head == 0) {
        q->cq_phase ^= 1;
    }

    nvme_dmb();
    *q->cq_db = q->cq_head;

    if (status != 0) {
        pr_err("cherry_nvme: io cmd 0x%02x failed with status 0x%04x\n", cmd->opcode, status);
        return -1;
    }

    return 0;
}

int nvme_cmd_identify_ctrl(struct nvme_dev *dev, struct nvme_id_ctrl *id_ctrl)
{
    struct nvme_sqe sqe;
    memset(&sqe, 0, sizeof(sqe));
    sqe.opcode = NVME_ADMIN_OP_IDENTIFY;
    sqe.nsid = 0;
    sqe.prp1 = (uint64_t)(uintptr_t)id_ctrl;
    sqe.cdw10 = NVME_ID_CNS_CTRL;

    return nvme_submit_admin_cmd(dev, &sqe, NULL);
}

int nvme_cmd_create_io_cq(struct nvme_dev *dev, struct nvme_queue *q)
{
    struct nvme_sqe sqe;
    memset(&sqe, 0, sizeof(sqe));
    sqe.opcode = NVME_ADMIN_OP_CREATE_CQ;
    sqe.prp1 = (uint64_t)(uintptr_t)q->cq;
    sqe.cdw10 = ((uint32_t)(q->depth - 1) << 16) | q->qid;
    sqe.cdw11 = 1; /* Physically Contiguous, Interrupts Disabled */

    return nvme_submit_admin_cmd(dev, &sqe, NULL);
}

int nvme_cmd_create_io_sq(struct nvme_dev *dev, struct nvme_queue *q)
{
    struct nvme_sqe sqe;
    memset(&sqe, 0, sizeof(sqe));
    sqe.opcode = NVME_ADMIN_OP_CREATE_SQ;
    sqe.prp1 = (uint64_t)(uintptr_t)q->sq;
    sqe.cdw10 = ((uint32_t)(q->depth - 1) << 16) | q->qid;
    sqe.cdw11 = ((uint32_t)q->qid << 16) | 1; /* CQID = qid, Physically Contiguous */

    return nvme_submit_admin_cmd(dev, &sqe, NULL);
}

int nvme_cmd_identify_ns(struct nvme_dev *dev, uint32_t nsid, struct nvme_id_ns *id_ns)
{
    struct nvme_sqe sqe;
    memset(&sqe, 0, sizeof(sqe));
    sqe.opcode = NVME_ADMIN_OP_IDENTIFY;
    sqe.nsid = nsid;
    sqe.prp1 = (uint64_t)(uintptr_t)id_ns;
    sqe.cdw10 = NVME_ID_CNS_NS;

    return nvme_submit_admin_cmd(dev, &sqe, NULL);
}

int nvme_read_blocks(struct nvme_dev *dev, uint32_t nsid, uint64_t lba, uint32_t count, void *buf)
{
    if (!dev || !dev->is_ready || count == 0 || !buf) return -1;

    uint32_t sector_size = dev->ns.block_size ? dev->ns.block_size : 512;
    uint8_t *ptr = (uint8_t *)buf;

    while (count > 0) {
        uint32_t page_offset = (uintptr_t)ptr & 0xFFF;
        uint32_t bytes_in_page = 4096 - page_offset;
        uint32_t max_blocks = bytes_in_page / sector_size;
        if (max_blocks == 0) max_blocks = 1;

        uint32_t batch = count > max_blocks ? max_blocks : count;

        struct nvme_sqe sqe;
        memset(&sqe, 0, sizeof(sqe));
        sqe.opcode = NVME_NVM_OP_READ;
        sqe.nsid = nsid;
        sqe.prp1 = (uint64_t)(uintptr_t)ptr;
        sqe.cdw10 = (uint32_t)(lba & 0xFFFFFFFF);
        sqe.cdw11 = (uint32_t)(lba >> 32);
        sqe.cdw12 = (batch - 1) & 0xFFFF;

        int ret = nvme_submit_io_cmd(dev, &sqe, NULL);
        if (ret != 0) {
            pr_err("cherry_nvme: read failed at lba 0x%llx count %u\n", (unsigned long long)lba, batch);
            return ret;
        }

        lba += batch;
        ptr += batch * sector_size;
        count -= batch;
    }

    return 0;
}

int nvme_write_blocks(struct nvme_dev *dev, uint32_t nsid, uint64_t lba, uint32_t count, const void *buf)
{
    if (!dev || !dev->is_ready || count == 0 || !buf) return -1;

    uint32_t sector_size = dev->ns.block_size ? dev->ns.block_size : 512;
    const uint8_t *ptr = (const uint8_t *)buf;

    while (count > 0) {
        uint32_t page_offset = (uintptr_t)ptr & 0xFFF;
        uint32_t bytes_in_page = 4096 - page_offset;
        uint32_t max_blocks = bytes_in_page / sector_size;
        if (max_blocks == 0) max_blocks = 1;

        uint32_t batch = count > max_blocks ? max_blocks : count;

        struct nvme_sqe sqe;
        memset(&sqe, 0, sizeof(sqe));
        sqe.opcode = NVME_NVM_OP_WRITE;
        sqe.nsid = nsid;
        sqe.prp1 = (uint64_t)(uintptr_t)ptr;
        sqe.cdw10 = (uint32_t)(lba & 0xFFFFFFFF);
        sqe.cdw11 = (uint32_t)(lba >> 32);
        sqe.cdw12 = (batch - 1) & 0xFFFF;

        int ret = nvme_submit_io_cmd(dev, &sqe, NULL);
        if (ret != 0) {
            pr_err("cherry_nvme: write failed at lba 0x%llx count %u\n", (unsigned long long)lba, batch);
            return ret;
        }

        lba += batch;
        ptr += batch * sector_size;
        count -= batch;
    }

    return 0;
}

int nvme_flush(struct nvme_dev *dev, uint32_t nsid)
{
    if (!dev || !dev->is_ready) return -1;

    struct nvme_sqe sqe;
    memset(&sqe, 0, sizeof(sqe));
    sqe.opcode = NVME_NVM_OP_FLUSH;
    sqe.nsid = nsid;

    return nvme_submit_io_cmd(dev, &sqe, NULL);
}
