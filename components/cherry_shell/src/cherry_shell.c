/*
 * CherryShell - Main Subsystem & Interactive Shell Task
 *
 * Copyright (c) 2026 ForgeStitch
 * SPDX-License-Identifier: MIT
 */

#include "cherry_shell.h"
#include "cherry_kal.h"
#include "cherry_vfs.h"
#include "uart.h"
#include <string.h>

extern int cherry_shell_sym_init(void);

/* Forward declare external functions registered into shell symbol table */
extern void *kal_malloc(size_t size);
extern void  kal_free(void *ptr);
extern uint64_t kal_get_time_ms(void);
extern void uart_printf(const char *fmt, ...);
extern void uart_puts(const char *str);
extern int vfs_init(void);
extern int cherry_vfs_init(void);

static kal_thread_t g_shell_thread = NULL;

int cherry_shell_init(void)
{
    static int s_inited = 0;
    if (s_inited) {
        return CHERRY_SHELL_OK;
    }
    s_inited = 1;

    cherry_shell_sym_init();

    /* Batch register essential OSAL, VFS and System APIs for C-command execution & addr2sym */
    cherry_shell_register_symbol("kal_malloc", (uintptr_t)kal_malloc, CHERRY_SHELL_SYM_FUNC, "Allocate memory from heap: kal_malloc(size)");
    cherry_shell_register_symbol("kal_free", (uintptr_t)kal_free, CHERRY_SHELL_SYM_FUNC, "Free heap memory: kal_free(ptr)");
    cherry_shell_register_symbol("kal_get_time_ms", (uintptr_t)kal_get_time_ms, CHERRY_SHELL_SYM_FUNC, "Get uptime in ms: kal_get_time_ms()");
    cherry_shell_register_symbol("uart_printf", (uintptr_t)uart_printf, CHERRY_SHELL_SYM_FUNC, "Print formatted string to console");
    cherry_shell_register_symbol("vfs_init", (uintptr_t)vfs_init, CHERRY_SHELL_SYM_FUNC, "Initialize VFS core");
    cherry_shell_register_symbol("cherry_vfs_init", (uintptr_t)cherry_vfs_init, CHERRY_SHELL_SYM_FUNC, "Initialize complete CherryVFS subsystem");
    cherry_shell_register_symbol("cherry_shell_exec", (uintptr_t)cherry_shell_exec, CHERRY_SHELL_SYM_FUNC, "Execute shell command string");
    cherry_shell_register_symbol("cherry_shell_addr_to_sym", (uintptr_t)cherry_shell_addr_to_sym, CHERRY_SHELL_SYM_FUNC, "Resolve address to symbol");

    extern void _start(void);
    cherry_shell_register_symbol("_start", (uintptr_t)_start, CHERRY_SHELL_SYM_FUNC, "System boot entrypoint");

    kal_printf("<6>cherry_shell: initialized with VxWorks C-function call & addr2sym engine\n");
    return CHERRY_SHELL_OK;
}

static void cherry_shell_task(void *arg)
{
    (void)arg;
    char line[128];
    size_t pos = 0;

    kal_printf("\n");
    kal_printf("=================================================================\n");
    kal_printf("   _____ _                         _____ _          _ _          \n");
    kal_printf("  / ____| |                       / ____| |        | | |         \n");
    kal_printf(" | |    | |__   ___ _ __ _ __ _   | (___ | |__   ___| | |         \n");
    kal_printf(" | |    | '_ \\ / _ \\ '__| '__| | | \\___ \\| '_ \\ / _ \\ | |         \n");
    kal_printf(" | |____| | | |  __/ |  | |  | |_| |___) | | | |  __/ | |         \n");
    kal_printf("  \\_____|_| |_|\\___|_|  |_|   \\__, |_____/|_| |_|\\___|_|_|         \n");
    kal_printf("                               __/ |                             \n");
    kal_printf("                              |___/   VxWorks-style C Shell      \n");
    kal_printf("=================================================================\n");
    kal_printf("Type 'help' for commands, or invoke C functions directly, e.g. test_add(10, 20)\n\n");

    kal_printf("cherry> ");

    for (;;) {
        int c = uart_getc_nonblock();
        if (c < 0) {
            kal_thread_sleep_ms(10);
            continue;
        }

        if (c == '\r' || c == '\n') {
            uart_putc('\r');
            uart_putc('\n');
            line[pos] = '\0';
            if (pos > 0) {
                cherry_shell_exec(line, NULL, 0);
                pos = 0;
            }
            kal_printf("cherry> ");
        } else if (c == '\b' || c == 0x7F) {
            if (pos > 0) {
                pos--;
                uart_puts("\b \b");
            }
        } else if (c >= 32 && c <= 126) {
            if (pos < sizeof(line) - 1) {
                line[pos++] = (char)c;
                uart_putc((char)c);
            }
        }
    }
}

int cherry_shell_start(void)
{
    cherry_shell_init();

    if (g_shell_thread == NULL) {
        g_shell_thread = kal_thread_create("tShell", cherry_shell_task, NULL, 4096, 1);
        if (!g_shell_thread) {
            kal_printf("<3>cherry_shell: failed to create shell thread\n");
            return CHERRY_SHELL_ERR_NOMEM;
        }
        kal_thread_set_affinity(g_shell_thread, 1);
    }
    return CHERRY_SHELL_OK;
}
