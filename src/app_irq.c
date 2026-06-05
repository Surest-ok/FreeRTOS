/*
 * Application IRQ Dispatcher for FreeRTOS on GICv3
 *
 * vApplicationIRQHandler() is called by FreeRTOS_IRQ_Handler in portASM.S
 * after reading ICC_IAR1_EL1. We dispatch based on the INTID.
 *
 * IMPORTANT: Do NOT write ICC_EOIR1_EL1 here — portASM.S handles EOI
 * after this function returns.
 */

#include <stdint.h>
#include "gic.h"
#include "uart.h"

#define MAX_IRQ_ENTRIES 256

typedef void (*isr_fn_t)(void);
static isr_fn_t isr_table[MAX_IRQ_ENTRIES];

void app_irq_install(uint32_t irq, isr_fn_t handler)
{
    if (irq < MAX_IRQ_ENTRIES)
        isr_table[irq] = handler;
}

/*
 * Called from FreeRTOS_IRQ_Handler (portASM.S) with:
 *   ulICCIAR = value read from ICC_IAR1_EL1
 *
 * portASM.S handles ICC_EOIR1_EL1 write after we return.
 */
void vApplicationIRQHandler(uint32_t ulICCIAR)
{
    uint32_t irq = ulICCIAR;

    if (irq >= 1020)
        return;  /* Spurious */

    if (irq < MAX_IRQ_ENTRIES && isr_table[irq]) {
        isr_table[irq]();
    } else {
        uart_printf("[IRQ] Unhandled: %d\r\n", irq);
    }
}
