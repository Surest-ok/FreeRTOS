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
#include "arch_api/arch.h"
#include "arch_api/irq.h"
#include "app_irq.h"
#include "gic.h"
#include "uart.h"

static int app_irq_trampoline(uint32_t irq, void *arg)
{
    isr_fn_t handler = (isr_fn_t)arg;
    (void)irq;

    if (handler != 0) {
        handler();
        return 1;
    }

    return 0;
}

void app_irq_install(uint32_t irq, isr_fn_t handler)
{
    (void)arch_irq_register(irq, app_irq_trampoline, (void *)handler);
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
    uint32_t cpu = arch_curr_cpu_id();

    if (irq >= 1020 && irq < 8192) {
        return;  /* Spurious / reserved */
    }

    arch_irq_note_enter(irq, cpu);
    int handled = arch_irq_dispatch(irq);
    arch_irq_note_exit(irq, cpu, handled);

    if (handled == 0) {
        uart_log(4, "freertos", "[irq] unhandled=%d\r\n", (int)irq);
    }
}
