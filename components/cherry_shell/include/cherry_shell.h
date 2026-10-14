/*
 * CherryShell - VxWorks-style Dynamic Shell for Embedded & Real-Time OS
 *
 * Copyright (c) 2026 ForgeStitch
 * SPDX-License-Identifier: MIT
 *
 * Features:
 *  1. C Functions as Commands (execute functions with arguments & print return value)
 *  2. Address-to-Symbol (addr2sym) Resolution & Stack Symbolizer
 *  3. Compile-time Section Exports & Dynamic Symbol Table
 *  4. POSIX VFS & OS Abstraction via CherryKAL
 */

#ifndef _CHERRY_SHELL_H_
#define _CHERRY_SHELL_H_

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Symbol types */
#define CHERRY_SHELL_SYM_FUNC   0x01U  /* Callable C function */
#define CHERRY_SHELL_SYM_CMD    0x02U  /* Shell command: int (*)(int argc, char **argv) */
#define CHERRY_SHELL_SYM_VAR    0x03U  /* Variable / Pointer */

/* Shell return codes */
#define CHERRY_SHELL_OK         0
#define CHERRY_SHELL_ERR_NOTFOUND   -1
#define CHERRY_SHELL_ERR_INVAL      -2
#define CHERRY_SHELL_ERR_NOMEM      -3
#define CHERRY_SHELL_ERR_EXEC       -4

/**
 * Symbol representation in CherryShell
 */
struct cherry_shell_symbol {
    const char *name;         /* Symbol identifier (e.g. "vfs_open") */
    uintptr_t   addr;         /* Function or variable address */
    uint32_t    type;         /* CHERRY_SHELL_SYM_* */
    uint32_t    flags;        /* Flags / Reserved */
    const char *description;  /* Human-readable description */
};

/* Command callback function prototype */
typedef int (*cherry_shell_cmd_fn)(int argc, char **argv);

/* General C function prototype for dynamic AAPCS64 invocation (up to 8 args) */
typedef uintptr_t (*cherry_shell_fn8)(uintptr_t a0, uintptr_t a1, uintptr_t a2, uintptr_t a3,
                                     uintptr_t a4, uintptr_t a5, uintptr_t a6, uintptr_t a7);

/* =========================================================================
 * Compile-time Symbol Export Macros (Section: .cherry_shell_sym)
 * ========================================================================= */
#define CHERRY_SHELL_EXPORT_CMD(_sname, _func, _desc) \
    __attribute__((used, section(".cherry_shell_sym"))) \
    static const struct cherry_shell_symbol _cherry_cmd_##_sname = { \
        .name = #_sname, \
        .addr = (uintptr_t)(_func), \
        .type = CHERRY_SHELL_SYM_CMD, \
        .flags = 0, \
        .description = _desc \
    }

#define CHERRY_SHELL_EXPORT_SYM(_sname, _func, _desc) \
    __attribute__((used, section(".cherry_shell_sym"))) \
    static const struct cherry_shell_symbol _cherry_sym_##_sname = { \
        .name = #_sname, \
        .addr = (uintptr_t)(_func), \
        .type = CHERRY_SHELL_SYM_FUNC, \
        .flags = 0, \
        .description = _desc \
    }

#define CHERRY_SHELL_EXPORT_VAR(_sname, _var, _desc) \
    __attribute__((used, section(".cherry_shell_sym"))) \
    static const struct cherry_shell_symbol _cherry_var_##_sname = { \
        .name = #_sname, \
        .addr = (uintptr_t)(&(_var)), \
        .type = CHERRY_SHELL_SYM_VAR, \
        .flags = 0, \
        .description = _desc \
    }

/* =========================================================================
 * CherryShell Core APIs
 * ========================================================================= */

/**
 * Initialize CherryShell subsystem (symbol table, built-in commands).
 */
int cherry_shell_init(void);

/**
 * Dynamically register a symbol at runtime.
 */
int cherry_shell_register_symbol(const char *name, uintptr_t addr, uint32_t type, const char *desc);

/**
 * Look up symbol by name.
 */
const struct cherry_shell_symbol *cherry_shell_find_symbol_by_name(const char *name);

/**
 * Look up nearest symbol by address (addr2sym).
 * Calculates offset from symbol start.
 */
const struct cherry_shell_symbol *cherry_shell_find_symbol_by_addr(uintptr_t addr, uintptr_t *out_offset);

/**
 * Format address to symbol string: "func_name" or "func_name+0x18"
 *
 * @param addr Address to resolve
 * @param name_buf Output buffer for symbol name / offset string
 * @param buf_len Length of output buffer
 * @param out_offset Optional pointer to store computed offset
 * @return 0 on success, negative error code if no matching symbol
 */
int cherry_shell_addr_to_sym(uintptr_t addr, char *name_buf, size_t buf_len, uintptr_t *out_offset);

/**
 * Look up address by symbol name.
 *
 * @param name Symbol name
 * @return Address, or 0 if not found
 */
uintptr_t cherry_shell_sym_to_addr(const char *name);

/**
 * Iterate all registered symbols.
 *
 * @param cb Callback function called for each symbol
 * @param user_data Context passed to callback
 */
typedef int (*cherry_shell_sym_iterator_t)(const struct cherry_shell_symbol *sym, void *user_data);
int cherry_shell_for_each_symbol(cherry_shell_sym_iterator_t cb, void *user_data);

/**
 * Invoke a C function dynamically with up to 8 arguments (AAPCS64 x0-x7).
 */
uintptr_t cherry_shell_call(void *func_ptr, int argc, uintptr_t argv[8]);

/**
 * Execute a command line string.
 * Supports both C function syntax: `func(arg1, arg2)`
 * and shell command syntax: `cmd arg1 arg2`
 *
 * @param cmdline Command line string to execute
 * @param out_buf Optional output buffer for execution message
 * @param out_buf_len Length of output buffer
 * @return Execution status (0 on success)
 */
int cherry_shell_exec(const char *cmdline, char *out_buf, size_t out_buf_len);

/**
 * Start the interactive shell task.
 */
int cherry_shell_start(void);

#ifdef __cplusplus
}
#endif

#endif /* _CHERRY_SHELL_H_ */
