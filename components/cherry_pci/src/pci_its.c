/*
 * CherryPCI - ARM GICv3 ITS (Interrupt Translation Service) Hardware Driver
 */

#include <stdint.h>
#include <stdbool.h>
#include "platform.h"
#include "gic.h"
#include "cherry_its.h"
#include "cherry_pci.h"
#include "cherry_printk.h"
#include "arch_api/arch.h"
#include "arch_api/irq.h"

/* GITS Register Access */
#define GITS_REG32(off)      (*(volatile uint32_t *)((uintptr_t)GITS_BASE + (off)))
#define GITS_REG64(off)      (*(volatile uint64_t *)((uintptr_t)GITS_BASE + (off)))

/* GICR MMIO Access */
#define GICR_REG32(core, off) (*(volatile uint32_t *)((uintptr_t)GICR_BASE + (uintptr_t)(core) * 0x20000 + (off)))
#define GICR_REG64(core, off) (*(volatile uint64_t *)((uintptr_t)GICR_BASE + (uintptr_t)(core) * 0x20000 + (off)))

/* ITS Command Buffer (64KB aligned) */
#define ITS_CMD_QUEUE_SIZE      (64 * 1024)
#define ITS_CMD_SIZE            32

static uint8_t s_cmd_queue[ITS_CMD_QUEUE_SIZE] __attribute__((aligned(64 * 1024)));
static uint8_t s_device_table[64 * 1024] __attribute__((aligned(64 * 1024)));
static uint8_t s_collection_table[64 * 1024] __attribute__((aligned(64 * 1024)));
static uint8_t s_itt_tables[PCI_MAX_DEVICES][4096] __attribute__((aligned(256)));

/* LPI Configuration and Pending Tables */
static uint8_t s_lpi_prop_table[64 * 1024] __attribute__((aligned(64 * 1024)));
static uint8_t s_lpi_pend_table[configNUMBER_OF_CORES][64 * 1024] __attribute__((aligned(64 * 1024)));

static bool s_its_present = false;
static uint32_t s_cmd_write_ptr = 0;
static uint32_t s_next_lpi = GIC_LPI_BASE;
static bool s_device_mapped[PCI_MAX_DEVICES] = {false};

static void its_send_raw_cmd(const uint64_t cmd[4])
{
    volatile uint64_t *dst = (volatile uint64_t *)&s_cmd_queue[s_cmd_write_ptr];
    dst[0] = cmd[0];
    dst[1] = cmd[1];
    dst[2] = cmd[2];
    dst[3] = cmd[3];

    __asm__ volatile("dmb sy" ::: "memory");

    s_cmd_write_ptr = (s_cmd_write_ptr + ITS_CMD_SIZE) % ITS_CMD_QUEUE_SIZE;
    GITS_REG64(GITS_CWRITER) = s_cmd_write_ptr;

    /* Wait for ITS hardware to process the command */
    volatile int timeout = 1000000;
    while ((GITS_REG64(GITS_CREADR) != s_cmd_write_ptr) && --timeout) {
        __asm__ volatile("yield");
    }
}

static void its_cmd_mapd(uint32_t dev_id, uintptr_t itt_addr, int size_bits, bool valid)
{
    uint64_t cmd[4] = {0};
    cmd[0] = GITS_CMD_MAPD | ((uint64_t)dev_id << 32);
    cmd[1] = (uint64_t)(size_bits - 1) & 0x1F;
    cmd[2] = (itt_addr & ~0xFFULL) | (valid ? (1ULL << 63) : 0);
    cmd[3] = 0;
    its_send_raw_cmd(cmd);
}

static void its_cmd_mapc(uint32_t coll_id, uint64_t target_rd, bool valid)
{
    uint64_t cmd[4] = {0};
    cmd[0] = GITS_CMD_MAPC | ((uint64_t)coll_id << 32);
    cmd[1] = 0;
    cmd[2] = (target_rd & ~0xFFFFULL) | (valid ? (1ULL << 63) : 0);
    cmd[3] = 0;
    its_send_raw_cmd(cmd);
}

static void its_cmd_mapti(uint32_t dev_id, uint32_t event_id, uint32_t intid, uint32_t coll_id)
{
    uint64_t cmd[4] = {0};
    cmd[0] = GITS_CMD_MAPTI | ((uint64_t)dev_id << 32);
    cmd[1] = (uint64_t)event_id | ((uint64_t)intid << 32);
    cmd[2] = (uint64_t)coll_id;
    cmd[3] = 0;
    its_send_raw_cmd(cmd);
}

static void its_cmd_sync(uint64_t target_rd)
{
    uint64_t cmd[4] = {0};
    cmd[0] = GITS_CMD_SYNC;
    cmd[1] = 0;
    cmd[2] = target_rd & ~0xFFFFULL;
    cmd[3] = 0;
    its_send_raw_cmd(cmd);
}

static void its_cmd_invall(uint32_t coll_id)
{
    uint64_t cmd[4] = {0};
    cmd[0] = GITS_CMD_INVALL | ((uint64_t)coll_id << 32);
    cmd[1] = 0;
    cmd[2] = 0;
    cmd[3] = 0;
    its_send_raw_cmd(cmd);
}

bool its_is_available(void)
{
    return s_its_present;
}

int its_init(void)
{
    /* Probe GITS existence */
    uint32_t iidr = GITS_REG32(GITS_IIDR);
    if (iidr == 0 || iidr == 0xFFFFFFFF) {
        pr_warn("cherry_pci: gicv3 its not present\n");
        s_its_present = false;
        return -1;
    }

    uint64_t typer = GITS_REG64(GITS_TYPER);
    bool pta = (typer & GITS_TYPER_PTA) != 0;

    /* 1. Initialize LPI Property Table: priority 0xA0, disabled */
    for (int i = 0; i < 64 * 1024; i++) {
        s_lpi_prop_table[i] = 0xA0;
    }

    /* 2. Configure Redistributor for each CPU core */
    for (int core = 0; core < configNUMBER_OF_CORES; core++) {
        /* Clear pending table */
        for (int i = 0; i < 64 * 1024; i++) {
            s_lpi_pend_table[core][i] = 0;
        }

        /* Set GICR_PROPBASER: Inner Shareable, Normal WB, Size = 13 (8192 LPIs) */
        uint64_t propbaser = (1ULL << 10) | (5ULL << 59)
                           | ((uintptr_t)s_lpi_prop_table & ~0xFFFULL)
                           | 13ULL;
        GICR_REG64(core, GICR_PROPBASER_REG) = propbaser;

        /* Set GICR_PENDBASER */
        uint64_t pendbaser = GICR_PENDBASER_PTZ | (1ULL << 10) | (5ULL << 59)
                           | ((uintptr_t)s_lpi_pend_table[core] & ~0xFFFFULL);
        GICR_REG64(core, GICR_PENDBASER_REG) = pendbaser;

        /* Enable LPIs on this Redistributor */
        GICR_REG32(core, GICR_CTLR_OFFSET) |= GICR_CTLR_ENABLE_LPIS;
    }

    /* 3. Configure GITS (ITS) */
    GITS_REG32(GITS_CTLR) = 0;  /* Disable while configuring */

    /* Setup Command Queue */
    s_cmd_write_ptr = 0;
    uint64_t cbaser = GITS_CBASER_VALID | GITS_CBASER_INNER_SHARE | GITS_CBASER_WA_WB
                    | ((uintptr_t)s_cmd_queue & ~0xFFFULL)
                    | (16 - 1); /* 64KB queue (16 * 4KB) */
    GITS_REG64(GITS_CBASER) = cbaser;
    GITS_REG64(GITS_CWRITER) = 0;

    /* Setup Device Table (BASER0) */
    uint64_t baser0 = GITS_BASER_VALID | GITS_BASER_TYPE_DEVICE | GITS_BASER_INNER_SHARE
                    | GITS_BASER_PAGE_SIZE_64K
                    | ((uintptr_t)s_device_table & ~0xFFFFULL);
    GITS_REG64(GITS_BASER(0)) = baser0;

    /* Setup Collection Table (BASER1) */
    uint64_t baser1 = GITS_BASER_VALID | GITS_BASER_TYPE_COLL | GITS_BASER_INNER_SHARE
                    | GITS_BASER_PAGE_SIZE_64K
                    | ((uintptr_t)s_collection_table & ~0xFFFFULL);
    GITS_REG64(GITS_BASER(1)) = baser1;

    /* Enable ITS */
    GITS_REG32(GITS_CTLR) = GITS_CTLR_ENABLED;

    /* 4. Map Collections for each CPU core */
    for (int core = 0; core < configNUMBER_OF_CORES; core++) {
        uint64_t target_rd = pta ? ((uintptr_t)GICR_BASE + (uintptr_t)core * 0x20000) : (uint64_t)core;
        its_cmd_mapc(core, target_rd, true);
        its_cmd_sync(target_rd);
    }

    s_its_present = true;
    pr_info("cherry_pci: gicv3 its engine initialized (cbaser=0x%lx, translater=0x%lx, pta=%d)\n",
            (unsigned long)cbaser, (unsigned long)GITS_TRANSLATER, (int)pta);
    return 0;
}

int its_alloc_msi(struct pci_dev *dev, isr_fn_t handler, void *arg, uint32_t *out_lpi, uint32_t *out_event_id)
{
    if (!s_its_present) {
        return -1;
    }

    if (s_next_lpi >= (GIC_LPI_BASE + GIC_LPI_COUNT)) {
        pr_err("cherry_pci: its out of lpi vectors\n");
        return -1;
    }

    uint32_t lpi = s_next_lpi++;
    uint32_t event_id = 0;
    uint32_t dev_id = dev->bdf;
    uint32_t dev_idx = dev->dev;

    /* Map device to its ITT if not mapped yet */
    if (!s_device_mapped[dev_idx]) {
        uintptr_t itt_addr = (uintptr_t)s_itt_tables[dev_idx];
        its_cmd_mapd(dev_id, itt_addr, 4 /* 16 events */, true);
        s_device_mapped[dev_idx] = true;
    }

    /* Enable LPI in Property Table: Priority 0xA0, Enabled (bit 0 = 1) */
    s_lpi_prop_table[lpi - GIC_LPI_BASE] = 0xA0 | 0x01;
    __asm__ volatile("dmb sy" ::: "memory");

    /* Register interrupt with ARCH dispatcher */
    arch_irq_register(lpi, (arch_irq_handler_t)handler, arg);

    /* Map (dev_id, event_id) -> (lpi, collection 0 = core 0) */
    its_cmd_mapti(dev_id, event_id, lpi, 0);

    uint64_t typer = GITS_REG64(GITS_TYPER);
    bool pta = (typer & GITS_TYPER_PTA) != 0;
    uint64_t target_rd = pta ? (uintptr_t)GICR_BASE : 0;
    its_cmd_sync(target_rd);
    its_cmd_invall(0);

    *out_lpi = lpi;
    *out_event_id = event_id;

    pr_info("cherry_pci: mapped its msi dev_id=0x%x event=%d -> lpi=%d (core=0)\n",
            dev_id, event_id, lpi);
    return 0;
}

void its_dump_info(void)
{
    if (!s_its_present) {
        pr_info("cherry_pci: its not present\n");
        return;
    }
    uint32_t ctlr = GITS_REG32(GITS_CTLR);
    uint64_t cbaser = GITS_REG64(GITS_CBASER);
    uint64_t cwriter = GITS_REG64(GITS_CWRITER);
    uint64_t creadr = GITS_REG64(GITS_CREADR);
    pr_info("cherry_pci: [its-status] ctlr=0x%x cbaser=0x%lx cwriter=%lu creadr=%lu allocated_lpi=%d\n",
            ctlr, (unsigned long)cbaser, (unsigned long)cwriter, (unsigned long)creadr, (int)(s_next_lpi - GIC_LPI_BASE));
}
