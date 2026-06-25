/*
 * GICv3 Driver Implementation for QEMU virt AArch64
 *
 * Prerequisites:
 *   - ICC_SRE_EL2.SRE set at EL2 before dropping to EL1 (startup.S)
 *   - HCR_EL2.RW=1, no IRQ/FIQ routing (startup.S)
 *
 * GICv3 components:
 *   - GICD (Distributor): 0x08000000 — SPI routing, priority
 *   - GICR (Redistributor): 0x080A0000 — per-core SGI/PPI
 *   - ICC_* (CPU Interface): system registers — per-core interrupt handling
 */

#include <stdint.h>
#include "platform.h"
#include "gic.h"
#include "uart.h"

/* SMP: get current core ID from MPIDR_EL1 */
static inline uint32_t gic_current_core(void)
{
    uint64_t mpidr;
    __asm__ volatile("mrs %0, mpidr_el1" : "=r"(mpidr));
    return (uint32_t)((mpidr & 0xFFU) + (((mpidr >> 8) & 0xFFU) << 4));
}

/* ---------------------------------------------------------------
 * MMIO access helpers
 * --------------------------------------------------------------- */
#define GICD_REG(offset)       (*(volatile uint32_t *)((uint64_t)GICD_BASE + (offset)))
#define GICD_REG64(offset)     (*(volatile uint64_t *)((uint64_t)GICD_BASE + (offset)))
#define GICR_REG(core, offset) (*(volatile uint32_t *)((uint64_t)GICR_BASE + (uint64_t)(core) * 0x20000 + (offset)))

/* ---------------------------------------------------------------
 * GIC Distributor register offsets
 * --------------------------------------------------------------- */
#define GICD_CTLR          0x0000
#define GICD_TYPER         0x0004
#define GICD_ISENABLER(n)  (0x0100 + ((n) / 32) * 4)
#define GICD_ICENABLER(n)  (0x0180 + ((n) / 32) * 4)
#define GICD_ICPENDR(n)    (0x0280 + ((n) / 32) * 4)
#define GICD_IGROUPR(n)    (0x0080 + ((n) / 32) * 4)
#define GICD_IPRIORITYR(n) (0x0400 + ((n) / 4) * 4)
#define GICD_ICFGR(n)      (0x0C00 + ((n) / 16) * 4)
#define GICD_IROUTER(n)    (0x6000 + (n) * 8)

/* GICD_CTLR bits */
#define GICD_CTLR_ENABLEGRP1NS  (1 << 1)
#define GICD_CTLR_ENABLEGRP1    (1 << 0)
#define GICD_CTLR_ARE_NS        (1 << 4)

/* ---------------------------------------------------------------
 * GIC Redistributor register offsets
 * RD_base (0x00000): CTLR, TYPER, WAKER
 * SGI_base (0x10000): ISENABLER, IPRIORITYR, IGROUPR for SGI/PPI
 * --------------------------------------------------------------- */
#define GICR_WAKER             0x0014
#define GICR_WAKER_PS          (1 << 1)   /* ProcessorSleep */
#define GICR_WAKER_CA          (1 << 2)   /* ChildrenAsleep */

#define GICR_SGI_BASE          0x10000
#define GICR_IGROUPR0          (GICR_SGI_BASE + 0x0080)
#define GICR_ISENABLER0        (GICR_SGI_BASE + 0x0100)
#define GICR_ICENABLER0        (GICR_SGI_BASE + 0x0180)
#define GICR_ICPENDR0          (GICR_SGI_BASE + 0x0280)
#define GICR_IPRIORITYR_BASE   (GICR_SGI_BASE + 0x0400)

/* ---------------------------------------------------------------
 * GICv3 CPU Interface system register encodings
 * Format: S3_<op1>_C<CRn>_C<CRm>_<op2>
 * --------------------------------------------------------------- */
#define ICC_IAR1_EL1     "S3_0_C12_C12_0"
#define ICC_EOIR1_EL1    "S3_0_C12_C12_1"
#define ICC_BPR1_EL1     "S3_0_C12_C12_3"
#define ICC_PMR_EL1      "S3_0_C4_C6_0"
#define ICC_SRE_EL1      "S3_0_C12_C12_5"
#define ICC_IGRPEN1_EL1  "S3_0_C12_C12_7"
#define ICC_SGI1R_EL1    "S3_0_C12_C11_5"

/* ---------------------------------------------------------------
 * System register access
 * --------------------------------------------------------------- */
#define read_sysreg(reg) ({ \
    uint64_t _val; \
    __asm__ volatile("mrs %0, " reg : "=r"(_val)); \
    _val; \
})

#define write_sysreg(reg, val) do { \
    __asm__ volatile("msr " reg ", %0" :: "r"((uint64_t)(val))); \
    __asm__ volatile("isb"); \
} while(0)

/* ================================================================
 * Distributor init
 * ================================================================ */
static void gicd_init(void)
{
    uint32_t typer = GICD_REG(GICD_TYPER);
    uint32_t max_irq = ((typer & 0x1F) + 1) * 32;
    if (max_irq > 1020) max_irq = 1020;

    /* Disable Distributor */
    GICD_REG(GICD_CTLR) = 0;

    /* Set all SPIs to Group 1 NS */
    for (int i = SPI_START; i < (int)max_irq; i += 32)
        GICD_REG(GICD_IGROUPR(i)) = 0xFFFFFFFF;

    /* Set default priority for all */
    for (int i = 0; i < (int)max_irq; i += 4)
        GICD_REG(GICD_IPRIORITYR(i)) = 0xA0A0A0A0;

    /* Route all SPIs to core 0 */
    for (int i = SPI_START; i < (int)max_irq; i++)
        GICD_REG64(GICD_IROUTER(i)) = 0;

    /* Enable Distributor: ARE_NS + EnableGrp1NS + EnableGrp1 */
    GICD_REG(GICD_CTLR) = GICD_CTLR_ARE_NS | GICD_CTLR_ENABLEGRP1NS | GICD_CTLR_ENABLEGRP1;
}

/* ================================================================
 * Redistributor init (per-core)
 * ================================================================ */
static void gicr_init(uint32_t core_id)
{
    /* Wake up Redistributor */
    GICR_REG(core_id, GICR_WAKER) &= ~GICR_WAKER_PS;

    /* Wait for ChildrenAsleep to clear */
    volatile int timeout = 1000000;
    while ((GICR_REG(core_id, GICR_WAKER) & GICR_WAKER_CA) && --timeout)
        ;

    /* Disable all SGI/PPI */
    GICR_REG(core_id, GICR_ICENABLER0) = 0xFFFFFFFF;
    GICR_REG(core_id, GICR_ICPENDR0) = 0xFFFFFFFF;

    /* Set all SGI/PPI to Group 1 NS (IGROUPR0 bit=1 → Group 1) */
    GICR_REG(core_id, GICR_IGROUPR0) = 0xFFFFFFFF;

    /* Set default priority for SGI/PPI */
    for (int i = 0; i < 32; i++)
        *(volatile uint8_t *)((uint64_t)GICR_BASE + (uint64_t)core_id * 0x20000
                              + GICR_IPRIORITYR_BASE + i) = 0xA0;
}

/* ================================================================
 * CPU Interface init — always runs at EL1 after startup drop
 * ================================================================ */
static void gic_cpuif_init(void)
{
    uint64_t val;

    /* Enable system register access */
    val = read_sysreg(ICC_SRE_EL1);
    val |= 0x7;
    write_sysreg(ICC_SRE_EL1, val);

    /* Priority mask: allow all */
    write_sysreg(ICC_PMR_EL1, 0xFF);

    /* Binary point */
    write_sysreg(ICC_BPR1_EL1, 0);

    /* Enable Group 1 NS interrupts */
    val = read_sysreg(ICC_IGRPEN1_EL1);
    val |= 0x1;
    write_sysreg(ICC_IGRPEN1_EL1, val);
}

/* ================================================================
 * Public API
 * ================================================================ */

void gic_init(uint32_t core_id)
{
    if (core_id == 0)
        gicd_init();
    gicr_init(core_id);
    gic_cpuif_init();
}

void gic_enable_irq(uint32_t irq)
{
    if (irq < 32) {
        uint32_t core = gic_current_core();
        GICR_REG(core, GICR_ISENABLER0) = (1 << irq);
    } else {
        GICD_REG(GICD_ISENABLER(irq)) = (1 << (irq % 32));
    }
}

void gic_disable_irq(uint32_t irq)
{
    if (irq < 32) {
        uint32_t core = gic_current_core();
        GICR_REG(core, GICR_ICENABLER0) = (1 << irq);
    } else {
        GICD_REG(GICD_ICENABLER(irq)) = (1 << (irq % 32));
    }
}

void gic_set_priority(uint32_t irq, uint8_t priority)
{
    if (irq < 32) {
        uint32_t core = gic_current_core();
        *(volatile uint8_t *)((uint64_t)GICR_BASE + (uint64_t)core * 0x20000
                              + GICR_IPRIORITYR_BASE + irq) = priority;
    } else {
        volatile uint32_t *reg = &GICD_REG(GICD_IPRIORITYR(irq));
        uint32_t shift = (irq % 4) * 8;
        *reg = (*reg & ~(0xFF << shift)) | ((uint32_t)priority << shift);
    }
}

void gic_set_affinity(uint32_t irq, uint8_t affinity)
{
    if (irq >= 32)
        GICD_REG64(GICD_IROUTER(irq)) = (uint64_t)affinity;
}

uint32_t gic_acknowledge_irq(void)
{
    return (uint32_t)read_sysreg(ICC_IAR1_EL1);
}

void gic_end_of_interrupt(uint32_t irq)
{
    write_sysreg(ICC_EOIR1_EL1, irq);
}

void gic_send_sgi(uint32_t sgi_id, uint32_t target_core)
{
    uint64_t aff1 = ((uint64_t)target_core >> 4) & 0xFFULL;
    uint64_t target_list = 1ULL << ((uint64_t)target_core & 0xFULL);
    uint64_t val = (aff1 << 16) | ((uint64_t)(sgi_id & 0xFU) << 24) | target_list;
    write_sysreg(ICC_SGI1R_EL1, val);
}
