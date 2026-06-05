/*
 * ARM Generic Timer Driver for QEMU virt AArch64
 * Uses the EL1 Physical Timer (CNTP_TVAL_EL0 / CNTP_CTL_EL0)
 */

#ifndef __GTIMER_H__
#define __GTIMER_H__

#include <stdint.h>

/* Default tick rate (ticks per second) */
#define GTIMER_TICK_RATE    100

/*
 * Initialize the Generic Timer
 * - Reads frequency from CNTFRQ_EL0
 * - Configures timer for periodic interrupts at tick_rate Hz
 * - Enables timer interrupt in GIC
 */
void gtimer_init(void);

/*
 * Start the timer (enable CNTP_CTL_EL0)
 */
void gtimer_start(void);

/*
 * Stop the timer (disable CNTP_CTL_EL0)
 */
void gtimer_stop(void);

/*
 * Reload the timer for next tick
 * Called from the ISR to re-arm the timer
 */
void gtimer_reload(void);

/*
 * Get the timer frequency (CNTFRQ_EL0)
 */
uint64_t gtimer_get_frequency(void);

/*
 * Get current counter value (CNTPCT_EL0)
 */
uint64_t gtimer_get_counter(void);

/*
 * Timer ISR callback - called from interrupt_dispatch
 */
void gtimer_handler(void);

#endif /* __GTIMER_H__ */
