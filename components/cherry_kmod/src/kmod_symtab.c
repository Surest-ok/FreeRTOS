/*
 * CherryKmod - Kernel Symbol Table & Export Manager
 *
 * Copyright (c) 2026 ForgeStitch
 * SPDX-License-Identifier: MIT
 */

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include "cherry_kmod.h"
#include "cherry_kal.h"

#define SYMTAB_HASH_SIZE    256

struct kmod_sym_entry {
    char                    name[64];
    uintptr_t               addr;
    struct kmod_sym_entry  *next;
};

static struct kmod_sym_entry *g_symtab[SYMTAB_HASH_SIZE];
static spinlock_t g_symtab_lock;
static bool g_symtab_initialized = false;

/* External linker section symbols */
extern const struct cherry_kmod_symbol __ksymtab_start[];
extern const struct cherry_kmod_symbol __ksymtab_end[];

/* Weak reference to cherry_shell symbol resolution if available */
struct cherry_shell_symbol;
extern const struct cherry_shell_symbol *cherry_shell_find_symbol_by_name(const char *name) __attribute__((weak));

/* Standard kernel symbols */
extern int uart_printf(const char *fmt, ...);
extern void uart_puts(const char *s);
extern void *kmalloc(size_t size, gfp_t flags);
extern void *kzalloc(size_t size, gfp_t flags);
extern uint64_t ktime_get_ms(void);

extern void vTaskDelay(const uint32_t xTicksToDelay);
extern uint32_t xTaskGetTickCount(void);
extern void vTaskDelete(void *xTaskToDelete);

static uint32_t sym_hash(const char *s)
{
    uint32_t h = 5381;
    while (*s) {
        h = ((h << 5) + h) + (uint32_t)(*s++);
    }
    return h % SYMTAB_HASH_SIZE;
}

int cherry_kmod_export_symbol(const char *name, uintptr_t addr)
{
    if (!name || !*name || !addr) {
        return -1;
    }

    uint32_t idx = sym_hash(name);
    unsigned long flags;
    spin_lock_irqsave(&g_symtab_lock, flags);

    /* Check if already present */
    struct kmod_sym_entry *curr = g_symtab[idx];
    while (curr) {
        if (strcmp(curr->name, name) == 0) {
            curr->addr = addr;
            spin_unlock_irqrestore(&g_symtab_lock, flags);
            return 0;
        }
        curr = curr->next;
    }

    struct kmod_sym_entry *entry = (struct kmod_sym_entry *)kmalloc(sizeof(struct kmod_sym_entry), GFP_KERNEL);
    if (!entry) {
        spin_unlock_irqrestore(&g_symtab_lock, flags);
        return -2;
    }

    strncpy(entry->name, name, sizeof(entry->name) - 1);
    entry->name[sizeof(entry->name) - 1] = '\0';
    entry->addr = addr;
    entry->next = g_symtab[idx];
    g_symtab[idx] = entry;

    spin_unlock_irqrestore(&g_symtab_lock, flags);
    return 0;
}

int cherry_kmod_unexport_symbol(const char *name)
{
    if (!name || !*name) {
        return -1;
    }

    uint32_t idx = sym_hash(name);
    unsigned long flags;
    spin_lock_irqsave(&g_symtab_lock, flags);

    struct kmod_sym_entry **curr_p = &g_symtab[idx];
    while (*curr_p) {
        struct kmod_sym_entry *entry = *curr_p;
        if (strcmp(entry->name, name) == 0) {
            *curr_p = entry->next;
            kfree(entry);
            spin_unlock_irqrestore(&g_symtab_lock, flags);
            return 0;
        }
        curr_p = &entry->next;
    }

    spin_unlock_irqrestore(&g_symtab_lock, flags);
    return -1;
}

uintptr_t cherry_kmod_lookup_symbol(const char *name)
{
    if (!name || !*name) {
        return 0;
    }

    /* 1. Check dynamic symbol table */
    uint32_t idx = sym_hash(name);
    unsigned long flags;
    spin_lock_irqsave(&g_symtab_lock, flags);

    struct kmod_sym_entry *curr = g_symtab[idx];
    while (curr) {
        if (strcmp(curr->name, name) == 0) {
            uintptr_t a = curr->addr;
            spin_unlock_irqrestore(&g_symtab_lock, flags);
            return a;
        }
        curr = curr->next;
    }
    spin_unlock_irqrestore(&g_symtab_lock, flags);

    /* 2. Check compiled-in __ksymtab section */
    if (__ksymtab_start < __ksymtab_end) {
        const struct cherry_kmod_symbol *sym = __ksymtab_start;
        while (sym < __ksymtab_end) {
            if (sym->name && strcmp(sym->name, name) == 0) {
                return sym->addr;
            }
            sym++;
        }
    }

    /* 3. Fallback: Check CherryShell symbol table if present */
    if (cherry_shell_find_symbol_by_name) {
        /* Definition of cherry_shell_symbol: { const char *name; uintptr_t addr; ... } */
        const void *sh_sym = cherry_shell_find_symbol_by_name(name);
        if (sh_sym) {
            uintptr_t sh_addr = *(const uintptr_t *)((const char *)sh_sym + sizeof(const char *));
            if (sh_addr != 0) {
                return sh_addr;
            }
        }
    }

    return 0;
}

void kmod_init_symtab(void)
{
    if (g_symtab_initialized) {
        return;
    }
    spin_lock_init(&g_symtab_lock);
    g_symtab_initialized = true;

    /* Register core OS and runtime symbols */
    cherry_kmod_export_symbol("printk", (uintptr_t)printk);
    cherry_kmod_export_symbol("uart_printf", (uintptr_t)uart_printf);
    cherry_kmod_export_symbol("uart_puts", (uintptr_t)uart_puts);
    cherry_kmod_export_symbol("kmalloc", (uintptr_t)kmalloc);
    cherry_kmod_export_symbol("kfree", (uintptr_t)kfree);
    cherry_kmod_export_symbol("kzalloc", (uintptr_t)kzalloc);
    cherry_kmod_export_symbol("krealloc", (uintptr_t)krealloc);
    cherry_kmod_export_symbol("ktime_get_ms", (uintptr_t)ktime_get_ms);

    cherry_kmod_export_symbol("vTaskDelay", (uintptr_t)vTaskDelay);
    cherry_kmod_export_symbol("xTaskGetTickCount", (uintptr_t)xTaskGetTickCount);
    cherry_kmod_export_symbol("vTaskDelete", (uintptr_t)vTaskDelete);

    cherry_kmod_export_symbol("cherry_kmod_export_symbol", (uintptr_t)cherry_kmod_export_symbol);
    cherry_kmod_export_symbol("cherry_kmod_lookup_symbol", (uintptr_t)cherry_kmod_lookup_symbol);
}
