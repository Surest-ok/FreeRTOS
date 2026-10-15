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

/* Read a single character from UART (non-blocking). Returns -1 if no character available. */
int uart_getc_nonblock(void)
{
    if (UART_REG_READ(UART0_BASE, UART0_FR_OFFSET) & UART_FR_RXFE) {
        return -1;
    }
    return (int)(UART_REG_READ(UART0_BASE, UART0_DR_OFFSET) & 0xFF);
}

/* Read a single character from UART (blocking) */
char uart_getc(void)
{
    while (UART_REG_READ(UART0_BASE, UART0_FR_OFFSET) & UART_FR_RXFE) {
        /* Wait */
    }
    return (char)(UART_REG_READ(UART0_BASE, UART0_DR_OFFSET) & 0xFF);
}

/* Formatted printing implementation for freestanding environment */
int vsnprintf(char *buf, size_t size, const char *fmt, va_list args)
{
    if (!buf || size == 0) return 0;

    int count = 0;
    char *p = buf;

    while (*fmt && (size_t)count < (size - 1)) {
        if (*fmt != '%') {
            *p++ = *fmt++;
            count++;
            continue;
        }

        fmt++;
        int pad_zero = 0;
        int left_align = 0;
        int width = 0;
        while (*fmt == '-' || *fmt == '0') {
            if (*fmt == '-') {
                left_align = 1;
            } else if (*fmt == '0') {
                pad_zero = 1;
            }
            fmt++;
        }
        while (*fmt >= '0' && *fmt <= '9') {
            width = width * 10 + (*fmt - '0');
            fmt++;
        }

        int is_long = 0;
        if (*fmt == 'l') {
            is_long = 1;
            fmt++;
            if (*fmt == 'l') {
                is_long = 2;
                fmt++;
            }
        }

        switch (*fmt) {
            case 'd':
            case 'i': {
                long long val;
                if (is_long == 2) val = va_arg(args, long long);
                else if (is_long == 1) val = va_arg(args, long);
                else val = va_arg(args, int);

                int is_neg = 0;
                if (val < 0) {
                    is_neg = 1;
                    val = -val;
                }
                char num_buf[32];
                int nlen = 0;
                unsigned long long uval = (unsigned long long)val;
                do {
                    num_buf[nlen++] = '0' + (uval % 10);
                    uval /= 10;
                } while (uval > 0);

                int total_len = nlen + (is_neg ? 1 : 0);
                if (!left_align) {
                    if (pad_zero && is_neg) {
                        if ((size_t)count < (size - 1)) {
                            *p++ = '-';
                            count++;
                            is_neg = 0;
                        }
                    }
                    while (total_len < width && (size_t)count < (size - 1)) {
                        *p++ = pad_zero ? '0' : ' ';
                        count++;
                        width--;
                    }
                }
                if (is_neg && (size_t)count < (size - 1)) {
                    *p++ = '-';
                    count++;
                }
                for (int j = nlen - 1; j >= 0 && (size_t)count < (size - 1); j--) {
                    *p++ = num_buf[j];
                    count++;
                }
                if (left_align) {
                    while (total_len < width && (size_t)count < (size - 1)) {
                        *p++ = ' ';
                        count++;
                        width--;
                    }
                }
                break;
            }

            case 'u': {
                unsigned long long uval;
                if (is_long == 2) uval = va_arg(args, unsigned long long);
                else if (is_long == 1) uval = va_arg(args, unsigned long);
                else uval = va_arg(args, unsigned int);

                char num_buf[32];
                int nlen = 0;
                do {
                    num_buf[nlen++] = '0' + (uval % 10);
                    uval /= 10;
                } while (uval > 0);

                if (!left_align) {
                    while (nlen < width && (size_t)count < (size - 1)) {
                        *p++ = pad_zero ? '0' : ' ';
                        count++;
                        width--;
                    }
                }
                for (int j = nlen - 1; j >= 0 && (size_t)count < (size - 1); j--) {
                    *p++ = num_buf[j];
                    count++;
                }
                if (left_align) {
                    while (nlen < width && (size_t)count < (size - 1)) {
                        *p++ = ' ';
                        count++;
                        width--;
                    }
                }
                break;
            }

            case 'x':
            case 'X':
            case 'p': {
                unsigned long long uval;
                if (*fmt == 'p') {
                    uval = (uintptr_t)va_arg(args, void *);
                    if (width == 0) width = 16;
                    pad_zero = 1;
                } else if (is_long == 2) {
                    uval = va_arg(args, unsigned long long);
                } else if (is_long == 1) {
                    uval = va_arg(args, unsigned long);
                } else {
                    uval = va_arg(args, unsigned int);
                }

                char num_buf[32];
                int nlen = 0;
                const char *hex_digits = (*fmt == 'X') ? "0123456789ABCDEF" : "0123456789abcdef";
                do {
                    num_buf[nlen++] = hex_digits[uval & 0xF];
                    uval >>= 4;
                } while (uval > 0);

                if (!left_align) {
                    while (nlen < width && (size_t)count < (size - 1)) {
                        *p++ = pad_zero ? '0' : ' ';
                        count++;
                        width--;
                    }
                }
                for (int j = nlen - 1; j >= 0 && (size_t)count < (size - 1); j--) {
                    *p++ = num_buf[j];
                    count++;
                }
                if (left_align) {
                    while (nlen < width && (size_t)count < (size - 1)) {
                        *p++ = ' ';
                        count++;
                        width--;
                    }
                }
                break;
            }

            case 's': {
                const char *s = va_arg(args, char *);
                if (!s) s = "(null)";
                int slen = 0;
                while (s[slen]) slen++;

                if (!left_align) {
                    while (slen < width && (size_t)count < (size - 1)) {
                        *p++ = ' ';
                        count++;
                        width--;
                    }
                }
                while (*s && (size_t)count < (size - 1)) {
                    *p++ = *s++;
                    count++;
                }
                if (left_align) {
                    while (slen < width && (size_t)count < (size - 1)) {
                        *p++ = ' ';
                        count++;
                        width--;
                    }
                }
                break;
            }

            case 'c': {
                *p++ = (char)va_arg(args, int);
                count++;
                break;
            }

            case '%': {
                *p++ = '%';
                count++;
                break;
            }

            default:
                *p++ = *fmt;
                count++;
                break;
        }

        fmt++;
    }

    *p = '\0';
    return count;
}

int snprintf(char *buf, size_t size, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int ret = vsnprintf(buf, size, fmt, ap);
    va_end(ap);
    return ret;
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

extern int printk(const char *fmt, ...);

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
    static const char * const lvl_prefix[8] = {
        "<0>", "<1>", "<2>", "<3>", "<4>", "<5>", "<6>", "<7>"
    };
    char full_fmt[64];
    snprintf(full_fmt, sizeof(full_fmt), "%s%s: %%s", lvl_prefix[level], tag);
    printk(full_fmt, buf);
}
