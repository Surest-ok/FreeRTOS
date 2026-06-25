/*
 * PL011 UART driver implementation
 * Polling mode for simple output
 */

#include <stdint.h>
#include <stdarg.h>
#include <stddef.h>
#include "platform.h"
#include "uart.h"
#include "port_smp.h"

#ifndef APP_LOG_LEVEL
#define APP_LOG_LEVEL 6
#endif

/* Helper macro to write to UART register */
#define UART_REG_WRITE(base, offset, value) \
    *(volatile uint32_t *)((base) + (offset)) = (value)

/* Helper macro to read from UART register */
#define UART_REG_READ(base, offset) \
    *(volatile uint32_t *)((base) + (offset))

/* Initialize UART for QEMU virt PL011
 * QEMU's emulated PL011 does not need baud rate configuration;
 * it just passes characters through. We only need to enable it.
 */
void uart_init(void)
{
    /* Disable UART while configuring */
    UART_REG_WRITE(UART0_BASE, UART0_CR_OFFSET, 0);

    /* Clear any pending interrupts */
    UART_REG_WRITE(UART0_BASE, UART0_ICR_OFFSET, 0x7FF);

    /* Configure 8-bit data, no parity, 1 stop bit, FIFOs enabled
     * LCRH bits: FEN(bit4)=1 FIFO, WLEN(bits6:5)=11 8-bit
     */
    UART_REG_WRITE(UART0_BASE, UART0_LCRH_OFFSET,
                   (1 << 4) | (3 << 5));

    /* Enable UART: UARTEN(bit0) + TXE(bit8) + RXE(bit9) */
    UART_REG_WRITE(UART0_BASE, UART0_CR_OFFSET,
                   (1 << 0) | (1 << 8) | (1 << 9));
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

    while (*fmt && (size_t)count < (size - 1)) {
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
                    count++;
                    i = -i;
                }
                /* Convert to string */
                {
                    char *end = p;
                    tmp = p;
                    do {
                        *p++ = '0' + (i % 10);
                        i /= 10;
                        count++;
                    } while (i > 0 && count < (int)(size - 1));
                    /* Reverse the digits in-place */
                    end = p;
                    while (tmp < p) {
                        char c = *tmp;
                        *tmp = *(p - 1);
                        *(p - 1) = c;
                        tmp++;
                        p--;
                    }
                    p = end;  /* Restore p to point past the digits */
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
                while (*s && (size_t)count < (size - 1)) {
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

/* Formatted print to UART (SMP-safe with the common CAS raw spinlock). */
static RawSpinlock_t uart_lock = {0};

static void uart_lock_acquire(void)
{
    vPortRawSpinLockAcquire(&uart_lock);
}

static void uart_lock_release(void)
{
    vPortRawSpinLockRelease(&uart_lock);
}

void uart_printf(const char *fmt, ...)
{
    char buf[256];
    va_list args;

    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    uart_lock_acquire();
    uart_puts(buf);
    uart_lock_release();
}

void uart_log(int level, const char *tag, const char *fmt, ...)
{
    char buf[224];
    va_list args;

    if (level < 0) {
        level = 0;
    } else if (level > 7) {
        level = 7;
    }
    if (tag == NULL) {
        tag = "freertos";
    }
    if (level > APP_LOG_LEVEL) {
        return;
    }

    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    uart_lock_acquire();
    uart_putc('<');
    uart_putc((char)('0' + level));
    uart_puts(">[    0.000000] ");
    uart_puts(tag);
    uart_puts(": ");
    uart_puts(buf);
    uart_lock_release();
}
