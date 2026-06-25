/*
 * Platform interrupt definitions for QEMU virt
 */
#pragma once

#include <sys/types.h>

/* Interrupt numbers for QEMU virt machine */
#define UART0_INT       33
#define ARM_GENERIC_TIMER_PHYSICAL_INT  30
#define ARM_GENERIC_TIMER_VIRTUAL_INT   27
#define ARM_GENERIC_TIMER_HYP_INT       26
#define ARM_GENERIC_TIMER_SEC_INT       29

/* Total number of interrupts */
#define MAX_INT 1024

/* Interrupt handler function pointer type */
typedef enum handler_return (*int_handler)(void *arg);

/* Interrupt API */
void register_int_handler(unsigned int vector, int_handler handler, void *arg);
status_t unmask_interrupt(unsigned int vector);
status_t mask_interrupt(unsigned int vector);
