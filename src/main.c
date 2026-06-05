/*
 * Phase 3: FreeRTOS V11.x on QEMU virt AArch64 (GICv3)
 *
 * Demo: two tasks printing at different rates via preemptive scheduling.
 */

#include <stdint.h>
#include "platform.h"
#include "uart.h"
#include "gic.h"
#include "gtimer.h"
#include "FreeRTOS.h"
#include "task.h"

/* Demo Task A: prints every 1 second */
static void task_print_a(void *pvParameters)
{
    (void)pvParameters;
    int count = 0;
    for (;;) {
        uart_printf("[TaskA] count=%d, tick=%d\r\n",
                     count, (uint32_t)xTaskGetTickCount());
        count++;
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

/* Demo Task B: prints every 700ms */
static void task_print_b(void *pvParameters)
{
    (void)pvParameters;
    int count = 0;
    for (;;) {
        uart_printf("[TaskB] count=%d, tick=%d\r\n",
                     count, (uint32_t)xTaskGetTickCount());
        count++;
        vTaskDelay(pdMS_TO_TICKS(700));
    }
}

int main(void)
{
    uint64_t el;
    asm volatile("mrs %0, CurrentEL" : "=r"(el));
    el = (el >> 2) & 3;

    uart_init();
    uart_printf("=== Phase 3: FreeRTOS V11.x on EL%d ===\r\n", (uint32_t)el);

    /* Initialize GICv3 (Distributor + Redistributor + CPU Interface) */
    gic_init(0);
    uart_puts("[1] GICv3 initialized\r\n");

    /* Initialize timer (compute step, but don't start yet) */
    gtimer_init();

    /* Note: Vector table is NOT installed here.
     * FreeRTOS installs _freertos_vector_table via VBAR_EL1
     * when vTaskStartScheduler() calls vPortRestoreTaskContext(). */

    /* Create demo tasks */
    xTaskCreate(task_print_a, "TaskA", 256, NULL, 2, NULL);
    xTaskCreate(task_print_b, "TaskB", 256, NULL, 2, NULL);
    uart_puts("[2] Tasks created\r\n");

    /* Start the scheduler — never returns */
    uart_puts("[3] Starting scheduler...\r\n");
    vTaskStartScheduler();

    /* Should never reach here */
    uart_puts("ERROR: Scheduler exited!\r\n");
    while (1) asm volatile("wfi");
    return 0;
}

/* ---- FreeRTOS Hook Functions ---- */

void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    (void)xTask;
    uart_printf("STACK OVERFLOW in: %s\r\n", pcTaskName);
    for (;;) asm volatile("wfi");
}

void vApplicationMallocFailedHook(void)
{
    uart_puts("MALLOC FAILED\r\n");
    for (;;) asm volatile("wfi");
}

void vAssertCalled(const char *file, int line)
{
    uart_printf("ASSERT FAIL: %s:%d\r\n", file, line);
    for (;;) asm volatile("wfi");
}
