/*
 * GICv2 Driver for QEMU virt AArch64
 * Distributor + CPU Interface (memory-mapped)
 *
 * QEMU virt GICv2 addresses:
 *   GICD = 0x08000000 (Distributor)
 *   GICC = 0x08010000 (CPU Interface)
 */

#ifndef __GIC_H__
#define __GIC_H__

#include <stdint.h>

/* Maximum ISR entries */
#define MAX_IRQ_NUM     1024

/* GICv2 special INTID */
#define GIC_SPURIOUS_ID  1023

/* Timer interrupt: PPI 14 → INTID 30 in GIC (PPI starts at ID 16) */
#define IRQ_TIMER        30

/* UART interrupt: SPI 1 → INTID 33 */
#define IRQ_UART         33

/* PPI range: INTID 16-31 */
#define PPI_START        16

/* SPI range: INTID 32+ */
#define SPI_START        32

/*
 * Initialize GICv2: Distributor + CPU Interface
 */
void gic_init(void);

/*
 * Enable/disable interrupt
 */
void gic_enable_irq(uint32_t irq);
void gic_disable_irq(uint32_t irq);

/*
 * Set interrupt priority (lower = higher priority)
 */
void gic_set_priority(uint32_t irq, uint8_t priority);

/*
 * Acknowledge IRQ (read from GICC_IAR)
 */
uint32_t gic_acknowledge_irq(void);

/*
 * End of interrupt (write to GICC_EOIR)
 */
void gic_end_of_interrupt(uint32_t irq);

/*
 * Send SGI
 */
void gic_send_sgi(uint32_t sgi_id, uint8_t target_list);

#endif /* __GIC_H__ */
