/*
 * Interrupt Framework Implementation for QEMU virt AArch64
 * ISR registration and dispatch (GICv2)
 */

#include <stdint.h>
#include "interrupt.h"
#include "gtimer.h"
#include "uart.h"

/* ISR table: maps interrupt ID to handler function */
static isr_handler_t isr_table[MAX_ISR_ENTRIES];

/* Install an ISR for a given interrupt ID */
void interrupt_install(uint32_t irq, isr_handler_t handler)
{
    if (irq < MAX_ISR_ENTRIES) {
        isr_table[irq] = handler;
    }
}

/* Dispatch interrupt to the registered handler
 * Called from vectors.S with the IAR value (interrupt ID)
 * Returns the IRQ ID for EOI
 */
uint32_t interrupt_dispatch(uint32_t iar)
{
    isr_handler_t handler;

    /* Spurious interrupt check */
    if (iar >= 1020) {
        return iar;
    }

    /* Look up handler */
    if (iar < MAX_ISR_ENTRIES) {
        handler = isr_table[iar];
        if (handler) {
            handler();
            return iar;
        }
    }

    /* Unhandled interrupt */
    uart_printf("[IRQ] Unhandled: %d\r\n", iar);
    return iar;
}

/* Install vector table base address (VBAR_EL1) */
void interrupt_init(void)
{
    __asm__ volatile(
        "ldr x0, =vector_table\n"
        "msr vbar_el1, x0\n"
        "isb\n"
    );
}
