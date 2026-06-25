#include <stdint.h>
#include "arch_api/arch.h"
#include "arch_api/exception.h"
#include "uart.h"

void vPortFatalException(uint64_t esr, uint64_t elr, uint64_t far, uint64_t spsr)
{
    arch_exception_frame_t frame = {
        .elr = elr,
        .spsr = spsr,
        .esr = esr,
        .far = far,
    };

    arch_exception_record(&frame);
    uart_log(0, "freertos",
             "[panic] core=%d reason=exception esr=0x%x elr=0x%x far=0x%x spsr=0x%x\r\n",
             (int)arch_curr_cpu_id(),
             (int)esr,
             (int)elr,
             (int)far,
             (int)spsr);

    for (;;) {
        __asm__ volatile("wfi");
    }
}
