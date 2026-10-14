/*
 * CherryShell - Built-in Shell Commands & Demo Functions
 *
 * Copyright (c) 2026 ForgeStitch
 * SPDX-License-Identifier: MIT
 */

#include "cherry_shell.h"
#include "cherry_kal.h"
#include "cherry_vfs.h"
#include "uart.h"
#include <string.h>

/* =========================================================================
 * 1. Built-in Commands
 * ========================================================================= */

static int help_sym_cb(const struct cherry_shell_symbol *sym, void *user_data)
{
    (void)user_data;
    const char *type_str = "[FUNC]";
    if (sym->type == CHERRY_SHELL_SYM_CMD) type_str = "[CMD] ";
    else if (sym->type == CHERRY_SHELL_SYM_VAR) type_str = "[VAR] ";

    kal_printf("  %-16s %s 0x%016lx %s\n",
               sym->name ? sym->name : "?",
               type_str,
               (unsigned long)sym->addr,
               sym->description ? sym->description : "");
    return 0;
}

static int cmd_help(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    kal_printf("\nCherryShell - VxWorks-style Dynamic Shell\n");
    kal_printf("Usage:\n");
    kal_printf("  - Command style:    <command> [arg1] [arg2] ...\n");
    kal_printf("  - C-function style: <func_name>([arg1], [arg2], ...)\n");
    kal_printf("  - Direct address:   0x<address>([arg1], [arg2], ...)\n");
    kal_printf("\nAvailable Commands and Functions:\n");
    kal_printf("  NAME             TYPE   ADDRESS            DESCRIPTION\n");
    kal_printf("  ---------------- ------ ------------------ ---------------------------\n");
    cherry_shell_for_each_symbol(help_sym_cb, NULL);
    kal_printf("\n");
    return 0;
}
CHERRY_SHELL_EXPORT_CMD(help, cmd_help, "Show this help menu");

static int syms_print_cb(const struct cherry_shell_symbol *sym, void *ud)
{
    const char *f = (const char *)ud;
    if (f && sym->name && strstr(sym->name, f) == NULL) {
        return 0;
    }
    const char *t = (sym->type == CHERRY_SHELL_SYM_CMD) ? "CMD " :
                    (sym->type == CHERRY_SHELL_SYM_VAR) ? "VAR " : "FUNC";
    kal_printf("  0x%016lx [%s] %s\n", (unsigned long)sym->addr, t, sym->name ? sym->name : "?");
    return 0;
}

static int cmd_syms(int argc, char **argv)
{
    const char *filter = (argc > 1) ? argv[1] : NULL;
    kal_printf("Registered Symbol Table:\n");
    kal_printf("  ADDRESS            TYPE   NAME\n");
    kal_printf("  ------------------ ------ --------------------------------\n");
    cherry_shell_for_each_symbol(syms_print_cb, (void *)filter);
    return 0;
}
CHERRY_SHELL_EXPORT_CMD(syms, cmd_syms, "List registered symbols [filter]");

static int cmd_addr2sym(int argc, char **argv)
{
    if (argc < 2) {
        kal_printf("Usage: addr2sym <hex_address>\n");
        return -1;
    }

    /* Parse hex address */
    const char *p = argv[1];
    if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) p += 2;

    uintptr_t addr = 0;
    while (*p) {
        char c = *p++;
        uintptr_t nibble;
        if (c >= '0' && c <= '9') nibble = c - '0';
        else if (c >= 'a' && c <= 'f') nibble = c - 'a' + 10;
        else if (c >= 'A' && c <= 'F') nibble = c - 'A' + 10;
        else {
            kal_printf("Invalid hex address: %s\n", argv[1]);
            return -1;
        }
        addr = (addr << 4) | nibble;
    }

    uintptr_t offset = 0;
    const struct cherry_shell_symbol *sym = cherry_shell_find_symbol_by_addr(addr, &offset);
    if (sym && sym->name) {
        if (offset == 0) {
            kal_printf("0x%016lx: %s\n", (unsigned long)addr, sym->name);
        } else {
            kal_printf("0x%016lx: %s + 0x%lx\n", (unsigned long)addr, sym->name, (unsigned long)offset);
        }
        return 0;
    }

    kal_printf("0x%016lx: [symbol not found]\n", (unsigned long)addr);
    return -1;
}
CHERRY_SHELL_EXPORT_CMD(addr2sym, cmd_addr2sym, "Resolve address to nearest symbol + offset");

static int cmd_ls(int argc, char **argv)
{
    const char *path = (argc > 1) ? argv[1] : "/";
    DIR *dir = opendir(path);
    if (!dir) {
        kal_printf("ls: cannot open directory '%s'\n", path);
        return -1;
    }

    kal_printf("Directory: %s\n", path);
    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL) {
        const char *type_name = "FILE";
        if (ent->d_type == DT_DIR)  type_name = "DIR ";
        else if (ent->d_type == DT_CHR)  type_name = "CHR ";
        else if (ent->d_type == DT_BLK)  type_name = "BLK ";
        else if (ent->d_type == DT_SOCK) type_name = "SOCK";

        kal_printf("  [%s]  ino=%-4lu  %s\n", type_name, (unsigned long)ent->d_ino, ent->d_name);
    }

    closedir(dir);
    return 0;
}
CHERRY_SHELL_EXPORT_CMD(ls, cmd_ls, "List files in directory [path]");

static int cmd_cat(int argc, char **argv)
{
    if (argc < 2) {
        kal_printf("Usage: cat <filename>\n");
        return -1;
    }

    int fd = open(argv[1], O_RDONLY);
    if (fd < 0) {
        kal_printf("cat: failed to open '%s'\n", argv[1]);
        return -1;
    }

    char buf[128];
    ssize_t n;
    while ((n = read(fd, buf, sizeof(buf) - 1)) > 0) {
        buf[n] = '\0';
        kal_printf("%s", buf);
    }
    kal_printf("\n");

    close(fd);
    return 0;
}
CHERRY_SHELL_EXPORT_CMD(cat, cmd_cat, "Display file contents <path>");

static int cmd_mkdir(int argc, char **argv)
{
    if (argc < 2) {
        kal_printf("Usage: mkdir <path>\n");
        return -1;
    }
    int ret = mkdir(argv[1], 0777);
    if (ret != 0) {
        kal_printf("mkdir: failed to create '%s' (ret=%d)\n", argv[1], ret);
        return ret;
    }
    kal_printf("Created directory: %s\n", argv[1]);
    return 0;
}
CHERRY_SHELL_EXPORT_CMD(mkdir, cmd_mkdir, "Create directory <path>");

static int cmd_touch(int argc, char **argv)
{
    if (argc < 2) {
        kal_printf("Usage: touch <path>\n");
        return -1;
    }
    int fd = open(argv[1], O_CREAT | O_WRONLY, 0666);
    if (fd < 0) {
        kal_printf("touch: failed to touch '%s'\n", argv[1]);
        return -1;
    }
    close(fd);
    kal_printf("Touched file: %s\n", argv[1]);
    return 0;
}
CHERRY_SHELL_EXPORT_CMD(touch, cmd_touch, "Create empty file <path>");

static int cmd_ps(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    uint32_t core = kal_get_core_id();
    uint64_t uptime_ms = kal_get_time_ms();
    kal_printf("System Info:\n");
    kal_printf("  Current Core: %u\n", (unsigned int)core);
    kal_printf("  Uptime:       %lu ms (%lu s)\n", (unsigned long)uptime_ms, (unsigned long)(uptime_ms / 1000));
    kal_printf("  Tick Rate:    %u Hz\n", (unsigned int)kal_tick_rate_hz());
    return 0;
}
CHERRY_SHELL_EXPORT_CMD(ps, cmd_ps, "Show current system tasks & core status");

static int cmd_echo(int argc, char **argv)
{
    for (int i = 1; i < argc; i++) {
        kal_printf("%s%s", argv[i], (i == argc - 1) ? "" : " ");
    }
    kal_printf("\n");
    return 0;
}
CHERRY_SHELL_EXPORT_CMD(echo, cmd_echo, "Echo text to console");

extern int vfs_run_stress_tests(int file_count);

static int cmd_vfs_stress(int argc, char **argv)
{
    int count = 50;
    if (argc > 1) {
        const char *p = argv[1];
        int val = 0;
        while (*p >= '0' && *p <= '9') {
            val = val * 10 + (*p++ - '0');
        }
        if (val > 0) count = val;
    }
    return vfs_run_stress_tests(count);
}
CHERRY_SHELL_EXPORT_CMD(vfs_stress, cmd_vfs_stress, "Run massive file operations stress test: vfs_stress [count]");

uintptr_t test_vfs_stress(uintptr_t count)
{
    return (uintptr_t)vfs_run_stress_tests((int)count);
}
CHERRY_SHELL_EXPORT_SYM(test_vfs_stress, test_vfs_stress, "Run VFS stress test as C function: test_vfs_stress(count)");

/* =========================================================================
 * 2. Callable C Functions (VxWorks-style invocation & acceptance tests)
 * ========================================================================= */

uintptr_t test_add(uintptr_t a, uintptr_t b)
{
    return a + b;
}
CHERRY_SHELL_EXPORT_SYM(test_add, test_add, "Add two numbers: test_add(a, b)");

uintptr_t test_sub(uintptr_t a, uintptr_t b)
{
    return a - b;
}
CHERRY_SHELL_EXPORT_SYM(test_sub, test_sub, "Subtract numbers: test_sub(a, b)");

uintptr_t test_mul(uintptr_t a, uintptr_t b)
{
    return a * b;
}
CHERRY_SHELL_EXPORT_SYM(test_mul, test_mul, "Multiply numbers: test_mul(a, b)");

uintptr_t test_sum8(uintptr_t a, uintptr_t b, uintptr_t c, uintptr_t d,
                    uintptr_t e, uintptr_t f, uintptr_t g, uintptr_t h)
{
    return a + b + c + d + e + f + g + h;
}
CHERRY_SHELL_EXPORT_SYM(test_sum8, test_sum8, "Sum 8 arguments: test_sum8(a..h)");

uintptr_t test_str_len(const char *str)
{
    if (!str) return 0;
    return (uintptr_t)strlen(str);
}
CHERRY_SHELL_EXPORT_SYM(test_str_len, test_str_len, "String length: test_str_len(str)");

uintptr_t dump_mem(uintptr_t addr, uintptr_t len)
{
    const uint8_t *p = (const uint8_t *)addr;
    if (!p) return 0;

    kal_printf("Memory dump at 0x%016lx (%lu bytes):\n", (unsigned long)addr, (unsigned long)len);
    for (uintptr_t i = 0; i < len; i += 16) {
        kal_printf("  0x%08lx: ", (unsigned long)(addr + i));
        for (uintptr_t j = 0; j < 16; j++) {
            if (i + j < len) {
                kal_printf("%02x ", p[i + j]);
            } else {
                kal_printf("   ");
            }
        }
        kal_printf(" |");
        for (uintptr_t j = 0; j < 16 && (i + j < len); j++) {
            char c = (char)p[i + j];
            kal_printf("%c", (c >= 32 && c <= 126) ? c : '.');
        }
        kal_printf("|\n");
    }
    return len;
}
CHERRY_SHELL_EXPORT_SYM(dump_mem, dump_mem, "Hex dump memory: dump_mem(addr, len)");
