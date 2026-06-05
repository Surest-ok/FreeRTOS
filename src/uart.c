/*
 * PL011 UART driver implementation
 * Polling mode for simple output
 */

#include <stdint.h>
#include <stdarg.h>
#include <stddef.h>
#include "platform.h"
#include "uart.h"

/* Helper macro to write to UART register */
#define UART_REG_WRITE(base, offset, value) \
    *(volatile uint32_t *)((base) + (offset)) = (value)

/* Helper macro to read from UART register */
#define UART_REG_READ(base, offset) \
    *(volatile uint32_t *)((base) + (offset))

/* Initialize UART for 115200 baud */
void uart_init(void)
{
    uint32_t baud_divisor;

    /* Disable UART */
    UART_REG_WRITE(UART0_BASE, UART0_CR_OFFSET, 0);

    /* Set baud rate divisor */
    /* For 100MHz clock and 115200 baud: divisor = 100000000 / 115200 ≈ 868 */
    baud_divisor = 868;
    UART_REG_WRITE(UART0_BASE, UART0_IBRD_OFFSET, baud_divisor / 64);
    UART_REG_WRITE(UART0_BASE, UART0_FBRD_OFFSET, baud_divisor % 64);

    /* Configure 8-bit data, no parity, 1 stop bit */
    UART_REG_WRITE(UART0_BASE, UART0_LCRH_OFFSET,
                   (1 << 1) | (1 << 2) | (1 << 5) | (1 << 6));  // 8-bit, FIFO

    /* Enable UART */
    UART_REG_WRITE(UART0_BASE, UART0_CR_OFFSET,
                   (1 << 0) | (1 << 8) | (1 << 9));  // UARTEN, TXE, RXE

    /* Clear any pending interrupts */
    UART_REG_WRITE(UART0_BASE, UART0_ICR_OFFSET, 0x7FF);

    /* Enable FIFOs */
    UART_REG_WRITE(UART0_BASE, UART0_IFLS_OFFSET, 0);
}

/* Write a single character to UART */
void uart_putc(char c)
{
    /* Wait until transmit FIFO is not full */
    while (UART_REG_READ(UART0_BASE, UART0_FR_OFFSET) & UART_FR_TXFF) {
        /* Wait */
    }

    /* Write character to data register */
    UART_REG_WRITE(UART0_BASE, UART0_DR_OFFSET, c);
}

/* Write a null-terminated string to UART */
void uart_puts(const char *str)
{
    while (*str) {
        uart_putc(*str++);
    }
}

/* Simple vsnprintf implementation for console output */
static int vsnprintf(char *buf, size_t size, const char *fmt, va_list args)
{
    int count = 0;
    char *p = buf;
    const char *s;
    int i;
    char *tmp;

    while (*fmt && count < (size - 1)) {
        if (*fmt != '%') {
            *p++ = *fmt++;
            count++;
            continue;
        }

        fmt++;

        switch (*fmt) {
            case 'd':  /* Integer */
                i = va_arg(args, int);
                if (i < 0) {
                    *p++ = '-';
                    i = -i;
                }
                /* Convert to string (simplified) */
                tmp = p;
                do {
                    *p++ = '0' + (i % 10);
                    i /= 10;
                    count++;
                } while (i > 0 && count < (size - 1));
                /* Reverse the digits */
                while (tmp < p) {
                    char c = *tmp;
                    *tmp = *(p - 1);
                    *(p - 1) = c;
                    tmp++;
                    p--;
                }
                break;

            case 'x':  /* Hexadecimal */
                i = va_arg(args, int);
                for (int j = 7; j >= 0; j--) {
                    int nibble = (i >> (j * 4)) & 0xF;
                    *p++ = nibble < 10 ? '0' + nibble : 'A' + nibble - 10;
                    count++;
                }
                break;

            case 's':  /* String */
                s = va_arg(args, char*);
                while (*s && count < (size - 1)) {
                    *p++ = *s++;
                    count++;
                }
                break;

            case 'c':  /* Character */
                *p++ = (char)va_arg(args, int);
                count++;
                break;

            case '%':  /* Percent sign */
                *p++ = '%';
                count++;
                break;

            default:
                *p++ = *fmt;
                count++;
        }

        fmt++;
    }

    *p = '\0';
    return count;
}

/* Formatted print to UART */
void uart_printf(const char *fmt, ...)
{
    char buf[256];
    va_list args;

    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    uart_puts(buf);
}