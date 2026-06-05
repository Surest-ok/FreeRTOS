/*
 * Phase 2: GICv3 + Generic Timer on QEMU virt
 */

#include <stdint.h>
#include "platform.h"
#include "uart.h"
#include "gic.h"
#include "gtimer.h"
#include "interrupt.h"

void secondary_cpu_entry(void)
{
    while (1) asm volatile("wfi");
}

int main(void)
{
    uint64_t el;
    asm volatile("mrs %0, CurrentEL" : "=r"(el));
    el = (el >> 2) & 3;

    uart_init();
    uart_printf("=== Phase 2: GICv3 + EL%d + Timer ===\r\n", (uint32_t)el);

    /* 1. Vector table */
    interrupt_init();
    uart_puts("[1] Vector table installed\r\n");

    /* 2. GICv3 */
    gic_init(0);
    uart_puts("[2] GICv3 initialized\r\n");

    /* 3. Timer */
    gtimer_init();
    interrupt_install(IRQ_TIMER, gtimer_handler);
    uart_puts("[3] Timer + ISR ready\r\n");

    /* 4. Enable IRQ + start timer */
    asm volatile("msr daifclr, #0x2");
    gtimer_start();
    uart_puts("[4] Timer started\r\n\r\n");

    while (1) asm volatile("wfi");
    return 0;
}
