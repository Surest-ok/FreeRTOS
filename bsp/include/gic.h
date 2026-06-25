/*
 * GICv3 Driver for QEMU virt AArch64
 * Distributor + Redistributor + CPU Interface (system registers)
 *
 * Prerequisite: ICC_SRE_EL2.SRE must be set at EL2 before dropping to EL1.
 */

#ifndef __GIC_H__
#define __GIC_H__

#include <stdint.h>

#define MAX_IRQ_NUM     1024
#define GIC_SPURIOUS_ID 1023

/* Timer: EL1 Physical Timer = PPI 14 = INTID 30 */
#define IRQ_TIMER        30

/* UART: SPI 1 = INTID 33 */
#define IRQ_UART         33

#define PPI_START        16
#define SPI_START        32

void gic_init(uint32_t core_id);
void gic_enable_irq(uint32_t irq);
void gic_disable_irq(uint32_t irq);
void gic_set_priority(uint32_t irq, uint8_t priority);
void gic_set_affinity(uint32_t irq, uint8_t affinity);
uint32_t gic_acknowledge_irq(void);
void gic_end_of_interrupt(uint32_t irq);
void gic_send_sgi(uint32_t sgi_id, uint32_t target_core);

#endif /* __GIC_H__ */
