/*
 * Platform definitions for QEMU virt AArch64
 * Hardware addresses (physical, no MMU)
 */

#ifndef __PLATFORM_H__
#define __PLATFORM_H__

/* RAM base address (QEMU virt default) */
#define RAM_BASE            0x40000000

/* Memory size */
#define RAM_SIZE            0x10000000  // 256MB

/* PL011 UART0 (QEMU virt hardware address) */
#define UART0_BASE          0x09000000
#define UART0_DR_OFFSET     0x000       // Data Register
#define UART0_RSR_OFFSET     0x004       // Receiver Status Register
#define UART0_ECR_OFFSET     0x004       // Error Clear Register
#define UART0_FR_OFFSET     0x018       // Flag Register
#define UART0_IBRD_OFFSET    0x024       // Integer Baud Rate Divisor
#define UART0_FBRD_OFFSET    0x028       // Fractional Baud Rate Divisor
#define UART0_LCRH_OFFSET    0x02C       // Line Control Register
#define UART0_CR_OFFSET      0x030       // Control Register
#define UART0_IFLS_OFFSET    0x034       // Interrupt FIFO Level Select
#define UART0_IMSC_OFFSET    0x038       // Interrupt Mask Set/Clear
#define UART0_RIS_OFFSET     0x03C       // Raw Interrupt Status
#define UART0_MIS_OFFSET     0x040       // Masked Interrupt Status
#define UART0_ICR_OFFSET     0x044       // Interrupt Clear Register
#define UART0_DMACR_OFFSET   0x048       // DMA Control Register

/* Flag Register bits */
#define UART_FR_TXFF        (1 << 5)     // Transmit FIFO Full
#define UART_FR_RXFE        (1 << 4)     // Receive FIFO Empty
#define UART_FR_BUSY        (1 << 3)     // Transmitter busy

/* Interrupt Mask bits */
#define UART_IMSC_RXIM      (1 << 4)     // Receive Interrupt Mask
#define UART_IMSC_RTIM      (1 << 5)     // Receive Timeout Interrupt Mask

/* GICv3 Distributor base addresses */
#define GICD_BASE           0x08000000  // Distributor base
#define GICR_BASE           0x080A0000  // Redistributor base

/* GICD registers offsets */
#define GICD_CTLR_OFFSET    0x000       // Distributor Control Register
#define GICD_TYPER_OFFSET   0x0008      // Distributor Type Register
#define GICD_IGROUPR_OFFSET 0x0080      // Interrupt Group Registers
#define GICD_ENABLE_SET_OFFSET 0x0100    // Enable Set Registers
#define GICD_ENABLE_CLEAR_OFFSET 0x0180  // Enable Clear Registers
#define GICD_PENDING_SET_OFFSET 0x0200   // Pending Set Registers
#define GICD_ICFGR_OFFSET   0x0C00      // Interrupt Configuration Registers
#define GICD_IPRIORITYR_OFFSET 0x0400   // Interrupt Priority Registers
#define GICD_IROUTER_OFFSET  0x06000     // Interrupt Routing Registers

/* GICR registers offsets */
#define GICR_CTLR_OFFSET    0x000       // Redistributor Control Register
#define GICR_TYPER_OFFSET   0x0008      // Redistributor Type Register
#define GICR_WAKER_OFFSET   0x0014      // Wake Register
#define GICR_IPRIORITYR_OFFSET 0x0800   // Interrupt Priority Registers

/* GIC registers definitions */
#define GICD_CTLR_ENABLEGRP1NS    (1 << 1)  // Enable Group 1 Non-secure interrupts
#define GICR_WAKER_PROCESSORSLEEP (1 << 1)  // Processor sleep
#define GICR_WAKER_CHILDRENASLEEP (1 << 0)  // Children asleep

/* Generic Timer registers */
#define CNTP_TVAL_EL0       "cntp_tval_el0"  // Physical Timer Compare Value
#define CNTP_CTL_EL0        "cntp_ctl_el0"   // Physical Timer Control
#define CNTPCT_EL0         "cntpct_el0"    // Physical Counter Value
#define CNTFRQ_EL0         "cntfrq_el0"    // Counter Frequency

/* Core registers */
#define MPIDR_EL1          "mpidr_el1"    // Multiprocessor Affinity Register
#define SCTLR_EL1          "sctlr_el1"    // System Control Register
#define SCR_EL2            "scr_el2"      // Secure Control Register
#define HCR_EL2            "hcr_el2"      // Hypervisor Control Register
#define SPSR_EL1           "spsr_el1"     // Processor Status Register
#define SPSR_EL2           "spsr_el2"     // Processor Status Register

/* SCTLR_EL1 bits */
#define SCTLR_EL1_M         (1 << 0)      // MMU enable
#define SCTLR_EL1_A         (1 << 1)      // Alignment check enable
#define SCTLR_EL1_C         (1 << 2)      // Cache enable
#define SCTLR_EL1_CP15BEN   (1 << 5)      // CP15 Barrier operation enable
#define SCTLR_EL1_I         (1 << 12)     // Instruction cache enable
#define SCTLR_EL1_Z         (1 << 13)     // Branch prediction enable
#define SCTLR_EL1_V         (1 << 14)     // Vector catch enable
#define SCTLR_EL1_UCT       (1 << 15)     // Unaligned trap control
#define SCTLR_EL1_NMFI      (1 << 18)     // Non-maskable interrupt filter
#define SCTLR_EL1_TSCXT     (1 << 19)     // Translation of context ID
#define SCTLR_EL1_EIS       (1 << 21)     // Endianness instruction set
#define SCTLR_EL1_E0E       (1 << 25)     // Big endian (0) vs little endian (1)
#define SCTLR_EL1_RES1      (0x00C00040)  // Reserved bits that must be set

/* CPACR_EL1 bits for FP/SIMD enable */
#define CPACR_EL1_TTA       (1 << 28)     // Traps access for EL0 and EL1 to Advanced SIMD registers
#define CPACR_EL1_EN_CP10  (1 << 20)     // Enable CP10 (FP) for EL1
#define CPACR_EL1_EN_CP11  (1 << 21)     // Enable CP11 (SIMD) for EL1
#define CPACR_FP_SIMD_ENABLE (CPACR_EL1_EN_CP10 | CPACR_EL1_EN_CP11)

/* CurrentEL bits */
#define CURRENT_EL_EL1      (1 << 2)
#define CURRENT_EL_EL2      (2 << 2)
#define CURRENT_EL_EL3      (3 << 2)

/* Virtual Secure/Guest */
#define PSR_MODE_EL1t       0x00000000    // EL1, Thumb
#define PSR_MODE_EL1h       0x00000004    // EL1, ARM
#define PSR_MODE_EL2t       0x00000008    // EL2, Thumb
#define PSR_MODE_EL2h       0x0000000C    // EL2, ARM

#endif /* __PLATFORM_H__ */