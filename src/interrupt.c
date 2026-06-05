/*
 * Interrupt Framework for QEMU virt AArch64 (GICv3)
 * ISR registration and dispatch
 */

#include <stdint.h>
#include "interrupt.h"
#include "uart.h"

static isr_handler_t isr_table[MAX_ISR_ENTRIES];

void interrupt_install(uint32_t irq, isr_handler_t handler)
{
    if (irq < MAX_ISR_ENTRIES)
        isr_table[irq] = handler;
}

void interrupt_dispatch(uint32_t iar)
{
    isr_handler_t handler;

    if (iar >= 1020)
        return;

    if (iar < MAX_ISR_ENTRIES) {
        handler = isr_table[iar];
        if (handler) {
            handler();
            return;
        }
    }

    uart_printf("[IRQ] Unhandled: %d\r\n", iar);
}

void interrupt_init(void)
{
    __asm__ volatile(
        "ldr x0, =vector_table\n"
        "msr vbar_el1, x0\n"
        "isb\n"
    );
}
