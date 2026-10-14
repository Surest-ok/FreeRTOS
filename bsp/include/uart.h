/*
 * PL011 UART driver for QEMU virt AArch64
 * Polling mode for now (no interrupts)
 */

#ifndef __UART_H__
#define __UART_H__

#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>

/* Initialize UART */
void uart_init(void);

/* Write a character to UART */
void uart_putc(char c);

/* Write a null-terminated string to UART */
void uart_puts(const char *str);

/* Read character from UART */
char uart_getc(void);
int  uart_getc_nonblock(void);

/* Formatted printing */
void uart_printf(const char *fmt, ...);
void uart_log(int level, const char *tag, const char *fmt, ...);
int vsnprintf(char *buf, size_t size, const char *fmt, va_list args);
int snprintf(char *buf, size_t size, const char *fmt, ...);

#endif /* __UART_H__ */
