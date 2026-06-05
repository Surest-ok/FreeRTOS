/*
 * GICv2 Driver Implementation for QEMU virt AArch64
 *
 * GICv2 uses a simpler memory-mapped interface:
 *   - GICD (Distributor): manages interrupt routing, priority, enable
 *   - GICC (CPU Interface): per-core acknowledge, end-of-interrupt, priority mask
 *
 * No ICC_* system registers needed — everything is MMIO.
 */

#include <stdint.h>
#include "platform.h"
#include "gic.h"
#include "uart.h"

/* ---------------------------------------------------------------
 * Base addresses
 * --------------------------------------------------------------- */
#define GICD_BASE_ADDR   ((uint64_t)GICD_BASE)   /* 0x08000000 */
#define GICC_BASE_ADDR   ((uint64_t)0x08010000)  /* GICv2 CPU Interface */

/* ---------------------------------------------------------------
 * GIC Distributor register offsets
 * --------------------------------------------------------------- */
#define GICD_CTLR        0x0000
#define GICD_TYPER       0x0004
#define GICD_ISENABLER(n) (0x0100 + ((n) / 32) * 4)
#define GICD_ICENABLER(n) (0x0180 + ((n) / 32) * 4)
#define GICD_ISPENDR(n)  (0x0200 + ((n) / 32) * 4)
#define GICD_ICPENDR(n)  (0x0280 + ((n) / 32) * 4)
#define GICD_IPRIORITYR(n) (0x0400 + ((n) / 4) * 4)
#define GICD_ITARGETSR(n) (0x0800 + ((n) / 4) * 4)
#define GICD_ICFGR(n)    (0x0C00 + ((n) / 16) * 4)
#define GICD_SGIR        0x0F00

/* GICD_CTLR bits */
#define GICD_CTLR_ENABLE  0x1
#define GICD_CTLR_RWP     (1 << 31)

/* ---------------------------------------------------------------
 * GIC CPU Interface register offsets
 * --------------------------------------------------------------- */
#define GICC_CTLR        0x0000
#define GICC_PMR         0x0004
#define GICC_BPR         0x0008
#define GICC_IAR         0x000C
#define GICC_EOIR        0x0010

/* GICC_CTLR bits */
#define GICC_CTLR_ENABLE  0x1

/* ---------------------------------------------------------------
 * MMIO access helpers
 * --------------------------------------------------------------- */
#define GICD_REG(offset)     (*(volatile uint32_t *)(GICD_BASE_ADDR + (offset)))
#define GICC_REG(offset)     (*(volatile uint32_t *)(GICC_BASE_ADDR + (offset)))

/* ---------------------------------------------------------------
 * Initialize GICv2 Distributor
 * --------------------------------------------------------------- */
static void gicd_init(void)
{
    uint32_t typer;
    uint32_t max_irq;

    typer = GICD_REG(GICD_TYPER);
    max_irq = ((typer & 0x1F) + 1) * 32;
    if (max_irq > 1020)
        max_irq = 1020;

    /* Disable Distributor */
    GICD_REG(GICD_CTLR) = 0;

    /* Set all SPIs to target core 0
     * GICD_ITARGETSR: 4 interrupts per register, 8-bit target each
     */
    for (int i = SPI_START; i < (int)max_irq; i += 4) {
        GICD_REG(GICD_ITARGETSR(i)) = 0x01010101;  /* All target core 0 */
    }

    /* Set all SPI priorities to 0xA0 (medium) */
    for (int i = SPI_START; i < (int)max_irq; i += 4) {
        GICD_REG(GICD_IPRIORITYR(i)) = 0xA0A0A0A0;
    }

    /* Set all PPI priorities too */
    for (int i = 0; i < SPI_START; i += 4) {
        GICD_REG(GICD_IPRIORITYR(i)) = 0xA0A0A0A0;
    }

    /* Disable all SPIs */
    for (int i = SPI_START; i < (int)max_irq; i += 32) {
        GICD_REG(GICD_ICENABLER(i)) = 0xFFFFFFFF;
    }

    /* Clear all SPI pending */
    for (int i = SPI_START; i < (int)max_irq; i += 32) {
        GICD_REG(GICD_ICPENDR(i)) = 0xFFFFFFFF;
    }

    /* Enable Distributor */
    GICD_REG(GICD_CTLR) = GICD_CTLR_ENABLE;
}

/* ---------------------------------------------------------------
 * Initialize GICv2 CPU Interface
 * --------------------------------------------------------------- */
static void gicc_init(void)
{
    /* Disable CPU Interface */
    GICC_REG(GICC_CTLR) = 0;

    /* Set priority mask (allow all priorities) */
    GICC_REG(GICC_PMR) = 0xFF;

    /* Set binary point (no preemption) */
    GICC_REG(GICC_BPR) = 0;

    /* Enable CPU Interface */
    GICC_REG(GICC_CTLR) = GICC_CTLR_ENABLE;
}

/* ---------------------------------------------------------------
 * Public API
 * --------------------------------------------------------------- */

void gic_init(void)
{
    gicd_init();
    gicc_init();
}

void gic_enable_irq(uint32_t irq)
{
    GICD_REG(GICD_ISENABLER(irq)) = (1 << (irq % 32));
}

void gic_disable_irq(uint32_t irq)
{
    GICD_REG(GICD_ICENABLER(irq)) = (1 << (irq % 32));
}

void gic_set_priority(uint32_t irq, uint8_t priority)
{
    volatile uint32_t *reg = &GICD_REG(GICD_IPRIORITYR(irq));
    uint32_t shift = (irq % 4) * 8;
    uint32_t mask = 0xFF << shift;
    *reg = (*reg & ~mask) | ((uint32_t)priority << shift);
}

uint32_t gic_acknowledge_irq(void)
{
    return GICC_REG(GICC_IAR);
}

void gic_end_of_interrupt(uint32_t irq)
{
    GICC_REG(GICC_EOIR) = irq;
}

void gic_send_sgi(uint32_t sgi_id, uint8_t target_list)
{
    /* GICD_SGIR: [23:16]=target list, [3:0]=SGI ID */
    GICD_REG(GICD_SGIR) = ((uint32_t)target_list << 16) | (sgi_id & 0xF);
}
