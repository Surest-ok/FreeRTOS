/*
 * Interrupt Framework for QEMU virt AArch64 (GICv3)
 */

#ifndef __INTERRUPT_H__
#define __INTERRUPT_H__

#include <stdint.h>

#define MAX_ISR_ENTRIES  256

typedef void (*isr_handler_t)(void);

void interrupt_install(uint32_t irq, isr_handler_t handler);
void interrupt_dispatch(uint32_t iar);
void interrupt_init(void);

#endif /* __INTERRUPT_H__ */
