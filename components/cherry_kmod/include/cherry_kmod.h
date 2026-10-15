/*
 * CherryKmod - Kernel Module Subsystem for FreeRTOS AArch64
 *
 * Supports dynamic loading, relocation, symbol resolution, and lifecycle
 * management of ELF64 relocatable objects (.ko).
 *
 * Copyright (c) 2026 ForgeStitch
 * SPDX-License-Identifier: MIT
 */

#ifndef _CHERRY_KMOD_H_
#define _CHERRY_KMOD_H_

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define KMOD_NAME_MAX       32
#define KMOD_HASH_SIZE      128

/* Module lifecycle states */
#define KMOD_STATE_UNLOADED 0
#define KMOD_STATE_LIVE     1
#define KMOD_STATE_COMING   2
#define KMOD_STATE_GOING    3

typedef int (*kmod_init_fn)(void);
typedef void (*kmod_exit_fn)(void);

/**
 * Symbol representation in kernel symbol table
 */
struct cherry_kmod_symbol {
    const char *name;
    uintptr_t   addr;
};

/**
 * Kernel Module Descriptor
 */
struct cherry_kmod {
    char                        name[KMOD_NAME_MAX];
    void                       *load_addr;      /* Allocated executable memory */
    size_t                      load_size;      /* Total size allocated */
    kmod_init_fn                init;           /* init_module */
    kmod_exit_fn                exit;           /* cleanup_module */
    int                         state;          /* KMOD_STATE_* */
    int                         ref_count;      /* Active reference count */
    size_t                      num_syms;       /* Symbols exported by this module */
    struct cherry_kmod_symbol  *syms;           /* Array of exported symbols */
    struct cherry_kmod         *next;
};

/* =========================================================================
 * Compile-time Symbol Export Macros (Section: .ksymtab)
 * ========================================================================= */
#define EXPORT_SYMBOL(sym) \
    __attribute__((used, section("__ksymtab"))) \
    static const struct cherry_kmod_symbol __kmod_sym_##sym = { \
        .name = #sym, \
        .addr = (uintptr_t)&sym \
    }

#define EXPORT_SYMBOL_GPL(sym) EXPORT_SYMBOL(sym)

#define MODULE_LICENSE(lic) \
    static const char __attribute__((used, section(".modinfo"))) __mod_license[] = "license=" lic

#define module_init(fn) int init_module(void) __attribute__((alias(#fn)))
#define module_exit(fn) void cleanup_module(void) __attribute__((alias(#fn)))

/* =========================================================================
 * Kernel Module Core APIs
 * ========================================================================= */

/**
 * Initialize kernel module subsystem and pre-register exported kernel symbols.
 */
int cherry_kmod_init(void);

/**
 * Register an exported symbol in the kernel symbol table.
 */
int cherry_kmod_export_symbol(const char *name, uintptr_t addr);

/**
 * Remove an exported symbol from the kernel symbol table.
 */
int cherry_kmod_unexport_symbol(const char *name);

/**
 * Look up a symbol address by name across exported kernel symbols,
 * module symbols, and system symbols.
 */
uintptr_t cherry_kmod_lookup_symbol(const char *name);

/**
 * Load an in-memory ELF64 relocatable object (.ko), perform relocations,
 * resolve external references, register exported symbols, and invoke init_module().
 */
struct cherry_kmod *cherry_kmod_load_buffer(const void *buf, size_t size, const char *name);

/**
 * Load a module from a VFS file path (e.g. /etc/module.ko).
 */
struct cherry_kmod *cherry_kmod_load_file(const char *path);

/**
 * Unload a kernel module: invokes cleanup_module(), unregisters symbols, and frees memory.
 */
int cherry_kmod_unload(const char *name);

/**
 * Find a loaded module by name.
 */
struct cherry_kmod *cherry_kmod_find(const char *name);

/**
 * Iterate over all loaded modules.
 */
int cherry_kmod_for_each(int (*cb)(struct cherry_kmod *mod, void *arg), void *arg);

/**
 * Format loaded modules list into buffer (lsmod format).
 */
int cherry_kmod_list(char *buf, size_t buf_len);

/* Shell commands */
int cherry_kmod_cmd_insmod(int argc, char **argv);
int cherry_kmod_cmd_rmmod(int argc, char **argv);
int cherry_kmod_cmd_lsmod(int argc, char **argv);

/**
 * Run verification tests for the kernel module subsystem.
 */
int cherry_kmod_run_tests(void);

#ifdef __cplusplus
}
#endif

#endif /* _CHERRY_KMOD_H_ */
