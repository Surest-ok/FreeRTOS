/*
 * CherryProcess - ELF64 Loader Implementation for Linux AArch64 Executables
 *
 * Copyright (c) 2026 ForgeStitch
 * SPDX-License-Identifier: MIT
 */

#include "cherry_elf.h"
#include "cherry_process.h"
#include "cherry_syscall.h"
#include "cherry_kal.h"
#include "cherry_printk.h"
#include <string.h>

#define ELF_PAGE_SIZE       4096
#define ELF_ALIGN_UP(x, a)  (((x) + ((a) - 1)) & ~((a) - 1))
#define ELF_ALIGN_DN(x, a)  ((x) & ~((a) - 1))

int proc_elf_validate(const void *buf, size_t size)
{
    if (!buf || size < sizeof(Elf64_Ehdr)) {
        return -EINVAL;
    }

    const Elf64_Ehdr *ehdr = (const Elf64_Ehdr *)buf;

    if (ehdr->e_ident[EI_MAG0] != ELFMAG0 ||
        ehdr->e_ident[EI_MAG1] != ELFMAG1 ||
        ehdr->e_ident[EI_MAG2] != ELFMAG2 ||
        ehdr->e_ident[EI_MAG3] != ELFMAG3) {
        return -ENOEXEC;
    }

    if (ehdr->e_ident[EI_CLASS] != ELFCLASS64) {
        return -ENOEXEC;
    }

    if (ehdr->e_ident[EI_DATA] != ELFDATA2LSB) {
        return -ENOEXEC;
    }

    if (ehdr->e_machine != EM_AARCH64) {
        return -ENOEXEC;
    }

    if (ehdr->e_type != ET_EXEC && ehdr->e_type != ET_DYN) {
        return -ENOEXEC;
    }

    return 0;
}

int proc_elf_load(const char *path, struct elf_image *img)
{
    if (!path || !img) {
        return -EINVAL;
    }
    memset(img, 0, sizeof(struct elf_image));

    int fd = sys_open(path, O_RDONLY, 0);
    if (fd < 0) {
        pr_err("proc_elf: failed to open '%s' (err=%d)\n", path, fd);
        return fd;
    }

    Elf64_Ehdr ehdr;
    ssize_t rd = sys_read(fd, &ehdr, sizeof(Elf64_Ehdr));
    if (rd != sizeof(Elf64_Ehdr)) {
        sys_close(fd);
        return -EIO;
    }

    int v_ret = proc_elf_validate(&ehdr, sizeof(Elf64_Ehdr));
    if (v_ret != 0) {
        pr_err("proc_elf: invalid ELF header in '%s' (err=%d)\n", path, v_ret);
        sys_close(fd);
        return v_ret;
    }

    if (ehdr.e_phnum == 0 || ehdr.e_phentsize != sizeof(Elf64_Phdr)) {
        sys_close(fd);
        return -ENOEXEC;
    }

    size_t phdr_bytes = ehdr.e_phnum * sizeof(Elf64_Phdr);
    Elf64_Phdr *ph_table = (Elf64_Phdr *)kmalloc(phdr_bytes, GFP_KERNEL);
    if (!ph_table) {
        sys_close(fd);
        return -ENOMEM;
    }

    sys_lseek(fd, (off_t)ehdr.e_phoff, 0);
    rd = sys_read(fd, ph_table, phdr_bytes);
    if (rd != (ssize_t)phdr_bytes) {
        kfree(ph_table);
        sys_close(fd);
        return -EIO;
    }

    /* Compute bounding span of all PT_LOAD segments */
    uintptr_t min_vaddr = (uintptr_t)-1;
    uintptr_t max_vaddr = 0;
    int load_count = 0;

    for (int i = 0; i < ehdr.e_phnum; i++) {
        if (ph_table[i].p_type == PT_LOAD) {
            load_count++;
            uintptr_t seg_start = ph_table[i].p_vaddr;
            uintptr_t seg_end   = ph_table[i].p_vaddr + ph_table[i].p_memsz;
            if (seg_start < min_vaddr) min_vaddr = seg_start;
            if (seg_end > max_vaddr)   max_vaddr = seg_end;
        }
    }

    if (load_count == 0 || max_vaddr <= min_vaddr) {
        kfree(ph_table);
        sys_close(fd);
        return -ENOEXEC;
    }

    bool is_pie = (ehdr.e_type == ET_DYN);
    size_t span = ELF_ALIGN_UP(max_vaddr - min_vaddr, ELF_PAGE_SIZE);

    /* Allocate memory for program image */
    void *load_mem = kmalloc(span + ELF_PAGE_SIZE, GFP_KERNEL);
    if (!load_mem) {
        kfree(ph_table);
        sys_close(fd);
        return -ENOMEM;
    }
    uintptr_t base = ELF_ALIGN_UP((uintptr_t)load_mem, ELF_PAGE_SIZE);
    memset((void *)base, 0, span);

    /* Load each PT_LOAD segment */
    for (int i = 0; i < ehdr.e_phnum; i++) {
        if (ph_table[i].p_type == PT_LOAD) {
            uintptr_t dest_addr;
            if (is_pie) {
                dest_addr = base + (ph_table[i].p_vaddr - min_vaddr);
            } else {
                /* If fixed executable address is within accessible memory */
                dest_addr = base + (ph_table[i].p_vaddr - min_vaddr);
            }

            if (ph_table[i].p_filesz > 0) {
                sys_lseek(fd, (off_t)ph_table[i].p_offset, 0);
                ssize_t seg_rd = sys_read(fd, (void *)dest_addr, ph_table[i].p_filesz);
                if (seg_rd != (ssize_t)ph_table[i].p_filesz) {
                    kfree(load_mem);
                    kfree(ph_table);
                    sys_close(fd);
                    return -EIO;
                }
            }

            /* Zero BSS portion */
            if (ph_table[i].p_memsz > ph_table[i].p_filesz) {
                size_t bss_sz = ph_table[i].p_memsz - ph_table[i].p_filesz;
                memset((void *)(dest_addr + ph_table[i].p_filesz), 0, bss_sz);
            }
        }
    }

    sys_close(fd);

    img->raw_mem     = load_mem;
    img->load_base   = base;
    img->total_size  = span;
    img->is_pie      = is_pie;
    img->entry_point = is_pie ? (base + (ehdr.e_entry - min_vaddr)) : (base + (ehdr.e_entry - min_vaddr));
    img->phnum       = ehdr.e_phnum;
    img->phentsize   = ehdr.e_phentsize;
    img->phdr_vaddr  = (uintptr_t)ph_table;

    pr_info("proc_elf: loaded '%s' (base=0x%lx, entry=0x%lx, size=%lu bytes, pie=%d)\n",
            path, (unsigned long)img->load_base, (unsigned long)img->entry_point,
            (unsigned long)img->total_size, img->is_pie);

    return 0;
}

void proc_elf_unload(struct elf_image *img)
{
    if (!img) return;
    if (img->raw_mem) {
        kfree(img->raw_mem);
        img->raw_mem = NULL;
    }
    if (img->phdr_vaddr) {
        kfree((void *)img->phdr_vaddr);
        img->phdr_vaddr = 0;
    }
}

uintptr_t proc_elf_setup_stack(uintptr_t stack_top, int argc, char **argv, char **envp,
                               const struct elf_image *img)
{
    if (!stack_top || !img) return 0;

    uintptr_t sp = stack_top;

    /* 1. Allocate space on stack for 16-byte random seed (AT_RANDOM) */
    sp -= 16;
    uint8_t *rand_seed = (uint8_t *)sp;
    uint64_t t = ktime_get_real_ns();
    for (int i = 0; i < 16; i++) {
        rand_seed[i] = (uint8_t)((t >> (i * 3)) ^ (i * 0x5a + 0x33));
    }
    uintptr_t at_random_ptr = sp;

    /* 2. Count environment variables */
    int envc = 0;
    if (envp) {
        while (envp[envc]) envc++;
    }

    /* 3. Push string data for envp onto stack */
    uintptr_t envp_ptrs[PROC_MAX_ENVP];
    for (int i = envc - 1; i >= 0; i--) {
        size_t len = strlen(envp[i]) + 1;
        sp -= len;
        memcpy((void *)sp, envp[i], len);
        envp_ptrs[i] = sp;
    }

    /* 4. Push string data for argv onto stack */
    uintptr_t argv_ptrs[PROC_MAX_ARGS];
    for (int i = argc - 1; i >= 0; i--) {
        if (argv && argv[i]) {
            size_t len = strlen(argv[i]) + 1;
            sp -= len;
            memcpy((void *)sp, argv[i], len);
            argv_ptrs[i] = sp;
        } else {
            argv_ptrs[i] = 0;
        }
    }

    /* 16-byte alignment */
    sp = ELF_ALIGN_DN(sp, 16);

    /* 5. Construct Auxiliary Vector (auxv) */
    struct {
        uint64_t a_type;
        uint64_t a_val;
    } auxv[] = {
        { AT_PHDR,          img->phdr_vaddr },
        { AT_PHENT,         img->phentsize },
        { AT_PHNUM,         img->phnum },
        { AT_PAGESZ,        ELF_PAGE_SIZE },
        { AT_BASE,          img->load_base },
        { AT_FLAGS,         0 },
        { AT_ENTRY,         img->entry_point },
        { AT_UID,           0 },
        { AT_EUID,          0 },
        { AT_GID,           0 },
        { AT_EGID,          0 },
        { AT_SECURE,        0 },
        { AT_RANDOM,        at_random_ptr },
        { AT_CLKTCK,        100 },
        { AT_NULL,          0 }
    };

    int auxv_count = sizeof(auxv) / sizeof(auxv[0]);

    /* Allocate space for auxv, envp[], argv[], argc */
    size_t table_size = (auxv_count * sizeof(auxv[0])) +
                        ((envc + 1) * sizeof(uint64_t)) +
                        ((argc + 1) * sizeof(uint64_t)) +
                        sizeof(uint64_t); /* argc */

    table_size = ELF_ALIGN_UP(table_size, 16);
    sp -= table_size;

    uint64_t *stack_words = (uint64_t *)sp;
    int idx = 0;

    /* argc */
    stack_words[idx++] = (uint64_t)argc;

    /* argv pointers */
    for (int i = 0; i < argc; i++) {
        stack_words[idx++] = (uint64_t)argv_ptrs[i];
    }
    stack_words[idx++] = 0; /* NULL terminator */

    /* envp pointers */
    for (int i = 0; i < envc; i++) {
        stack_words[idx++] = (uint64_t)envp_ptrs[i];
    }
    stack_words[idx++] = 0; /* NULL terminator */

    /* auxv table */
    for (int i = 0; i < auxv_count; i++) {
        stack_words[idx++] = auxv[i].a_type;
        stack_words[idx++] = auxv[i].a_val;
    }

    return sp;
}

static int elf_entry_trampoline(int argc, char **argv)
{
    (void)argc;
    if (!argv || !argv[0]) return -1;

    struct elf_image *img = (struct elf_image *)argv[0];
    if (!img || !img->entry_point) return -1;

    /* Invoke entry point */
    typedef int (*elf_entry_fn)(void);
    elf_entry_fn entry = (elf_entry_fn)img->entry_point;

    int ret = entry();
    return ret;
}

int proc_spawn_elf(const char *path, int argc, char **argv, char **envp,
                   int stdin_vfs_fd, int stdout_vfs_fd, int stderr_vfs_fd)
{
    (void)argc;
    (void)argv;
    struct elf_image *img = (struct elf_image *)kmalloc(sizeof(struct elf_image), GFP_KERNEL);
    if (!img) return -ENOMEM;

    int ret = proc_elf_load(path, img);
    if (ret != 0) {
        kfree(img);
        return ret;
    }

    char *spawn_argv[2];
    spawn_argv[0] = (char *)img;
    spawn_argv[1] = NULL;

    pid_t pid = proc_spawn(path, elf_entry_trampoline, 1, spawn_argv, envp,
                           stdin_vfs_fd, stdout_vfs_fd, stderr_vfs_fd);

    if (pid <= 0) {
        proc_elf_unload(img);
        kfree(img);
        return pid;
    }

    return pid;
}
