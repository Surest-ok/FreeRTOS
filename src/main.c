/*
 * Main entry point for FreeRTOS SMP kernel prototype
 * Phase 1: Bare-metal Hello World
 */

#include <stdint.h>
#include "platform.h"
#include "uart.h"

/* Secondary CPU entry point */
void secondary_cpu_entry(void)
{
    /* Just print a message and loop */
    uart_printf("Secondary CPU entry (not implemented yet)\n");

    /* For now, just loop indefinitely */
    while (1) {
        /* Wait for interrupt */
        asm volatile("wfi");
    }
}

/* Main function called from startup.S */
int main(void)
{
    /* Initialize UART first */
    uart_init();
    uart_puts("UART initialized\n");

    /* Print hello message with some system info */
    uart_printf("Hello from QEMU AArch64!\n");
    uart_printf("Running at EL1\n");

    /* Read MPIDR to get core ID */
    uint64_t mpidr = 0;
    asm volatile("mrs %0, MPIDR_EL1" : "=r"(mpidr));
    uint32_t core_id = mpidr & 0xFF;
    uart_printf("Core ID: %d\n", core_id);

    /* For now, just loop */
    uart_printf("Entering infinite loop...\n");
    while (1) {
        /* Do nothing */
        asm volatile("nop");
    }
    return 0;
}