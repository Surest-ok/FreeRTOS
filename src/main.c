/*
 * Main entry point for FreeRTOS SMP kernel prototype
 * Phase 2: Exception vectors, GICv2, Generic Timer tick
 */

#include <stdint.h>
#include "platform.h"
#include "uart.h"
#include "gic.h"
#include "gtimer.h"
#include "interrupt.h"

void secondary_cpu_entry(void)
{
    uart_printf("Secondary CPU entry (not implemented yet)\r\n");
    while (1) {
        asm volatile("wfi");
    }
}

int main(void)
{
    uart_init();
    uart_puts("=== Phase 2: Exception Vectors + GICv2 + Timer ===\r\n");

    /* Check current EL */
    uint64_t current_el;
    asm volatile("mrs %0, CurrentEL" : "=r"(current_el));
    uart_printf("Core 0 at EL%d\r\n", (uint32_t)(current_el >> 2) & 3);

    /* Step 1: Install exception vector table */
    interrupt_init();
    uart_puts("[1] Vector table installed\r\n");

    /* Step 2: Initialize GICv2 */
    gic_init();
    uart_puts("[2] GICv2 initialized\r\n");

    /* Step 3: Initialize Generic Timer */
    gtimer_init();
    uart_puts("[3] Timer initialized\r\n");

    /* Step 4: Register timer ISR */
    interrupt_install(IRQ_TIMER, gtimer_handler);
    uart_puts("[4] Timer ISR registered\r\n");

    /* Step 5: Unmask IRQ and start timer */
    asm volatile("msr daifclr, #0x2");
    gtimer_start();
    uart_puts("[5] IRQ unmasked, timer started\r\n");
    uart_puts("=== Initialization complete ===\r\n\r\n");

    /* Main loop */
    while (1) {
        asm volatile("wfi");
    }
    return 0;
}
