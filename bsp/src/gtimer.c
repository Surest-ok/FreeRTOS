/*
 * ARM Generic Timer Driver for QEMU virt AArch64 (GICv3)
 * EL1 Physical Timer: CNTP_TVAL_EL0 / CNTP_CTL_EL0
 * Interrupt: PPI 14 → INTID 30
 *
 * FreeRTOS integration:
 *   vConfigureTickInterrupt() — arms timer, installs FreeRTOS_Tick_Handler ISR
 *   vClearTickInterrupt()     — reloads timer for next tick
 */

#include <stdint.h>
#include "platform.h"
#include "gtimer.h"
#include "gic.h"
#include "uart.h"
#include "app_irq.h"

static uint64_t timer_step;

#define read_sysreg(reg) ({ \
    uint64_t _val; \
    __asm__ volatile("mrs %0, " reg : "=r"(_val)); \
    _val; \
})

#define write_sysreg(reg, val) do { \
    __asm__ volatile("msr " reg ", %0" :: "r"((uint64_t)(val))); \
    __asm__ volatile("isb"); \
} while(0)

/* FreeRTOS tick handler (defined in port.c) */
extern void FreeRTOS_Tick_Handler(void);

static uint32_t gtimer_current_core(void)
{
    uint64_t mpidr = read_sysreg("mpidr_el1");
    return (uint32_t)((mpidr & 0xFFU) + (((mpidr >> 8) & 0xFFU) << 4));
}

void gtimer_init(void)
{
    uint64_t freq = read_sysreg("cntfrq_el0");
    timer_step = freq / GTIMER_TICK_RATE;

    /* Disable timer */
    write_sysreg("cntp_ctl_el0", 0);
}

void gtimer_start(void)
{
    write_sysreg("cntp_tval_el0", timer_step);
    write_sysreg("cntp_ctl_el0", 0x1);
}

void gtimer_stop(void) { write_sysreg("cntp_ctl_el0", 0); }
void gtimer_reload(void) { write_sysreg("cntp_tval_el0", timer_step); }
uint64_t gtimer_get_frequency(void) { return read_sysreg("cntfrq_el0"); }
uint64_t gtimer_get_counter(void) { return read_sysreg("cntpct_el0"); }

/*
 * FreeRTOS tick setup — called from xPortStartScheduler() via
 * configSETUP_TICK_INTERRUPT() macro.
 *
 * Timer priority = 0xF0 = portLOWEST_USABLE_INTERRUPT_PRIORITY << portPRIORITY_SHIFT
 * = 30 << 3 = 240. This is the lowest usable priority in GICv3 with 32 levels.
 */
void vConfigureTickInterrupt(void)
{
    /* Only core 0 drives the FreeRTOS tick.
     * Other cores get scheduling via SGI IPI. */
    if (gtimer_current_core() != 0U) {
        return;
    }

    /* Ensure timer is disabled */
    write_sysreg("cntp_ctl_el0", 0);
    write_sysreg("cntp_tval_el0", timer_step);

    /* Set timer priority to lowest usable (0xF0) */
    gic_set_priority(IRQ_TIMER, 0xF0);

    /* Enable timer IRQ in GIC */
    gic_enable_irq(IRQ_TIMER);

    /* Install FreeRTOS tick handler */
    app_irq_install(IRQ_TIMER, FreeRTOS_Tick_Handler);

    /* Arm and enable timer */
    write_sysreg("cntp_tval_el0", timer_step);
    write_sysreg("cntp_ctl_el0", 0x1);
}

/*
 * Clear tick interrupt — called from FreeRTOS_Tick_Handler() via
 * configCLEAR_TICK_INTERRUPT() macro.
 */
void vClearTickInterrupt(void)
{
    write_sysreg("cntp_tval_el0", timer_step);
}
