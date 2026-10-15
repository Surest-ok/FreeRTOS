/*
 * CherryKmod - Shell Commands (insmod, rmmod, lsmod)
 *
 * Copyright (c) 2026 ForgeStitch
 * SPDX-License-Identifier: MIT
 */

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include "cherry_kmod.h"
#include "cherry_shell.h"

extern void printk(const char *fmt, ...);
extern int uart_printf(const char *fmt, ...);

int cherry_kmod_cmd_insmod(int argc, char **argv)
{
    if (argc < 2) {
        uart_printf("Usage: insmod <filename.ko>\n");
        return -1;
    }

    const char *path = argv[1];
    struct cherry_kmod *mod = cherry_kmod_load_file(path);
    if (!mod) {
        uart_printf("insmod: failed to load '%s'\n", path);
        return -1;
    }

    uart_printf("insmod: loaded '%s'\n", mod->name);
    return 0;
}

int cherry_kmod_cmd_rmmod(int argc, char **argv)
{
    if (argc < 2) {
        uart_printf("Usage: rmmod <module_name>\n");
        return -1;
    }

    const char *name = argv[1];
    int ret = cherry_kmod_unload(name);
    if (ret != 0) {
        uart_printf("rmmod: failed to unload '%s' (err=%d)\n", name, ret);
        return -1;
    }

    uart_printf("rmmod: '%s' unloaded successfully\n", name);
    return 0;
}

int cherry_kmod_cmd_lsmod(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    char buf[1024];
    int len = cherry_kmod_list(buf, sizeof(buf));
    if (len > 0) {
        uart_printf("%s", buf);
    } else {
        uart_printf("lsmod: no modules loaded\n");
    }
    return 0;
}

CHERRY_SHELL_EXPORT_CMD(insmod, cherry_kmod_cmd_insmod, "load a kernel module: insmod <file.ko>");
CHERRY_SHELL_EXPORT_CMD(rmmod, cherry_kmod_cmd_rmmod, "unload a kernel module: rmmod <mod_name>");
CHERRY_SHELL_EXPORT_CMD(lsmod, cherry_kmod_cmd_lsmod, "list loaded kernel modules: lsmod");

CHERRY_SHELL_EXPORT_CMD(cherry_insmod, cherry_kmod_cmd_insmod, "alias to insmod");
CHERRY_SHELL_EXPORT_CMD(cherry_rmmod, cherry_kmod_cmd_rmmod, "alias to rmmod");
CHERRY_SHELL_EXPORT_CMD(cherry_lsmod, cherry_kmod_cmd_lsmod, "alias to lsmod");
