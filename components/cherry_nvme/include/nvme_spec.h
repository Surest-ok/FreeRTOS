/*
 * CherryNVMe - NVMe 1.4 Register and Structure Specifications
 *
 * Copyright (c) 2026 ForgeStitch
 * SPDX-License-Identifier: MIT
 */

#ifndef _NVME_SPEC_H_
#define _NVME_SPEC_H_

#include <stdint.h>

/* Controller Register Offsets (BAR0) */
#define NVME_REG_CAP        0x0000  /* Controller Capabilities (64-bit) */
#define NVME_REG_VS         0x0008  /* Version (32-bit) */
#define NVME_REG_INTMS      0x000C  /* Interrupt Mask Set (32-bit) */
#define NVME_REG_INTMC      0x0010  /* Interrupt Mask Clear (32-bit) */
#define NVME_REG_CC         0x0014  /* Controller Configuration (32-bit) */
#define NVME_REG_CSTS       0x001C  /* Controller Status (32-bit) */
#define NVME_REG_NSSR       0x0020  /* NVM Subsystem Reset (32-bit) */
#define NVME_REG_AQA        0x0024  /* Admin Queue Attributes (32-bit) */
#define NVME_REG_ASQ        0x0028  /* Admin Submission Queue Base (64-bit) */
#define NVME_REG_ACQ        0x0030  /* Admin Completion Queue Base (64-bit) */
#define NVME_REG_DBS        0x1000  /* Doorbell Base Offset */

/* CAP Register Bitfields */
#define NVME_CAP_MQES(cap)      ((uint16_t)((cap) & 0xFFFF))
#define NVME_CAP_CQS(cap)       ((bool)(((cap) >> 16) & 0x1))
#define NVME_CAP_AMS(cap)       ((uint8_t)(((cap) >> 17) & 0x3))
#define NVME_CAP_TO(cap)        ((uint8_t)(((cap) >> 24) & 0xFF))
#define NVME_CAP_DSTRD(cap)     ((uint8_t)(((cap) >> 32) & 0xF))
#define NVME_CAP_CSS(cap)       ((uint8_t)(((cap) >> 37) & 0xFF))
#define NVME_CAP_MPSMIN(cap)    ((uint8_t)(((cap) >> 48) & 0xF))
#define NVME_CAP_MPSMAX(cap)    ((uint8_t)(((cap) >> 52) & 0xF))

/* CC Register Bitfields */
#define NVME_CC_EN              (1U << 0)
#define NVME_CC_CSS_NVM         (0U << 4)
#define NVME_CC_MPS_4K          (0U << 7)
#define NVME_CC_AMS_RR          (0U << 11)
#define NVME_CC_SHN_NONE        (0U << 14)
#define NVME_CC_SHN_NORMAL      (1U << 14)
#define NVME_CC_IOSQES(log2)    (((uint32_t)(log2) & 0xF) << 16)
#define NVME_CC_IOCQES(log2)    (((uint32_t)(log2) & 0xF) << 20)

/* CSTS Register Bitfields */
#define NVME_CSTS_RDY           (1U << 0)
#define NVME_CSTS_CFS           (1U << 1)
#define NVME_CSTS_SHST_MASK     (3U << 2)

/* Admin Command Opcodes */
#define NVME_ADMIN_OP_DEL_SQ    0x00
#define NVME_ADMIN_OP_CREATE_SQ 0x01
#define NVME_ADMIN_OP_DEL_CQ    0x04
#define NVME_ADMIN_OP_CREATE_CQ 0x05
#define NVME_ADMIN_OP_IDENTIFY  0x06
#define NVME_ADMIN_OP_ABORT     0x08
#define NVME_ADMIN_OP_SET_FEAT  0x09
#define NVME_ADMIN_OP_GET_FEAT  0x0A
#define NVME_ADMIN_OP_ASYNC_EVT 0x0C

/* NVM I/O Command Opcodes */
#define NVME_NVM_OP_FLUSH       0x00
#define NVME_NVM_OP_WRITE       0x01
#define NVME_NVM_OP_READ        0x02

/* Identify CNS Codes */
#define NVME_ID_CNS_NS          0x00
#define NVME_ID_CNS_CTRL        0x01
#define NVME_ID_CNS_ACTIVE_NS   0x02

/* Submission Queue Entry (SQE) - 64 bytes */
struct nvme_sqe {
    uint8_t  opcode;
    uint8_t  flags;
    uint16_t cid;
    uint32_t nsid;
    uint64_t rsvd2;
    uint64_t mptr;
    uint64_t prp1;
    uint64_t prp2;
    uint32_t cdw10;
    uint32_t cdw11;
    uint32_t cdw12;
    uint32_t cdw13;
    uint32_t cdw14;
    uint32_t cdw15;
} __attribute__((packed));

/* Completion Queue Entry (CQE) - 16 bytes */
struct nvme_cqe {
    uint32_t result;
    uint32_t rsvd;
    uint16_t sq_head;
    uint16_t sq_id;
    uint16_t cid;
    uint16_t status;
} __attribute__((packed));

/* LBA Format Data Structure (4 bytes) */
struct nvme_lbaf {
    uint16_t ms;        /* Metadata Size */
    uint8_t  lbads;     /* LBA Data Size (2^lbads) */
    uint8_t  rp;        /* Relative Performance */
} __attribute__((packed));

/* Identify Controller Structure (4096 bytes) */
struct nvme_id_ctrl {
    uint16_t vid;
    uint16_t ssvid;
    char     sn[20];
    char     mn[40];
    char     fr[8];
    uint8_t  rab;
    uint8_t  ieee[3];
    uint8_t  cmic;
    uint8_t  mdts;
    uint16_t cntlid;
    uint32_t ver;
    uint32_t rtd3r;
    uint32_t rtd3e;
    uint32_t oaes;
    uint8_t  rsvd96[160];
    uint16_t oacs;
    uint8_t  acl;
    uint8_t  aerl;
    uint8_t  frmw;
    uint8_t  lpa;
    uint8_t  elpe;
    uint8_t  npss;
    uint8_t  avscc;
    uint8_t  apsta;
    uint16_t wctemp;
    uint16_t cctemp;
    uint8_t  rsvd270[242];
    uint8_t  sqes;
    uint8_t  cqes;
    uint16_t maxcmd;
    uint32_t nn;        /* Number of Namespaces */
    uint16_t oncs;
    uint16_t fuses;
    uint8_t  fna;
    uint8_t  vwc;
    uint16_t awun;
    uint16_t awupf;
    uint8_t  nvscc;
    uint8_t  rsvd531;
    uint16_t acwu;
    uint8_t  rsvd534[2];
    uint32_t sgls;
    uint8_t  rsvd540[1508];
    uint8_t  psd[1024];
    uint8_t  vs[1024];
} __attribute__((packed));

/* Identify Namespace Structure (4096 bytes) */
struct nvme_id_ns {
    uint64_t nsze;      /* Namespace Size (total blocks) */
    uint64_t ncap;      /* Namespace Capacity */
    uint64_t nuse;      /* Namespace Utilization */
    uint8_t  nsfeat;
    uint8_t  nlbaf;     /* Number of LBA Formats */
    uint8_t  flbas;     /* Formatted LBA Size */
    uint8_t  mc;
    uint8_t  dpc;
    uint8_t  dps;
    uint8_t  nmic;
    uint8_t  rescap;
    uint8_t  fpi;
    uint8_t  dlfeat;
    uint8_t  rsvd34[94];
    struct nvme_lbaf lbaf[16];
    uint8_t  rsvd192[192];
    uint8_t  vs[3712];
} __attribute__((packed));

#endif /* _NVME_SPEC_H_ */
