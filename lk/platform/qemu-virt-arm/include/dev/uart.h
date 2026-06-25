/*
 * Generic UART interface
 */
#pragma once

#include <lk/compiler.h>
#include <sys/types.h>
#include <stdbool.h>

__BEGIN_CDECLS

int uart_putc(int port, char c);
int uart_getc(int port, bool wait);
void uart_flush_tx(int port);
void uart_flush_rx(int port);
void uart_init_port(int port, uint baud);

/* Platform-specific functions */
int uart_pputc(int port, char c);
int uart_pgetc(int port);

__END_CDECLS
