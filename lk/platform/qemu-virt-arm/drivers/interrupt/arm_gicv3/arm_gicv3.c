/*
 * GICv3 driver for LK (ported from ThreadX)
 *
 * QEMU virt platform GIC memory map:
 * - GIC Distributor (GICD): 0x08000000
 * - GIC Redistributor (GICR): 0x080A0000 (per-CPU)
 * - GIC CPU Interface: System registers (ICC_*)
 *
 * Use of this source code is governed by a MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT
 */

#include <assert.h>
#include <arch_api/irq.h>
#include <lk/bits.h>
#include <lk/err.h>
#include <sys/types.h>
#include <lk/debug.h>
#include <dev/interrupt/arm_gicv3.h>
#include <lk/reg.h>
#include <kernel/thread.h>
#include <kernel/debug.h>
#include <lk/init.h>
#include <platform/interrupts.h>
#include <arch/ops.h>
#include <platform/gic.h>
#include <lk/trace.h>

#define LOCAL_TRACE 0

#if ARCH_ARM64
#include <arch/arm64.h>
#define iframe arm64_iframe_short
#define IFRAME_PC(frame) ((frame)->elr)
#endif

/* QEMU virt platform GIC base addresses (physical) */
#define GICD_BASE_PHYS      0x08000000
/* For GICv3, GICR is at different offset in QEMU virt */
/* Per CPU: RD_base = 0x080A0000 + (cpu_id * 0x20000) */
#define GICR_RD_BASE_PHYS   0x080A0000
#define GICR_SIZE_PER_CPU   0x20000     /* 128KB per CPU */
#define GICR_SGI_OFFSET     0x10000      /* SGI base is RD_base + 64KB */

/* Virtual addresses - will be mapped by platform */
static uintptr_t gicd_base_virt = 0;
static uintptr_t gicr_rd_base_virt = 0;
static uintptr_t gicr_sgi_base_virt = 0;

/* Register accessors using LK's MMIO functions */
static inline uint32_t gic_read32(uintptr_t addr) {
    uint32_t val = mmio_read32((volatile uint32_t *)addr);
    DSB;
    return val;
}

static inline void gic_write32(uintptr_t addr, uint32_t val) {
    DSB;
    mmio_write32((volatile uint32_t *)addr, val);
    DSB;
}

#define GICD_READ(offset)       gic_read32(gicd_base_virt + (offset))
#define GICD_WRITE(offset, val) gic_write32(gicd_base_virt + (offset), (val))
#define GICR_RD_READ(offset)    gic_read32(gicr_rd_base_virt + (offset))
#define GICR_RD_WRITE(offset, val) gic_write32(gicr_rd_base_virt + (offset), (val))
#define GICR_SGI_READ(offset)   gic_read32(gicr_sgi_base_virt + (offset))
#define GICR_SGI_WRITE(offset, val) gic_write32(gicr_sgi_base_virt + (offset), (val))

/* Register offsets */
#define GICD_CTLR_OFFSET        0x0000
#define GICD_TYPER_OFFSET       0x0004
#define GICD_ISENABLER_OFFSET   0x0100
#define GICD_ICENABLER_OFFSET   0x0180
#define GICD_IPRIORITYR_OFFSET  0x0400
#define GICD_ITARGETSR_OFFSET   0x0800
#define GICD_ICFGR_OFFSET       0x0C00

#define GICR_WAKER_OFFSET       0x0014
#define GICR_IGROUPR0_OFFSET    0x0080
#define GICR_ISENABLER0_OFFSET  0x0100
#define GICR_ICENABLER0_OFFSET  0x0180
#define GICR_IPRIORITYR_OFFSET  0x0400

/* GICR_WAKER bits */
#define GICR_WAKER_PROCESSOR_SLEEP  (1 << 1)
#define GICR_WAKER_CHILDREN_ASLEEP  (1 << 2)

/* ICC System Register access macros */
#define READ_ICC_IAR1()          ({ uint64_t __val; __asm__ volatile("mrs %0, S3_0_C12_C12_0" : "=r"(__val)); __val; })
#define WRITE_ICC_EOIR1(val)     __asm__ volatile("msr S3_0_C12_C12_1, %0" : : "r"((uint64_t)(val)))
#define WRITE_ICC_PMR(val)       __asm__ volatile("msr S3_0_C4_C6_0, %0" : : "r"((uint64_t)(val)))
#define WRITE_ICC_IGRPEN1(val)   __asm__ volatile("msr S3_0_C12_C12_7, %0" : : "r"((uint64_t)(val)))
#define WRITE_ICC_CTLR(val)      __asm__ volatile("msr S3_0_C12_C12_4, %0" : : "r"((uint64_t)(val)))
#define READ_ICC_SRE()           ({ uint64_t __val; __asm__ volatile("mrs %0, S3_0_C12_C12_5" : "=r"(__val)); __val; })
#define WRITE_ICC_SRE(val)       __asm__ volatile("msr S3_0_C12_C12_5, %0" : : "r"((uint64_t)(val)))
#define WRITE_ICC_BPR1(val)      __asm__ volatile("msr S3_0_C12_C12_3, %0" : : "r"((uint64_t)(val)))

/* Interrupt handler table */
struct int_handler_struct {
    int_handler handler;
    void *arg;
};

#define GIC_MAX_PER_CPU_INT 32
static struct int_handler_struct int_handler_table_per_cpu[GIC_MAX_PER_CPU_INT][SMP_MAX_CPUS];
static struct int_handler_struct int_handler_table_shared[MAX_INT-GIC_MAX_PER_CPU_INT];

static struct int_handler_struct *get_int_handler(unsigned int vector, uint cpu) {
    if (vector < GIC_MAX_PER_CPU_INT) {
        return &int_handler_table_per_cpu[vector][cpu];
    } else {
        return &int_handler_table_shared[vector - GIC_MAX_PER_CPU_INT];
    }
}

void register_int_handler(unsigned int vector, int_handler handler, void *arg) {
    struct int_handler_struct *h;
    uint cpu = arch_curr_cpu_num();

    if (vector >= MAX_INT) {
        panic("register_int_handler: vector out of range %d\n", vector);
    }

    h = get_int_handler(vector, cpu);
    h->handler = handler;
    h->arg = arg;
}

/* Wake up GICR */
static void gicv3_wakeup_gicr(void) {
    uint32_t waker_value;

    dprintf(ALWAYS, "GICv3: Waking up GICR at 0x%lx...\n", gicr_rd_base_virt);

    /* Read GICR_WAKER */
    waker_value = GICR_RD_READ(GICR_WAKER_OFFSET);
    dprintf(ALWAYS, "GICv3: GICR_WAKER initial value: 0x%x\n", waker_value);

    /* Clear ProcessorSleep bit to wake up the redistributor */
    waker_value &= ~GICR_WAKER_PROCESSOR_SLEEP;
    GICR_RD_WRITE(GICR_WAKER_OFFSET, waker_value);
    DSB;

    /* Wait for ChildrenAsleep to clear */
    int timeout = 1000000;
    while (((GICR_RD_READ(GICR_WAKER_OFFSET) & GICR_WAKER_CHILDREN_ASLEEP) != 0) && timeout-- > 0) {
        /* Wait */
    }

    if (timeout <= 0) {
        dprintf(ALWAYS, "GICv3: WARNING - GICR wakeup timeout\n");
    } else {
        dprintf(ALWAYS, "GICv3: GICR is awake\n");
    }
}

/* Configure CPU interface - must be called per CPU */
static void gicv3_cpu_init(uint cpu_id) {
    dprintf(ALWAYS, "GICv3: Configuring CPU %u interface\n", cpu_id);

    /* Set priority mask to allow all priorities */
    WRITE_ICC_PMR(0xff);
    ISB;

    /* Enable Group 1 interrupts */
    WRITE_ICC_IGRPEN1(1);
    ISB;

    /* Set binary point register to 0 (no priority grouping) */
    WRITE_ICC_BPR1(0);
    ISB;

    /* Set control register (EOImode=0) */
    WRITE_ICC_CTLR(0);
    ISB;

    dprintf(ALWAYS, "GICv3: CPU %u interface configured\n", cpu_id);
}

/* Initialize GICv3 for secondary CPU - must be called on each secondary CPU */
void arm_gicv3_init_percpu(void) {
    uint cpu_id = arch_curr_cpu_num();

    dprintf(ALWAYS, "GICv3: Per-CPU init for CPU %u\n", cpu_id);

    /* Calculate this CPU's GICR base address */
    uintptr_t cpu_gicr_rd_base = gicr_rd_base_virt + (cpu_id * GICR_SIZE_PER_CPU);
    uintptr_t cpu_gicr_sgi_base = cpu_gicr_rd_base + GICR_SGI_OFFSET;

    /* Temporarily update globals for this CPU's GICR access */
    uintptr_t saved_rd = gicr_rd_base_virt;
    uintptr_t saved_sgi = gicr_sgi_base_virt;
    gicr_rd_base_virt = cpu_gicr_rd_base;
    gicr_sgi_base_virt = cpu_gicr_sgi_base;

    /* Wake up this CPU's GICR */
    uint32_t waker_value = GICR_RD_READ(GICR_WAKER_OFFSET);
    dprintf(ALWAYS, "GICv3: CPU %u GICR_WAKER = 0x%x\n", cpu_id, waker_value);

    waker_value &= ~GICR_WAKER_PROCESSOR_SLEEP;
    GICR_RD_WRITE(GICR_WAKER_OFFSET, waker_value);
    DSB;

    int timeout = 1000000;
    while (((GICR_RD_READ(GICR_WAKER_OFFSET) & GICR_WAKER_CHILDREN_ASLEEP) != 0) && timeout-- > 0);

    if (timeout <= 0) {
        dprintf(ALWAYS, "GICv3: WARNING - CPU %u GICR wakeup timeout\n", cpu_id);
    } else {
        dprintf(ALWAYS, "GICv3: CPU %u GICR is awake\n", cpu_id);
    }

    /* Enable SGI interrupts 14 and 15 (IPI reschedule and generic) for this CPU */
    dprintf(ALWAYS, "GICv3: CPU %u enabling SGIs 14-15\n", cpu_id);

    /* Set interrupts to Group 1 */
    uint32_t igroupr = GICR_SGI_READ(GICR_IGROUPR0_OFFSET);
    igroupr |= (1 << 14) | (1 << 15);
    GICR_SGI_WRITE(GICR_IGROUPR0_OFFSET, igroupr);

    /* Set priorities */
    for (int irq = 14; irq <= 15; irq++) {
        uint32_t priority_offset = GICR_IPRIORITYR_OFFSET + (irq & ~0x3);
        uint32_t priority_shift = (irq & 0x3) * 8;
        uint32_t priority_val = GICR_SGI_READ(priority_offset);
        priority_val &= ~(0xff << priority_shift);
        priority_val |= (0xa0 << priority_shift);
        GICR_SGI_WRITE(priority_offset, priority_val);
    }

    /* Enable the interrupts */
    GICR_SGI_WRITE(GICR_ISENABLER0_OFFSET, (1 << 14) | (1 << 15));
    DSB;

    dprintf(ALWAYS, "GICv3: CPU %u SGIs 14-15 enabled\n", cpu_id);

    /* Restore CPU 0's GICR base for future use */
    gicr_rd_base_virt = saved_rd;
    gicr_sgi_base_virt = saved_sgi;

    /* Configure CPU interface */
    gicv3_cpu_init(cpu_id);

    dprintf(ALWAYS, "GICv3: CPU %u initialization complete\n", cpu_id);
}

/* Main GICv3 initialization */
void arm_gicv3_init(void) {
    uint32_t typer;

    dprintf(ALWAYS, "GICv3: Initializing GICv3 for QEMU virt\n");

    /* Map GIC registers to virtual addresses */
    gicd_base_virt = GICBASE(0);  /* Platform provides this */
    gicr_rd_base_virt = gicd_base_virt + (GICR_RD_BASE_PHYS - GICD_BASE_PHYS);
    gicr_sgi_base_virt = gicr_rd_base_virt + GICR_SGI_OFFSET;

    dprintf(SPEW, "GICv3: GICD base virt = 0x%lx\n", gicd_base_virt);
    dprintf(SPEW, "GICv3: GICR RD base virt = 0x%lx\n", gicr_rd_base_virt);
    dprintf(SPEW, "GICv3: GICR SGI base virt = 0x%lx\n", gicr_sgi_base_virt);

    /* GICv3 system register interface should already be enabled by EL2/EL3 */
    /* In QEMU, it's enabled by default, so we skip ICC_SRE access */
    dprintf(ALWAYS, "GICv3: Assuming system register interface is enabled\n");

    /* Disable distributor before configuration */
    dprintf(ALWAYS, "GICv3: Disabling distributor\n");
    GICD_WRITE(GICD_CTLR_OFFSET, 0);

    /* Read GICD_TYPER */
    typer = GICD_READ(GICD_TYPER_OFFSET);
    dprintf(ALWAYS, "GICv3: GICD_TYPER = 0x%x\n", typer);

    /* Wake up GICR */
    gicv3_wakeup_gicr();

    /* Enable distributor with Affinity Routing Enabled (ARE) */
    dprintf(ALWAYS, "GICv3: Enabling distributor with ARE\n");
    /* GICD_CTLR: EnableGrp0=1, EnableGrp1=1, ARE_S=1, ARE_NS=1 */
    GICD_WRITE(GICD_CTLR_OFFSET, (1 << 0) | (1 << 1) | (1 << 4) | (1 << 5));
    ISB;

    /* Configure CPU 0 interface */
    gicv3_cpu_init(0);

    dprintf(ALWAYS, "GICv3: Initialization complete\n");
}

/* Enable interrupt */
status_t unmask_interrupt(unsigned int vector) {
    if (vector >= MAX_INT) {
        return ERR_INVALID_ARGS;
    }

    if (vector < 32) {
        /* PPI/SGI - use redistributor SGI base */
        dprintf(SPEW, "GICv3: Enabling PPI/SGI %u via GICR\n", vector);

        /* Set interrupt group to Group 1 (non-secure) */
        GICR_SGI_WRITE(GICR_IGROUPR0_OFFSET, GICR_SGI_READ(GICR_IGROUPR0_OFFSET) | (1 << vector));

        /* Set priority (lower value = higher priority) */
        uint32_t priority_offset = GICR_IPRIORITYR_OFFSET + (vector & ~0x3);
        uint32_t priority_shift = (vector & 0x3) * 8;
        uint32_t priority_val = GICR_SGI_READ(priority_offset);
        priority_val &= ~(0xff << priority_shift);
        priority_val |= (0xa0 << priority_shift);  /* Priority 0xa0 */
        GICR_SGI_WRITE(priority_offset, priority_val);

        /* Enable the interrupt */
        GICR_SGI_WRITE(GICR_ISENABLER0_OFFSET, (1 << vector));
        DSB;

        dprintf(SPEW, "GICv3: PPI/SGI %u enabled successfully\n", vector);
    } else {
        /* SPI - use distributor */
        unsigned int reg = (vector / 32);
        unsigned int bit = vector % 32;
        dprintf(SPEW, "GICv3: Enabling SPI %u\n", vector);

        /* Set interrupt group to Group 1 */
        unsigned int group_reg = vector / 32;
        GICD_WRITE(0x0080 + (group_reg * 4), GICD_READ(0x0080 + (group_reg * 4)) | (1 << bit));

        /* Enable the interrupt */
        GICD_WRITE(GICD_ISENABLER_OFFSET + (reg * 4), (1 << bit));
        DSB;
    }

    return NO_ERROR;
}

/* Disable interrupt */
status_t mask_interrupt(unsigned int vector) {
    if (vector >= MAX_INT) {
        return ERR_INVALID_ARGS;
    }

    if (vector < 32) {
        GICR_SGI_WRITE(GICR_ICENABLER0_OFFSET, (1 << vector));
    } else {
        unsigned int reg = (vector / 32);
        unsigned int bit = vector % 32;
        GICD_WRITE(GICD_ICENABLER_OFFSET + (reg * 4), (1 << bit));
    }

    return NO_ERROR;
}

/* Platform IRQ handler */
static enum handler_return __platform_irq(struct iframe *frame) {
    /* Get interrupt ID from ICC_IAR1 */
    uint32_t iar = (uint32_t)READ_ICC_IAR1();
    unsigned int vector = iar & 0x3ff;

    if (vector >= 0x3fe) {
        /* Spurious interrupt */
        return INT_NO_RESCHEDULE;
    }

    THREAD_STATS_INC(interrupts);

    uint cpu = arch_curr_cpu_num();
    arch_irq_note_enter(vector, cpu);

    LTRACEF_LEVEL(2, "GICv3 IRQ: vector %d, cpu %u\n", vector, cpu);

    /* Call registered handler */
    enum handler_return ret = INT_NO_RESCHEDULE;
    struct int_handler_struct *handler = get_int_handler(vector, cpu);
    int handled = 0;
    if (handler->handler) {
        ret = handler->handler(handler->arg);
        handled = 1;
    } else {
        handled = arch_irq_dispatch(vector);
    }
    arch_irq_note_exit(vector, cpu, handled);

    /* Signal end of interrupt */
    WRITE_ICC_EOIR1(iar);

    return ret;
}

enum handler_return platform_irq(struct iframe *frame);
enum handler_return platform_irq(struct iframe *frame) {
    return __platform_irq(frame);
}

void platform_fiq(struct iframe *frame);
void platform_fiq(struct iframe *frame) {
    PANIC_UNIMPLEMENTED;
}

/* Send SGI (Software Generated Interrupt) - for SMP IPI */
status_t arm_gic_sgi(uint irq, uint flags, uint cpu_mask) {
    (void)flags;

    if (irq >= 16) {
        return ERR_INVALID_ARGS;
    }

    /* ICC_SGI1R format:
     * [55:48] - Aff3
     * [39:32] - Aff2
     * [23:16] - Aff1
     * [15:0]  - TargetList (bit per CPU in Aff0)
     */
    for (uint aff1 = 0; aff1 < ((SMP_MAX_CPUS + 15U) / 16U); aff1++) {
        uint16_t target_list = (cpu_mask >> (aff1 * 16U)) & 0xffffU;
        if (target_list == 0) {
            continue;
        }

        uint64_t sgi_val = ((uint64_t)irq << 24) |
                           ((uint64_t)aff1 << 16) |
                           target_list;
        __asm__ volatile("msr S3_0_C12_C11_5, %0" : : "r"(sgi_val));
        ISB;
    }

    ISB;

    return NO_ERROR;
}
