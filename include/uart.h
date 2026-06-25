/*
 * PL011 UART driver for QEMU virt AArch64
 * Polling mode for now (no interrupts)
 */

#ifndef __UART_H__
#define __UART_H__

#include <stdint.h>

/* Initialize UART */
void uart_init(void);

/* Write a character to UART */
void uart_putc(char c);

/* Write a null-terminated string to UART */
void uart_puts(const char *str);

/* Write formatted string to UART (simple implementation) */
void uart_printf(const char *fmt, ...);
void uart_log(int level, const char *tag, const char *fmt, ...);

#endif /* __UART_H__ */
