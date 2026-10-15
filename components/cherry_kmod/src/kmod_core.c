/*
 * CherryKmod - Kernel Module Core & Lifecycle Manager
 *
 * Copyright (c) 2026 ForgeStitch
 * SPDX-License-Identifier: MIT
 */

#include <stdint.h>
#include <stddef.h>
extern int snprintf(char *str, size_t size, const char *format, ...);
#include <stdbool.h>
#include <string.h>
#include "cherry_kmod.h"
#include "cherry_kal.h"
#include "cherry_vfs.h"

extern void kmod_init_symtab(void);
extern int kmod_elf_load(const void *buf, size_t size, struct cherry_kmod *mod);

static struct cherry_kmod *g_kmod_list = NULL;
static struct mutex g_kmod_mutex;
static bool g_kmod_initialized = false;

static void extract_mod_name(const char *path, char *out, size_t max_len)
{
    const char *slash = strrchr(path, '/');
    const char *base = slash ? slash + 1 : path;
    size_t i = 0;
    while (base[i] && base[i] != '.' && i < max_len - 1) {
        out[i] = base[i];
        i++;
    }
    out[i] = '\0';
}

int cherry_kmod_init(void)
{
    if (g_kmod_initialized) {
        return 0;
    }
    g_kmod_initialized = true;

    mutex_init(&g_kmod_mutex);
    kmod_init_symtab();

    /* Register shell commands */
    cherry_kmod_export_symbol("cherry_kmod_load_buffer", (uintptr_t)cherry_kmod_load_buffer);
    cherry_kmod_export_symbol("cherry_kmod_load_file", (uintptr_t)cherry_kmod_load_file);
    cherry_kmod_export_symbol("cherry_kmod_unload", (uintptr_t)cherry_kmod_unload);
    cherry_kmod_export_symbol("cherry_kmod_find", (uintptr_t)cherry_kmod_find);

    printk("[kmod] kernel module subsystem initialized successfully\n");
    return 0;
}

struct cherry_kmod *cherry_kmod_find(const char *name)
{
    if (!name || !*name) return NULL;

    mutex_lock(&g_kmod_mutex);
    struct cherry_kmod *curr = g_kmod_list;
    while (curr) {
        if (strcmp(curr->name, name) == 0) {
            mutex_unlock(&g_kmod_mutex);
            return curr;
        }
        curr = curr->next;
    }
    mutex_unlock(&g_kmod_mutex);
    return NULL;
}

struct cherry_kmod *cherry_kmod_load_buffer(const void *buf, size_t size, const char *name)
{
    if (!buf || size == 0 || !name || !*name) {
        return NULL;
    }

    if (!g_kmod_initialized) {
        cherry_kmod_init();
    }

    if (cherry_kmod_find(name) != NULL) {
        printk("kmod: module '%s' is already loaded\n", name);
        return NULL;
    }

    struct cherry_kmod *mod = (struct cherry_kmod *)kzalloc(sizeof(struct cherry_kmod), GFP_KERNEL);
    if (!mod) {
        return NULL;
    }

    strncpy(mod->name, name, KMOD_NAME_MAX - 1);
    mod->name[KMOD_NAME_MAX - 1] = '\0';
    mod->state = KMOD_STATE_COMING;

    int ret = kmod_elf_load(buf, size, mod);
    if (ret != 0) {
        printk("kmod: failed to load module '%s' (err=%d)\n", name, ret);
        kfree(mod);
        return NULL;
    }

    /* Add to active module list */
    mutex_lock(&g_kmod_mutex);
    mod->next = g_kmod_list;
    g_kmod_list = mod;
    mod->state = KMOD_STATE_LIVE;
    mutex_unlock(&g_kmod_mutex);

    /* Call module initialization function if present */
    if (mod->init) {
        int init_ret = mod->init();
        if (init_ret != 0) {
            printk("kmod: module '%s' init_module() failed with error %d\n", name, init_ret);
            cherry_kmod_unload(name);
            return NULL;
        }
    }

    printk("kmod: loaded '%s' (load_addr=0x%lx, size=%lu bytes, syms=%lu)\n",
           mod->name, (uintptr_t)mod->load_addr, (unsigned long)mod->load_size, (unsigned long)mod->num_syms);

    return mod;
}

struct cherry_kmod *cherry_kmod_load_file(const char *path)
{
    if (!path || !*path) {
        return NULL;
    }

    int fd = open(path, O_RDONLY, 0);
    if (fd < 0) {
        printk("kmod: cannot open '%s' (err=%d)\n", path, fd);
        return NULL;
    }

    off_t file_size = lseek(fd, 0, SEEK_END);
    if (file_size <= 0) {
        close(fd);
        printk("kmod: empty file '%s'\n", path);
        return NULL;
    }
    lseek(fd, 0, SEEK_SET);

    uint8_t *file_buf = (uint8_t *)kmalloc(file_size, GFP_KERNEL);
    if (!file_buf) {
        close(fd);
        printk("kmod: out of memory reading '%s'\n", path);
        return NULL;
    }

    ssize_t rd = read(fd, file_buf, file_size);
    close(fd);

    if (rd != file_size) {
        kfree(file_buf);
        printk("kmod: read truncated in '%s'\n", path);
        return NULL;
    }

    char mod_name[KMOD_NAME_MAX];
    extract_mod_name(path, mod_name, sizeof(mod_name));

    struct cherry_kmod *mod = cherry_kmod_load_buffer(file_buf, file_size, mod_name);
    kfree(file_buf);

    return mod;
}

int cherry_kmod_unload(const char *name)
{
    if (!name || !*name) {
        return -1;
    }

    mutex_lock(&g_kmod_mutex);
    struct cherry_kmod **curr_p = &g_kmod_list;
    while (*curr_p) {
        struct cherry_kmod *mod = *curr_p;
        if (strcmp(mod->name, name) == 0) {
            if (mod->ref_count > 0) {
                mutex_unlock(&g_kmod_mutex);
                printk("kmod: module '%s' is in use (ref_count=%d)\n", name, mod->ref_count);
                return -2;
            }

            mod->state = KMOD_STATE_GOING;
            *curr_p = mod->next;
            mutex_unlock(&g_kmod_mutex);

            /* Call cleanup_module() if present */
            if (mod->exit) {
                mod->exit();
            }

            /* Unexport all symbols registered by this module */
            if (mod->syms) {
                for (size_t i = 0; i < mod->num_syms; i++) {
                    if (mod->syms[i].name) {
                        cherry_kmod_unexport_symbol(mod->syms[i].name);
                    }
                }
                kfree(mod->syms);
            }

            /* Free allocated executable memory */
            if (mod->load_addr) {
                kfree(mod->load_addr);
            }

            printk("kmod: unloaded '%s'\n", mod->name);
            kfree(mod);
            return 0;
        }
        curr_p = &mod->next;
    }

    mutex_unlock(&g_kmod_mutex);
    printk("kmod: module '%s' not found\n", name);
    return -3;
}

int cherry_kmod_for_each(int (*cb)(struct cherry_kmod *mod, void *arg), void *arg)
{
    if (!cb) return -1;

    mutex_lock(&g_kmod_mutex);
    struct cherry_kmod *curr = g_kmod_list;
    while (curr) {
        int ret = cb(curr, arg);
        if (ret != 0) {
            mutex_unlock(&g_kmod_mutex);
            return ret;
        }
        curr = curr->next;
    }
    mutex_unlock(&g_kmod_mutex);
    return 0;
}

int cherry_kmod_list(char *buf, size_t buf_len)
{
    if (!buf || buf_len < 64) {
        return -1;
    }

    size_t off = 0;
    int written = snprintf(buf + off, buf_len - off, "%-24s %8s %7s\n", "Module", "Size", "Used by");
    if (written > 0) off += (size_t)written;

    mutex_lock(&g_kmod_mutex);
    struct cherry_kmod *curr = g_kmod_list;
    while (curr && off + 48 < buf_len) {
        written = snprintf(buf + off, buf_len - off, "%-24s %8lu %7d\n",
                           curr->name, (unsigned long)curr->load_size, curr->ref_count);
        if (written > 0) off += (size_t)written;
        curr = curr->next;
    }
    mutex_unlock(&g_kmod_mutex);

    return (int)off;
}
