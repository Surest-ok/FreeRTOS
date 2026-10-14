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
extern void uart_printf(const char *fmt, ...);
extern void uart_puts(const char *str);
extern int vfs_init(void);
extern int cherry_vfs_init(void);

static struct task_struct *g_shell_thread = NULL;

int cherry_shell_init(void)
{
    static int s_inited = 0;
    if (s_inited) {
        return CHERRY_SHELL_OK;
    }
    s_inited = 1;

    cherry_shell_sym_init();

    /* Batch register essential Linux OSAL, VFS and System APIs for C-command execution & addr2sym */
    cherry_shell_register_symbol("kmalloc", (uintptr_t)kmalloc, CHERRY_SHELL_SYM_FUNC, "Allocate memory from heap: kmalloc(size, flags)");
    cherry_shell_register_symbol("kfree", (uintptr_t)kfree, CHERRY_SHELL_SYM_FUNC, "Free heap memory: kfree(ptr)");
    cherry_shell_register_symbol("ktime_get_ms", (uintptr_t)ktime_get_ms, CHERRY_SHELL_SYM_FUNC, "Get uptime in ms: ktime_get_ms()");
    cherry_shell_register_symbol("uart_printf", (uintptr_t)uart_printf, CHERRY_SHELL_SYM_FUNC, "Print formatted string to console");
    cherry_shell_register_symbol("vfs_init", (uintptr_t)vfs_init, CHERRY_SHELL_SYM_FUNC, "Initialize VFS core");
    cherry_shell_register_symbol("cherry_vfs_init", (uintptr_t)cherry_vfs_init, CHERRY_SHELL_SYM_FUNC, "Initialize complete CherryVFS subsystem");
    cherry_shell_register_symbol("cherry_shell_exec", (uintptr_t)cherry_shell_exec, CHERRY_SHELL_SYM_FUNC, "Execute shell command string");
    cherry_shell_register_symbol("cherry_shell_addr_to_sym", (uintptr_t)cherry_shell_addr_to_sym, CHERRY_SHELL_SYM_FUNC, "Resolve address to symbol");

    extern void _start(void);
    cherry_shell_register_symbol("_start", (uintptr_t)_start, CHERRY_SHELL_SYM_FUNC, "System boot entrypoint");

    pr_info("cherry_shell: initialized with VxWorks C-function call & addr2sym engine\n");
    return CHERRY_SHELL_OK;
}

static void shell_puts(int fd, const char *str)
{
    if (!str) return;
    if (fd >= 0) {
        write(fd, str, strlen(str));
    } else {
        uart_puts(str);
    }
}

static void shell_putc(int fd, char c)
{
    if (fd >= 0) {
        write(fd, &c, 1);
    } else {
        uart_putc(c);
    }
}

static int cherry_shell_task(void *arg)
{
    (void)arg;
    char line[128];
    size_t pos = 0;

    int tty_fd = open("/dev/console", O_RDWR);
    if (tty_fd < 0) {
        tty_fd = open("/dev/ttyS0", O_RDWR);
    }

    shell_puts(tty_fd, "\n");
    shell_puts(tty_fd, "=================================================================\n");
    shell_puts(tty_fd, "   _____ _                         _____ _          _ _          \n");
    shell_puts(tty_fd, "  / ____| |                       / ____| |        | | |         \n");
    shell_puts(tty_fd, " | |    | |__   ___ _ __ _ __ _   | (___ | |__   ___| | |         \n");
    shell_puts(tty_fd, " | |    | '_ \\ / _ \\ '__| '__| | | \\___ \\| '_ \\ / _ \\ | |         \n");
    shell_puts(tty_fd, " | |____| | | |  __/ |  | |  | |_| |___) | | | |  __/ | |         \n");
    shell_puts(tty_fd, "  \\_____|_| |_|\\___|_|  |_|   \\__, |_____/|_| |_|\\___|_|_|         \n");
    shell_puts(tty_fd, "                               __/ |                             \n");
    shell_puts(tty_fd, "                              |___/   VxWorks-style C Shell      \n");
    shell_puts(tty_fd, "=================================================================\n");
    shell_puts(tty_fd, "Terminal: /dev/console (PL011 Character Device via CherryVFS)\n");
    shell_puts(tty_fd, "Type 'help' for commands, or invoke C functions directly, e.g. test_add(10, 20)\n\n");

    shell_puts(tty_fd, "cherry> ");

    for (;;) {
        char c = 0;
        ssize_t n = 0;
        if (tty_fd >= 0) {
            n = read(tty_fd, &c, 1);
        } else {
            int ch = uart_getc_nonblock();
            if (ch >= 0) {
                c = (char)ch;
                n = 1;
            }
        }

        if (n <= 0) {
            msleep(10);
            continue;
        }

        if (c == '\r' || c == '\n') {
            shell_puts(tty_fd, "\r\n");
            line[pos] = '\0';
            if (pos > 0) {
                cherry_shell_exec(line, NULL, 0);
                pos = 0;
            }
            shell_puts(tty_fd, "cherry> ");
        } else if (c == '\b' || c == 0x7F) {
            if (pos > 0) {
                pos--;
                shell_puts(tty_fd, "\b \b");
            }
        } else if (c >= 32 && c <= 126) {
            if (pos < sizeof(line) - 1) {
                line[pos++] = c;
                shell_putc(tty_fd, c);
            }
        }
    }
    return 0;
}

int cherry_shell_start(void)
{
    cherry_shell_init();

    if (g_shell_thread == NULL) {
        g_shell_thread = kthread_run((kthread_fn_t)cherry_shell_task, NULL, "tShell");
        if (!g_shell_thread) {
            pr_err("cherry_shell: failed to create shell thread\n");
            return CHERRY_SHELL_ERR_NOMEM;
        }
        kthread_bind(g_shell_thread, 0);
    }
    return CHERRY_SHELL_OK;
}
