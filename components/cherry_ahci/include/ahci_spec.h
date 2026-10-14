/*
 * CherryAHCI - Serial ATA Advanced Host Controller Interface (AHCI 1.3+) Definitions
 *
 * Copyright (c) 2026 ForgeStitch
 * SPDX-License-Identifier: MIT
 */

#ifndef _AHCI_SPEC_H_
#define _AHCI_SPEC_H_

#include <stdint.h>

/* Generic Host Control (GHC) Registers */
#define AHCI_GHC_CAP        0x0000  /* Host Capabilities */
#define AHCI_GHC_GHC        0x0004  /* Global Host Control */
#define AHCI_GHC_IS         0x0008  /* Interrupt Status */
#define AHCI_GHC_PI         0x000C  /* Ports Implemented */
#define AHCI_GHC_VS         0x0010  /* Version */
#define AHCI_GHC_CCC_CTL    0x0014  /* Command Completion Coalescing Control */
#define AHCI_GHC_CCC_PORTS  0x0018  /* Command Completion Coalescing Ports */
#define AHCI_GHC_EM_LOC     0x001C  /* Enclosure Management Location */
#define AHCI_GHC_EM_CTL     0x0020  /* Enclosure Management Control */
#define AHCI_GHC_CAP2       0x0024  /* Host Capabilities Extended */
#define AHCI_GHC_BOHC       0x0028  /* BIOS/OS Handoff Control and Status */

/* GHC Bits */
#define AHCI_GHC_HR         (1U << 0)   /* HBA Reset */
#define AHCI_GHC_IE         (1U << 1)   /* Interrupt Enable */
#define AHCI_GHC_MRSM       (1U << 2)   /* MSI Revert to Single Message */
#define AHCI_GHC_AE         (1U << 31)  /* AHCI Enable */

/* Port Register Offsets (Port p Base = 0x100 + p * 0x80) */
#define AHCI_PORT_OFFSET(p) (0x0100 + ((p) * 0x0080))

#define AHCI_PORT_CLB       0x00    /* Command List Base Address (32-bit) */
#define AHCI_PORT_CLBU      0x04    /* Command List Base Address Upper 32-bits */
#define AHCI_PORT_FB        0x08    /* FIS Base Address (32-bit) */
#define AHCI_PORT_FBU       0x0C    /* FIS Base Address Upper 32-bits */
#define AHCI_PORT_IS        0x10    /* Interrupt Status */
#define AHCI_PORT_IE        0x14    /* Interrupt Enable */
#define AHCI_PORT_CMD       0x18    /* Command and Status */
#define AHCI_PORT_TFD       0x20    /* Task File Data */
#define AHCI_PORT_SIG       0x24    /* Signature */
#define AHCI_PORT_SSTS      0x28    /* SATA Status (SCR0: SStatus) */
#define AHCI_PORT_SCTL      0x2C    /* SATA Control (SCR2: SControl) */
#define AHCI_PORT_SERR      0x30    /* SATA Error (SCR1: SError) */
#define AHCI_PORT_SACT      0x34    /* SATA Active (SCR3: SActive) */
#define AHCI_PORT_CI        0x38    /* Command Issue */
#define AHCI_PORT_SNTF      0x3C    /* SATA Notification (SCR4: SNotification) */
#define AHCI_PORT_FBS       0x40    /* FIS-based Switching Control */

/* Port CMD Register Bits */
#define AHCI_PORT_CMD_ST    (1U << 0)   /* Start */
#define AHCI_PORT_CMD_SUD   (1U << 1)   /* Spin-Up Device */
#define AHCI_PORT_CMD_POD   (1U << 2)   /* Power On Device */
#define AHCI_PORT_CMD_CLO   (1U << 3)   /* Command List Override */
#define AHCI_PORT_CMD_FRE   (1U << 4)   /* FIS Receive Enable */
#define AHCI_PORT_CMD_FR    (1U << 14)  /* FIS Receive Running */
#define AHCI_PORT_CMD_CR    (1U << 15)  /* Command List Running */

/* Port TFD Register Bits */
#define AHCI_PORT_TFD_ERR   (1U << 0)   /* Error */
#define AHCI_PORT_TFD_DRQ   (1U << 3)   /* Data Request */
#define AHCI_PORT_TFD_BSY   (1U << 7)   /* Busy */

/* Port Signature Definitions */
#define AHCI_SIG_ATA        0x00000101  /* SATA Hard Disk Drive */
#define AHCI_SIG_ATAPI      0xEB140101  /* SATAPI Optical Drive */
#define AHCI_SIG_SEMB       0xC33C0101  /* Enclosure Management Bridge */
#define AHCI_SIG_PM         0x96690101  /* Port Multiplier */

/* Port SSTS Register Bits */
#define AHCI_PORT_SSTS_DET_MASK     0x0F
#define AHCI_PORT_SSTS_DET_PRESENT  0x03

/* FIS Types */
#define AHCI_FIS_REG_H2D    0x27    /* Register FIS - Host to Device */
#define AHCI_FIS_REG_D2H    0x34    /* Register FIS - Device to Host */
#define AHCI_FIS_DMA_ACT    0x39    /* DMA Activate FIS - Device to Host */
#define AHCI_FIS_DMA_SETUP  0x41    /* DMA Setup FIS - Bidirectional */
#define AHCI_FIS_DATA       0x46    /* Data FIS - Bidirectional */
#define AHCI_FIS_BIST       0x58    /* BIST Activate FIS - Bidirectional */
#define AHCI_FIS_PIO_SETUP  0x5F    /* PIO Setup FIS - Device to Host */
#define AHCI_FIS_DEV_BITS   0xA1    /* Set Device Bits FIS - Device to Host */

/* ATA Command Opcodes */
#define ATA_CMD_IDENTIFY            0xEC
#define ATA_CMD_READ_DMA_EXT        0x25
#define ATA_CMD_WRITE_DMA_EXT       0x35
#define ATA_CMD_FLUSH_CACHE_EXT     0xEA

/* Register - Host to Device FIS (20 bytes) */
struct ahci_fis_reg_h2d {
    uint8_t  fis_type;      /* 0x27 */
    uint8_t  pmport_c;      /* pmport:4, rsvd:3, c:1 */
    uint8_t  command;       /* ATA Command register */
    uint8_t  features_lo;   /* Features (7:0) */
    uint8_t  lba_lo;        /* LBA (7:0) */
    uint8_t  lba_mid;       /* LBA (15:8) */
    uint8_t  lba_hi;        /* LBA (23:16) */
    uint8_t  device;        /* Device register (1<<6 for LBA) */
    uint8_t  lba_exp_lo;    /* LBA (31:24) */
    uint8_t  lba_exp_mid;   /* LBA (39:32) */
    uint8_t  lba_exp_hi;    /* LBA (47:40) */
    uint8_t  features_hi;   /* Features (15:8) */
    uint8_t  count_lo;      /* Sector count (7:0) */
    uint8_t  count_hi;      /* Sector count (15:8) */
    uint8_t  icc;           /* Isochronous command completion */
    uint8_t  control;       /* Control register */
    uint8_t  rsvd[4];
} __attribute__((packed));

/* Command Header Structure (32 bytes) */
struct ahci_cmd_header {
    uint16_t flags;         /* CFL:5, A:1, W:1, P:1, R:1, B:1, C:1, rsvd:1, PMP:4 */
    uint16_t prdtl;         /* Physical Region Descriptor Table Length */
    uint32_t prdbc;         /* Physical Region Descriptor Byte Count transferred */
    uint32_t ctba;          /* Command Table Base Address (32-bit, 128B aligned) */
    uint32_t ctbau;         /* Command Table Base Address Upper 32-bits */
    uint32_t rsvd[4];
} __attribute__((packed));

/* Physical Region Descriptor Table (PRDT) Entry (16 bytes) */
struct ahci_prdt_entry {
    uint32_t dba;           /* Data Base Address (32-bit, 2-byte aligned) */
    uint32_t dbau;          /* Data Base Address Upper 32-bits */
    uint32_t rsvd;
    uint32_t dbc_ioc;       /* DBC:22 (Byte count - 1), rsvd:9, IOC:1 (Interrupt On Completion) */
} __attribute__((packed));

/* Command Table Structure (CFIS + ATAPI + Reserved + PRDTs) */
struct ahci_cmd_table {
    uint8_t                cfis[64];    /* Command FIS */
    uint8_t                acmd[16];    /* ATAPI Command */
    uint8_t                rsvd[48];
    struct ahci_prdt_entry prdt[8];     /* Up to 8 PRDT entries (covers up to 32MB) */
} __attribute__((packed, aligned(128)));

/* Received FIS Structure (256 bytes) */
struct ahci_rcv_fis {
    uint8_t dsfis[28];      /* DMA Setup FIS */
    uint8_t rsvd1[4];
    uint8_t psfis[20];      /* PIO Setup FIS */
    uint8_t rsvd2[12];
    uint8_t rfis[20];       /* D2H Register FIS */
    uint8_t rsvd3[4];
    uint8_t sdbfis[8];      /* Set Device Bits FIS */
    uint8_t ufis[64];       /* Unknown FIS */
    uint8_t rsvd4[96];
} __attribute__((packed, aligned(256)));

#endif /* _AHCI_SPEC_H_ */
