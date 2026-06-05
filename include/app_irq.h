/*
 * Application IRQ Dispatcher Interface
 */

#ifndef __APP_IRQ_H__
#define __APP_IRQ_H__

#include <stdint.h>

typedef void (*isr_fn_t)(void);

void app_irq_install(uint32_t irq, isr_fn_t handler);

#endif /* __APP_IRQ_H__ */
