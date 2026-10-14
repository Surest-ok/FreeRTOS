/*
 * CherryPCI - Standard PCI Configuration Space Registers & Bitmasks
 *
 * Designed for CherryOS / FreeRTOS.
 * Follows Linux kernel PCI register definitions.
 */

#ifndef CHERRY_PCI_REGS_H
#define CHERRY_PCI_REGS_H

#include <stdint.h>

/* Standard PCI Configuration Space Header (Type 0) */
#define PCI_VENDOR_ID           0x00    /* 16 bits */
#define PCI_DEVICE_ID           0x02    /* 16 bits */
#define PCI_COMMAND             0x04    /* 16 bits */
#define PCI_STATUS              0x06    /* 16 bits */
#define PCI_CLASS_REVISION      0x08    /* High 24 bits are class, low 8 revision */
#define PCI_REVISION_ID         0x08    /* 8 bits  */
#define PCI_CLASS_PROG          0x09    /* Reg. Level Program Interface */
#define PCI_CLASS_DEVICE        0x0A    /* Device class: 16 bits */
#define PCI_CACHE_LINE_SIZE     0x0C    /* 8 bits */
#define PCI_LATENCY_TIMER       0x0D    /* 8 bits */
#define PCI_HEADER_TYPE         0x0E    /* 8 bits */
#define PCI_BIST                0x0F    /* 8 bits */

/* Header types */
#define PCI_HEADER_TYPE_NORMAL  0
#define PCI_HEADER_TYPE_BRIDGE  1
#define PCI_HEADER_TYPE_CARDBUS 2
#define PCI_HEADER_TYPE_MFD     0x80    /* Multi-Function Device */

/* Command Register bits */
#define PCI_COMMAND_IO          (1U << 0)   /* Enable response in I/O space */
#define PCI_COMMAND_MEMORY      (1U << 1)   /* Enable response in Memory space */
#define PCI_COMMAND_MASTER      (1U << 2)   /* Enable bus mastering */
#define PCI_COMMAND_SPECIAL     (1U << 3)   /* Enable response to special cycles */
#define PCI_COMMAND_INVALIDATE  (1U << 4)   /* Use memory write and invalidate */
#define PCI_COMMAND_VGA_PALETTE (1U << 5)   /* Enable palette snooping */
#define PCI_COMMAND_PARITY      (1U << 6)   /* Enable parity checking */
#define PCI_COMMAND_WAIT        (1U << 7)   /* Enable address/data stepping */
#define PCI_COMMAND_SERR        (1U << 8)   /* Enable SERR */
#define PCI_COMMAND_FAST_BACK   (1U << 9)   /* Enable back-to-back writes */
#define PCI_COMMAND_INTX_DISABLE (1U << 10) /* INTx Emulation Disable */

/* Status Register bits */
#define PCI_STATUS_INTERRUPT    (1U << 3)   /* Interrupt status */
#define PCI_STATUS_CAP_LIST     (1U << 4)   /* Support Capability List */
#define PCI_STATUS_66MHZ        (1U << 5)   /* Support 66 Mhz PCI 2.1 bus */
#define PCI_STATUS_FAST_BACK    (1U << 7)   /* Accept fast-back-to-back */
#define PCI_STATUS_PARITY       (1U << 8)   /* Master data parity error */
#define PCI_STATUS_DEVSEL_MASK  0x0600      /* DEVSEL timing */
#define PCI_STATUS_SIG_TARGET_ABORT (1U << 11) /* Set on target abort */
#define PCI_STATUS_REC_TARGET_ABORT (1U << 12) /* Master ack of " */
#define PCI_STATUS_REC_MASTER_ABORT (1U << 13) /* Set on master abort */
#define PCI_STATUS_SIG_SYSTEM_ERROR (1U << 14) /* Set when SERR asserts */
#define PCI_STATUS_DETECTED_PARITY  (1U << 15) /* Set on parity error */

/* Base Address Registers (BAR0 .. BAR5) */
#define PCI_BASE_ADDRESS_0      0x10    /* 32 bits */
#define PCI_BASE_ADDRESS_1      0x14    /* 32 bits */
#define PCI_BASE_ADDRESS_2      0x18    /* 32 bits */
#define PCI_BASE_ADDRESS_3      0x1C    /* 32 bits */
#define PCI_BASE_ADDRESS_4      0x20    /* 32 bits */
#define PCI_BASE_ADDRESS_5      0x24    /* 32 bits */
#define PCI_BAR_COUNT           6

#define PCI_BASE_ADDRESS_SPACE          0x01
#define PCI_BASE_ADDRESS_SPACE_IO       0x01
#define PCI_BASE_ADDRESS_SPACE_MEMORY   0x00
#define PCI_BASE_ADDRESS_MEM_TYPE_MASK  0x06
#define PCI_BASE_ADDRESS_MEM_TYPE_32    0x00    /* 32 bit address */
#define PCI_BASE_ADDRESS_MEM_TYPE_1M    0x02    /* Below 1M [obsolete] */
#define PCI_BASE_ADDRESS_MEM_TYPE_64    0x04    /* 64 bit address */
#define PCI_BASE_ADDRESS_MEM_PREFETCH   0x08    /* prefetchable? */
#define PCI_BASE_ADDRESS_MEM_MASK       (~0x0FULL)
#define PCI_BASE_ADDRESS_IO_MASK        (~0x03ULL)

/* Linux-style Resource Flags */
#ifndef IORESOURCE_IO
#define IORESOURCE_IO           0x00000100  /* Resource is I/O space */
#endif
#ifndef IORESOURCE_MEM
#define IORESOURCE_MEM          0x00000200  /* Resource is Memory space */
#endif
#ifndef IORESOURCE_PREFETCH
#define IORESOURCE_PREFETCH     0x00002000  /* Resource is prefetchable */
#endif
#ifndef IORESOURCE_MEM_64
#define IORESOURCE_MEM_64       0x00100000  /* 64-bit Memory space */
#endif

/* Subsystem registers */
#define PCI_SUBSYSTEM_VENDOR_ID 0x2C
#define PCI_SUBSYSTEM_ID        0x2E

/* Capability & Interrupt Registers */
#define PCI_CAPABILITY_LIST     0x34    /* Offset of first capability list entry */
#define PCI_INTERRUPT_LINE      0x3C    /* 8 bits */
#define PCI_INTERRUPT_PIN       0x3D    /* 8 bits (1=INTA, 2=INTB, 3=INTC, 4=INTD) */
#define PCI_MIN_GNT             0x3E    /* 8 bits */
#define PCI_MAX_LAT             0x3F    /* 8 bits */

/* Capability IDs (Standard PCI 0x34 list) */
#define PCI_CAP_ID_PM           0x01    /* Power Management */
#define PCI_CAP_ID_AGP          0x02    /* Accelerated Graphics Port */
#define PCI_CAP_ID_VPD          0x03    /* Vital Product Data */
#define PCI_CAP_ID_SLOTID       0x04    /* Slot Identification */
#define PCI_CAP_ID_MSI          0x05    /* Message Signalled Interrupts */
#define PCI_CAP_ID_CHSWP        0x06    /* CompactPCI HotSwap */
#define PCI_CAP_ID_PCIX         0x07    /* PCI-X */
#define PCI_CAP_ID_HT           0x08    /* HyperTransport */
#define PCI_CAP_ID_VNDR         0x09    /* Vendor-Specific */
#define PCI_CAP_ID_SHPC         0x0C    /* PCI Standard Hot-Plug Controller */
#define PCI_CAP_ID_SSVID        0x0D    /* Bridge subsystem vendor/device ID */
#define PCI_CAP_ID_AGP3         0x0E    /* AGP Target PCI-PCI bridge */
#define PCI_CAP_ID_EXP          0x10    /* PCI Express */
#define PCI_CAP_ID_MSIX         0x11    /* MSI-X */

/* PCIe Extended Capability IDs (offset 0x100..0xFFF) */
#define PCI_EXT_CAP_ID_ERR      0x0001  /* Advanced Error Reporting (AER) */
#define PCI_EXT_CAP_ID_VC       0x0002  /* Virtual Channel */
#define PCI_EXT_CAP_ID_DSN      0x0003  /* Device Serial Number */
#define PCI_EXT_CAP_ID_PWR      0x0004  /* Power Budgeting */
#define PCI_EXT_CAP_ID_RCLD     0x0005  /* Root Complex Link Declaration */
#define PCI_EXT_CAP_ID_RCILC    0x0006  /* Root Complex Internal Link Control */
#define PCI_EXT_CAP_ID_RCEC     0x0007  /* Root Complex Event Collector */
#define PCI_EXT_CAP_ID_MFVC     0x0008  /* Multi-Function VC */
#define PCI_EXT_CAP_ID_VC9      0x0009  /* VC9 */
#define PCI_EXT_CAP_ID_RCRB     0x000A  /* RCRB Header */
#define PCI_EXT_CAP_ID_VNDR     0x000B  /* Vendor-Specific Extended Cap */
#define PCI_EXT_CAP_ID_ACS      0x000D  /* Access Control Services */
#define PCI_EXT_CAP_ID_ARI      0x000E  /* Alternative Routing-ID Interpretation */
#define PCI_EXT_CAP_ID_ATS      0x000F  /* Address Translation Services */
#define PCI_EXT_CAP_ID_SRIOV    0x0010  /* Single Root I/O Virtualization */
#define PCI_EXT_CAP_ID_PRI      0x0013  /* Page Request Interface */
#define PCI_EXT_CAP_ID_PASID    0x001B  /* Process Address Space ID */
#define PCI_EXT_CAP_ID_DPC      0x001D  /* Downstream Port Containment */
#define PCI_EXT_CAP_ID_L1SS     0x001E  /* L1 PM Substates */
#define PCI_EXT_CAP_ID_PTM      0x001F  /* Precision Time Measurement */

/* Message Signalled Interrupts (MSI) registers */
#define PCI_MSI_FLAGS           0x02    /* Various flags */
#define PCI_MSI_FLAGS_ENABLE    (1U << 0)   /* MSI enable */
#define PCI_MSI_FLAGS_QMASK     0x000E  /* Maximum queue size available */
#define PCI_MSI_FLAGS_QSIZE     0x0070  /* Controls size of allocated queue */
#define PCI_MSI_FLAGS_64BIT     (1U << 7)   /* 64-bit addresses supported */
#define PCI_MSI_FLAGS_MASKBIT   (1U << 8)   /* Per-vector masking supported */
#define PCI_MSI_ADDRESS_LO      0x04    /* Lower 32 bits for 32/64 bit devices */
#define PCI_MSI_ADDRESS_HI      0x08    /* Upper 32 bits for 64 bit devices */
#define PCI_MSI_DATA_32         0x08    /* 16 bits for 32-bit devices */
#define PCI_MSI_DATA_64         0x0C    /* 16 bits for 64-bit devices */

/* MSI-X registers */
#define PCI_MSIX_FLAGS          0x02
#define PCI_MSIX_FLAGS_QSIZE    0x07FF  /* Table size */
#define PCI_MSIX_FLAGS_MASKALL  (1U << 14)  /* Mask all vectors */
#define PCI_MSIX_FLAGS_ENABLE   (1U << 15)  /* MSI-X enable */
#define PCI_MSIX_TABLE          0x04    /* Table offset and BIR */
#define PCI_MSIX_PBA            0x08    /* PBA offset and BIR */
#define PCI_MSIX_FLAGS_BIRMASK  0x0007  /* BAR Indicator Register */

/* Interrupt allocation flags */
#define PCI_IRQ_LEGACY          (1U << 0)   /* Allow or prefer legacy INTx */
#define PCI_IRQ_MSI             (1U << 1)   /* Allow or prefer MSI */
#define PCI_IRQ_MSIX            (1U << 2)   /* Allow or prefer MSI-X */
#define PCI_IRQ_ALL_TYPES       (PCI_IRQ_LEGACY | PCI_IRQ_MSI | PCI_IRQ_MSIX)

#endif /* CHERRY_PCI_REGS_H */
