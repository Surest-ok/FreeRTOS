/*
 * ARM Generic Timer Driver for QEMU virt AArch64
 * EL1 Physical Timer: CNTP_TVAL_EL0 / CNTP_CTL_EL0
 * Interrupt: PPI 14 → INTID 30
 *
 * FreeRTOS tick integration:
 *   vConfigureTickInterrupt() — called by xPortStartScheduler()
 *   vClearTickInterrupt()     — called by FreeRTOS_Tick_Handler()
 */

#ifndef __GTIMER_H__
#define __GTIMER_H__

#include <stdint.h>

/* Tick rate for FreeRTOS (must match configTICK_RATE_HZ) */
#define GTIMER_TICK_RATE    1000

void gtimer_init(void);
void gtimer_start(void);
void gtimer_stop(void);
void gtimer_reload(void);
uint64_t gtimer_get_frequency(void);
uint64_t gtimer_get_counter(void);

/* FreeRTOS tick hooks */
void vConfigureTickInterrupt(void);
void vClearTickInterrupt(void);

#endif /* __GTIMER_H__ */
