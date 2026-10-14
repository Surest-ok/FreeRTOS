/*
 * CherryShell - Symbol Management & Address-to-Symbol (addr2sym) Engine
 *
 * Copyright (c) 2026 ForgeStitch
 * SPDX-License-Identifier: MIT
 */

#include "cherry_shell.h"
#include "cherry_kal.h"
#include "uart.h" /* for snprintf */
#include <string.h>

#define CHERRY_SHELL_MAX_DYNAMIC_SYMS  256
#define CHERRY_SHELL_MAX_ADDR_DELTA    0x100000UL /* 1MB max distance for addr2sym */

/* Linker symbols for static export table */
extern const struct cherry_shell_symbol __cherry_shell_sym_start[];
extern const struct cherry_shell_symbol __cherry_shell_sym_end[];

static struct cherry_shell_symbol g_dyn_syms[CHERRY_SHELL_MAX_DYNAMIC_SYMS];
static size_t g_dyn_sym_count = 0;
static struct mutex g_sym_mutex;
static int g_sym_mutex_inited = 0;

static void sym_lock(void)
{
    if (g_sym_mutex_inited) {
        mutex_lock(&g_sym_mutex);
    }
}

static void sym_unlock(void)
{
    if (g_sym_mutex_inited) {
        mutex_unlock(&g_sym_mutex);
    }
}

int cherry_shell_sym_init(void)
{
    if (!g_sym_mutex_inited) {
        mutex_init(&g_sym_mutex);
        g_sym_mutex_inited = 1;
    }
    return CHERRY_SHELL_OK;
}

int cherry_shell_register_symbol(const char *name, uintptr_t addr, uint32_t type, const char *desc)
{
    if (!name || addr == 0) {
        return CHERRY_SHELL_ERR_INVAL;
    }

    sym_lock();

    /* Check if already registered */
    for (size_t i = 0; i < g_dyn_sym_count; i++) {
        if (strcmp(g_dyn_syms[i].name, name) == 0) {
            g_dyn_syms[i].addr = addr;
            g_dyn_syms[i].type = type;
            g_dyn_syms[i].description = desc;
            sym_unlock();
            return CHERRY_SHELL_OK;
        }
    }

    if (g_dyn_sym_count >= CHERRY_SHELL_MAX_DYNAMIC_SYMS) {
        sym_unlock();
        return CHERRY_SHELL_ERR_NOMEM;
    }

    g_dyn_syms[g_dyn_sym_count].name = name;
    g_dyn_syms[g_dyn_sym_count].addr = addr;
    g_dyn_syms[g_dyn_sym_count].type = type;
    g_dyn_syms[g_dyn_sym_count].flags = 0;
    g_dyn_syms[g_dyn_sym_count].description = desc;
    g_dyn_sym_count++;

    sym_unlock();
    return CHERRY_SHELL_OK;
}

const struct cherry_shell_symbol *cherry_shell_find_symbol_by_name(const char *name)
{
    if (!name) return NULL;

    sym_lock();

    /* 1. Search compile-time exported symbols (.cherry_shell_sym) */
    const struct cherry_shell_symbol *sym = __cherry_shell_sym_start;
    while (sym < __cherry_shell_sym_end) {
        if (sym->name && strcmp(sym->name, name) == 0) {
            sym_unlock();
            return sym;
        }
        sym++;
    }

    /* 2. Search dynamic symbols */
    for (size_t i = 0; i < g_dyn_sym_count; i++) {
        if (g_dyn_syms[i].name && strcmp(g_dyn_syms[i].name, name) == 0) {
            sym_unlock();
            return &g_dyn_syms[i];
        }
    }

    sym_unlock();
    return NULL;
}

uintptr_t cherry_shell_sym_to_addr(const char *name)
{
    const struct cherry_shell_symbol *sym = cherry_shell_find_symbol_by_name(name);
    return sym ? sym->addr : 0;
}

const struct cherry_shell_symbol *cherry_shell_find_symbol_by_addr(uintptr_t addr, uintptr_t *out_offset)
{
    if (addr == 0) return NULL;

    sym_lock();

    const struct cherry_shell_symbol *best_sym = NULL;
    uintptr_t min_delta = (uintptr_t)-1;

    /* 1. Search compile-time symbols */
    const struct cherry_shell_symbol *sym = __cherry_shell_sym_start;
    while (sym < __cherry_shell_sym_end) {
        if (sym->addr != 0 && sym->addr <= addr) {
            uintptr_t delta = addr - sym->addr;
            if (delta < min_delta && delta < CHERRY_SHELL_MAX_ADDR_DELTA) {
                min_delta = delta;
                best_sym = sym;
                if (delta == 0) break; /* Exact match */
            }
        }
        sym++;
    }

    /* 2. Search dynamic symbols */
    for (size_t i = 0; i < g_dyn_sym_count; i++) {
        uintptr_t saddr = g_dyn_syms[i].addr;
        if (saddr != 0 && saddr <= addr) {
            uintptr_t delta = addr - saddr;
            if (delta < min_delta && delta < CHERRY_SHELL_MAX_ADDR_DELTA) {
                min_delta = delta;
                best_sym = &g_dyn_syms[i];
                if (delta == 0) break;
            }
        }
    }

    sym_unlock();

    if (best_sym) {
        if (out_offset) {
            *out_offset = min_delta;
        }
        return best_sym;
    }

    return NULL;
}

int cherry_shell_addr_to_sym(uintptr_t addr, char *name_buf, size_t buf_len, uintptr_t *out_offset)
{
    if (!name_buf || buf_len == 0) {
        return CHERRY_SHELL_ERR_INVAL;
    }

    uintptr_t offset = 0;
    const struct cherry_shell_symbol *sym = cherry_shell_find_symbol_by_addr(addr, &offset);

    if (out_offset) {
        *out_offset = offset;
    }

    if (sym && sym->name) {
        if (offset == 0) {
            snprintf(name_buf, buf_len, "%s", sym->name);
        } else {
            snprintf(name_buf, buf_len, "%s+0x%lx", sym->name, (unsigned long)offset);
        }
        return CHERRY_SHELL_OK;
    }

    snprintf(name_buf, buf_len, "0x%016lx", (unsigned long)addr);
    return CHERRY_SHELL_ERR_NOTFOUND;
}

int cherry_shell_for_each_symbol(cherry_shell_sym_iterator_t cb, void *user_data)
{
    if (!cb) return CHERRY_SHELL_ERR_INVAL;

    sym_lock();

    /* 1. Static symbols */
    const struct cherry_shell_symbol *sym = __cherry_shell_sym_start;
    while (sym < __cherry_shell_sym_end) {
        if (sym->name && sym->addr != 0) {
            int ret = cb(sym, user_data);
            if (ret != 0) {
                sym_unlock();
                return ret;
            }
        }
        sym++;
    }

    /* 2. Dynamic symbols */
    for (size_t i = 0; i < g_dyn_sym_count; i++) {
        if (g_dyn_syms[i].name && g_dyn_syms[i].addr != 0) {
            int ret = cb(&g_dyn_syms[i], user_data);
            if (ret != 0) {
                sym_unlock();
                return ret;
            }
        }
    }

    sym_unlock();
    return CHERRY_SHELL_OK;
}
