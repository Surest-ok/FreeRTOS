/*
 * ARM Generic Timer Driver for QEMU virt AArch64 (GICv3)
 * EL1 Physical Timer: CNTP_TVAL_EL0 / CNTP_CTL_EL0
 * Interrupt: PPI 14 → INTID 30
 */

#include <stdint.h>
#include "platform.h"
#include "gtimer.h"
#include "gic.h"
#include "uart.h"

static uint64_t timer_step;
static volatile uint64_t gtimer_ticks;

#define read_sysreg(reg) ({ \
    uint64_t _val; \
    __asm__ volatile("mrs %0, " reg : "=r"(_val)); \
    _val; \
})

#define write_sysreg(reg, val) do { \
    __asm__ volatile("msr " reg ", %0" :: "r"((uint64_t)(val))); \
    __asm__ volatile("isb"); \
} while(0)

void gtimer_init(void)
{
    uint64_t freq = read_sysreg("cntfrq_el0");
    timer_step = freq / GTIMER_TICK_RATE;

    uart_printf("[Timer] freq=%d Hz, step=%d\r\n", (uint32_t)freq, (uint32_t)timer_step);

    /* Disable timer */
    write_sysreg("cntp_ctl_el0", 0);
    write_sysreg("cntp_tval_el0", timer_step);

    /* Enable GIC interrupt */
    gic_enable_irq(IRQ_TIMER);
    gic_set_priority(IRQ_TIMER, 0x80);

    gtimer_ticks = 0;
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

void gtimer_handler(void)
{
    gtimer_ticks++;
    gtimer_reload();
    if (gtimer_ticks % 100 == 0)
        uart_printf("[Timer] tick %d (%d sec)\r\n",
                    (uint32_t)gtimer_ticks, (uint32_t)(gtimer_ticks / 100));
}
