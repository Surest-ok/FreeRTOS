/* Simple test program for UART */
#include "platform.h"
#include "uart.h"

int main(void)
{
    uart_init();
    uart_puts("Hello from FreeRTOS SMP Phase 1!\r\n");
    uart_printf("Core ID: %d\r\n", 0);
    while(1);
    return 0;
}