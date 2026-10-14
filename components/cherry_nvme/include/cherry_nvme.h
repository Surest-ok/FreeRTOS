/*
 * CherryNVMe - NVMe Subsystem for CherryOS / FreeRTOS
 *
 * Copyright (c) 2026 ForgeStitch
 * SPDX-License-Identifier: MIT
 */

#ifndef _CHERRY_NVME_H_
#define _CHERRY_NVME_H_

#include <stdint.h>
#include <stdbool.h>
#include "nvme_spec.h"
#include "cherry_pci.h"
#include "cherry_vfs.h"
#include "vfs_dev.h"

#define NVME_AQ_DEPTH       32
#define NVME_IO_DEPTH       64

struct nvme_dev;

struct nvme_queue {
    struct nvme_sqe    *sq;
    struct nvme_cqe    *cq;
    volatile uint32_t  *sq_db;
    volatile uint32_t  *cq_db;
    uint16_t            depth;
    uint16_t            sq_tail;
    uint16_t            cq_head;
    uint8_t             cq_phase;
    uint16_t            qid;
};

struct nvme_ns {
    struct nvme_dev    *dev;
    uint32_t            nsid;
    uint64_t            block_count;
    uint32_t            block_size;
    struct vfs_bdev     bdev;
    struct vfs_bdev     alias_bdev;
};

struct nvme_dev {
    struct pci_dev     *pdev;
    uintptr_t           bar0;
    uint32_t            stride;
    uint32_t            db_stride;
    char                serial[21];
    char                model[41];
    char                firmware[9];
    uint32_t            num_namespaces;

    /* Admin and I/O Queues */
    struct nvme_queue   admin_q;
    struct nvme_queue   io_q;

    /* Primary Namespace */
    struct nvme_ns      ns;
    bool                is_ready;
};

/* Subsystem Management */
int  cherry_nvme_init(void);
void cherry_nvme_run_tests(void);
struct nvme_dev *nvme_get_primary_dev(void);

/* Low-level Block I/O Operations */
int  nvme_read_blocks(struct nvme_dev *dev, uint32_t nsid, uint64_t lba, uint32_t count, void *buf);
int  nvme_write_blocks(struct nvme_dev *dev, uint32_t nsid, uint64_t lba, uint32_t count, const void *buf);
int  nvme_flush(struct nvme_dev *dev, uint32_t nsid);

/* Shell Diagnostic Commands */
void nvme_register_shell_commands(void);

#endif /* _CHERRY_NVME_H_ */
