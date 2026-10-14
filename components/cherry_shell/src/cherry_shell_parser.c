/*
 * CherryShell - VxWorks-style Command & C-Function Expression Parser
 *
 * Copyright (c) 2026 ForgeStitch
 * SPDX-License-Identifier: MIT
 */

#include "cherry_shell.h"
#include "cherry_kal.h"
#include "uart.h"
#include <string.h>

#define CHERRY_SHELL_MAX_ARGS 8
#define CHERRY_SHELL_ARG_LEN  128

/* AArch64 AAPCS64 direct invocation */
uintptr_t cherry_shell_call(void *func_ptr, int argc, uintptr_t argv[8])
{
    (void)argc;
    cherry_shell_fn8 fn = (cherry_shell_fn8)func_ptr;
    return fn(argv[0], argv[1], argv[2], argv[3],
              argv[4], argv[5], argv[6], argv[7]);
}

/* Helper to strip leading and trailing whitespace */
static char *trim_whitespace(char *str)
{
    if (!str) return NULL;
    while (*str == ' ' || *str == '\t' || *str == '\r' || *str == '\n') {
        str++;
    }
    if (*str == '\0') return str;

    char *end = str + strlen(str) - 1;
    while (end > str && (*end == ' ' || *end == '\t' || *end == '\r' || *end == '\n')) {
        *end-- = '\0';
    }
    return str;
}

/* Helper to parse hexadecimal or decimal integer */
static bool parse_number(const char *str, uintptr_t *out_val)
{
    if (!str || !*str) return false;

    bool negative = false;
    if (*str == '-') {
        negative = true;
        str++;
    }

    if (str[0] == '0' && (str[1] == 'x' || str[1] == 'X')) {
        /* Hexadecimal */
        str += 2;
        if (!*str) return false;
        uintptr_t val = 0;
        while (*str) {
            char c = *str++;
            uintptr_t nibble;
            if (c >= '0' && c <= '9') nibble = c - '0';
            else if (c >= 'a' && c <= 'f') nibble = c - 'a' + 10;
            else if (c >= 'A' && c <= 'F') nibble = c - 'A' + 10;
            else return false;
            val = (val << 4) | nibble;
        }
        *out_val = negative ? (uintptr_t)(-(intptr_t)val) : val;
        return true;
    }

    /* Decimal */
    const char *p = str;
    while (*p) {
        if (*p < '0' || *p > '9') return false;
        p++;
    }

    uintptr_t val = 0;
    p = str;
    while (*p) {
        val = val * 10 + (*p++ - '0');
    }
    *out_val = negative ? (uintptr_t)(-(intptr_t)val) : val;
    return true;
}

/* Parse a single raw argument string into uintptr_t value */
static uintptr_t evaluate_arg(char *arg, char parsed_strings[][CHERRY_SHELL_ARG_LEN], int *str_idx)
{
    arg = trim_whitespace(arg);
    size_t len = strlen(arg);
    if (len == 0) return 0;

    /* 1. Quoted string: "hello" or 'hello' */
    if ((arg[0] == '"' && arg[len - 1] == '"') || (arg[0] == '\'' && arg[len - 1] == '\'')) {
        arg[len - 1] = '\0';
        char *str_body = arg + 1;
        if (*str_idx < CHERRY_SHELL_MAX_ARGS) {
            strncpy(parsed_strings[*str_idx], str_body, CHERRY_SHELL_ARG_LEN - 1);
            parsed_strings[*str_idx][CHERRY_SHELL_ARG_LEN - 1] = '\0';
            char *ret_ptr = parsed_strings[*str_idx];
            (*str_idx)++;
            return (uintptr_t)ret_ptr;
        }
        return (uintptr_t)str_body;
    }

    /* 2. Numeric literal */
    uintptr_t num_val = 0;
    if (parse_number(arg, &num_val)) {
        return num_val;
    }

    /* 3. Symbol identifier lookup */
    uintptr_t sym_addr = cherry_shell_sym_to_addr(arg);
    if (sym_addr != 0) {
        return sym_addr;
    }

    /* 4. Fallback: treat unquoted string as string pointer */
    if (*str_idx < CHERRY_SHELL_MAX_ARGS) {
        strncpy(parsed_strings[*str_idx], arg, CHERRY_SHELL_ARG_LEN - 1);
        parsed_strings[*str_idx][CHERRY_SHELL_ARG_LEN - 1] = '\0';
        char *ret_ptr = parsed_strings[*str_idx];
        (*str_idx)++;
        return (uintptr_t)ret_ptr;
    }

    return (uintptr_t)arg;
}

int cherry_shell_exec(const char *cmdline, char *out_buf, size_t out_buf_len)
{
    if (!cmdline) return CHERRY_SHELL_ERR_INVAL;

    char line_buf[256];
    strncpy(line_buf, cmdline, sizeof(line_buf) - 1);
    line_buf[sizeof(line_buf) - 1] = '\0';

    char *cmd = trim_whitespace(line_buf);
    if (*cmd == '\0' || *cmd == '#') {
        return CHERRY_SHELL_OK; /* Empty or comment */
    }

    uintptr_t args[CHERRY_SHELL_MAX_ARGS] = {0};
    char *cmd_argv[CHERRY_SHELL_MAX_ARGS + 1] = {0};
    char raw_argv_strings[CHERRY_SHELL_MAX_ARGS][CHERRY_SHELL_ARG_LEN] = {{0}};
    char parsed_strings[CHERRY_SHELL_MAX_ARGS][CHERRY_SHELL_ARG_LEN] = {{0}};
    int str_idx = 0;
    int argc = 0;

    char cmd_name[64] = {0};
    bool is_c_func_syntax = false;

    char *open_paren = strchr(cmd, '(');
    if (open_paren != NULL) {
        /* C function syntax: func_name(arg1, arg2, ...) */
        is_c_func_syntax = true;
        size_t name_len = (size_t)(open_paren - cmd);
        if (name_len >= sizeof(cmd_name)) {
            name_len = sizeof(cmd_name) - 1;
        }
        memcpy(cmd_name, cmd, name_len);
        cmd_name[name_len] = '\0';
        trim_whitespace(cmd_name);

        char *args_str = open_paren + 1;
        char *close_paren = strrchr(args_str, ')');
        if (close_paren) {
            *close_paren = '\0';
        }

        /* Tokenize arguments by comma, respecting quotes */
        char *p = args_str;
        char *token_start = p;
        bool in_dquote = false;
        bool in_squote = false;

        while (*p && argc < CHERRY_SHELL_MAX_ARGS) {
            if (*p == '"' && !in_squote) {
                in_dquote = !in_dquote;
            } else if (*p == '\'' && !in_dquote) {
                in_squote = !in_squote;
            } else if (*p == ',' && !in_dquote && !in_squote) {
                *p = '\0';
                char *token = trim_whitespace(token_start);
                if (*token) {
                    size_t tlen = strlen(token);
                    const char *argv_str = token;
                    if ((token[0] == '"' && token[tlen - 1] == '"') || (token[0] == '\'' && token[tlen - 1] == '\'')) {
                        token[tlen - 1] = '\0';
                        argv_str = token + 1;
                    }
                    strncpy(raw_argv_strings[argc], argv_str, CHERRY_SHELL_ARG_LEN - 1);
                    cmd_argv[argc + 1] = raw_argv_strings[argc];
                    args[argc] = evaluate_arg((char *)argv_str, parsed_strings, &str_idx);
                    argc++;
                }
                token_start = p + 1;
            }
            p++;
        }
        if (argc < CHERRY_SHELL_MAX_ARGS) {
            char *token = trim_whitespace(token_start);
            if (*token) {
                size_t tlen = strlen(token);
                const char *argv_str = token;
                if ((token[0] == '"' && token[tlen - 1] == '"') || (token[0] == '\'' && token[tlen - 1] == '\'')) {
                    token[tlen - 1] = '\0';
                    argv_str = token + 1;
                }
                strncpy(raw_argv_strings[argc], argv_str, CHERRY_SHELL_ARG_LEN - 1);
                cmd_argv[argc + 1] = raw_argv_strings[argc];
                args[argc] = evaluate_arg((char *)argv_str, parsed_strings, &str_idx);
                argc++;
            }
        }
    } else {
        /* Shell command syntax: cmd arg1 arg2 ... */
        char *p = cmd;
        while (*p && *p != ' ' && *p != '\t') p++;
        size_t name_len = (size_t)(p - cmd);
        if (name_len >= sizeof(cmd_name)) {
            name_len = sizeof(cmd_name) - 1;
        }
        memcpy(cmd_name, cmd, name_len);
        cmd_name[name_len] = '\0';

        char *token_start = p;
        bool in_dquote = false;
        bool in_squote = false;

        while (*p && argc < CHERRY_SHELL_MAX_ARGS) {
            if (*p == '"' && !in_squote) {
                in_dquote = !in_dquote;
            } else if (*p == '\'' && !in_dquote) {
                in_squote = !in_squote;
            } else if ((*p == ' ' || *p == '\t') && !in_dquote && !in_squote) {
                *p = '\0';
                char *token = trim_whitespace(token_start);
                if (*token) {
                    size_t tlen = strlen(token);
                    const char *argv_str = token;
                    if ((token[0] == '"' && token[tlen - 1] == '"') || (token[0] == '\'' && token[tlen - 1] == '\'')) {
                        token[tlen - 1] = '\0';
                        argv_str = token + 1;
                    }
                    strncpy(raw_argv_strings[argc], argv_str, CHERRY_SHELL_ARG_LEN - 1);
                    cmd_argv[argc + 1] = raw_argv_strings[argc];
                    args[argc] = evaluate_arg((char *)argv_str, parsed_strings, &str_idx);
                    argc++;
                }
                token_start = p + 1;
            }
            p++;
        }
        if (argc < CHERRY_SHELL_MAX_ARGS) {
            char *token = trim_whitespace(token_start);
            if (*token) {
                size_t tlen = strlen(token);
                const char *argv_str = token;
                if ((token[0] == '"' && token[tlen - 1] == '"') || (token[0] == '\'' && token[tlen - 1] == '\'')) {
                    token[tlen - 1] = '\0';
                    argv_str = token + 1;
                }
                strncpy(raw_argv_strings[argc], argv_str, CHERRY_SHELL_ARG_LEN - 1);
                cmd_argv[argc + 1] = raw_argv_strings[argc];
                args[argc] = evaluate_arg((char *)argv_str, parsed_strings, &str_idx);
                argc++;
            }
        }
    }

    cmd_argv[0] = cmd_name;

    /* Symbol lookup */
    const struct cherry_shell_symbol *sym = cherry_shell_find_symbol_by_name(cmd_name);
    uintptr_t func_addr = 0;
    uint32_t sym_type = CHERRY_SHELL_SYM_FUNC;

    if (sym != NULL) {
        func_addr = sym->addr;
        sym_type = sym->type;
    } else {
        /* Check if command name is a raw hex address: e.g. 0x40001234 */
        uintptr_t raw_addr = 0;
        if (parse_number(cmd_name, &raw_addr) && raw_addr != 0) {
            func_addr = raw_addr;
            sym_type = CHERRY_SHELL_SYM_FUNC;
        }
    }

    if (func_addr == 0) {
        kal_printf("%s: command or symbol not found\n", cmd_name);
        if (out_buf && out_buf_len > 0) {
            snprintf(out_buf, out_buf_len, "%s: not found", cmd_name);
        }
        return CHERRY_SHELL_ERR_NOTFOUND;
    }

    /* Execution dispatch */
    if (sym_type == CHERRY_SHELL_SYM_CMD && !is_c_func_syntax) {
        /* Standard shell command: int (*)(int argc, char **argv) */
        cherry_shell_cmd_fn cmd_fn = (cherry_shell_cmd_fn)func_addr;
        int ret = cmd_fn(argc + 1, cmd_argv);
        if (out_buf && out_buf_len > 0) {
            snprintf(out_buf, out_buf_len, "%d", ret);
        }
        return ret;
    }

    if (sym_type == CHERRY_SHELL_SYM_VAR) {
        /* Inspect variable value */
        uintptr_t val = *(volatile uintptr_t *)func_addr;
        kal_printf("%s = 0x%016lx (*%s = 0x%016lx = %ld)\n",
                   cmd_name, (unsigned long)func_addr, cmd_name, (unsigned long)val, (long)val);
        if (out_buf && out_buf_len > 0) {
            snprintf(out_buf, out_buf_len, "value = %ld = 0x%lx", (long)val, (unsigned long)val);
        }
        return CHERRY_SHELL_OK;
    }

    /* C Function execution (VxWorks style) */
    uintptr_t ret_val = cherry_shell_call((void *)func_addr, argc, args);

    /* Print VxWorks standard format: value = <dec> = 0x<hex> */
    kal_printf("value = %ld = 0x%lx\n", (long)ret_val, (unsigned long)ret_val);

    if (out_buf && out_buf_len > 0) {
        snprintf(out_buf, out_buf_len, "value = %ld = 0x%lx", (long)ret_val, (unsigned long)ret_val);
    }

    return CHERRY_SHELL_OK;
}
