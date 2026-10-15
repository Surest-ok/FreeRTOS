/*
 * CherryKmod - AArch64 ELF64 Relocatable Object Loader
 *
 * Implements section allocation, symbol resolution, and AArch64 ABI relocations
 * for loadable kernel modules (.ko).
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

/* Standard ELF Types */
typedef uint64_t Elf64_Addr;
typedef uint64_t Elf64_Off;
typedef uint16_t Elf64_Half;
typedef uint32_t Elf64_Word;
typedef int32_t  Elf64_Sword;
typedef uint64_t Elf64_Xword;
typedef int64_t  Elf64_Sxword;

#define EI_NIDENT       16

typedef struct {
    unsigned char e_ident[EI_NIDENT];
    Elf64_Half    e_type;
    Elf64_Half    e_machine;
    Elf64_Word    e_version;
    Elf64_Addr    e_entry;
    Elf64_Off     e_phoff;
    Elf64_Off     e_shoff;
    Elf64_Word    e_flags;
    Elf64_Half    e_ehsize;
    Elf64_Half    e_phentsize;
    Elf64_Half    e_phnum;
    Elf64_Half    e_shentsize;
    Elf64_Half    e_shnum;
    Elf64_Half    e_shstrndx;
} Elf64_Ehdr;

typedef struct {
    Elf64_Word   sh_name;
    Elf64_Word   sh_type;
    Elf64_Xword  sh_flags;
    Elf64_Addr   sh_addr;
    Elf64_Off    sh_offset;
    Elf64_Xword  sh_size;
    Elf64_Word   sh_link;
    Elf64_Word   sh_info;
    Elf64_Xword  sh_addralign;
    Elf64_Xword  sh_entsize;
} Elf64_Shdr;

typedef struct {
    Elf64_Word    st_name;
    unsigned char st_info;
    unsigned char st_other;
    Elf64_Half    st_shndx;
    Elf64_Addr    st_value;
    Elf64_Xword   st_size;
} Elf64_Sym;

typedef struct {
    Elf64_Addr   r_offset;
    Elf64_Xword  r_info;
    Elf64_Sxword r_addend;
} Elf64_Rela;

#define ELF64_R_SYM(i)     ((i) >> 32)
#define ELF64_R_TYPE(i)    ((i) & 0xffffffffL)
#define ELF64_ST_BIND(i)   ((i) >> 4)
#define ELF64_ST_TYPE(i)   ((i) & 0xf)

#define STB_LOCAL          0
#define STB_GLOBAL         1
#define STB_WEAK           2

#define STT_NOTYPE         0
#define STT_OBJECT         1
#define STT_FUNC           2
#define STT_SECTION        3
#define STT_FILE           4
#define STT_COMMON         5

#define SHN_UNDEF          0
#define SHN_LORESERVE      0xff00
#define SHN_ABS            0xfff1
#define SHN_COMMON         0xfff2

/* Section types */
#define SHT_NULL           0
#define SHT_PROGBITS       1
#define SHT_SYMTAB         2
#define SHT_STRTAB         3
#define SHT_RELA           4
#define SHT_NOBITS         8

/* Section flags */
#define SHF_WRITE          (1 << 0)
#define SHF_ALLOC          (1 << 1)
#define SHF_EXECINSTR      (1 << 2)

/* AArch64 Relocations */
#define R_AARCH64_NONE                  0
#define R_AARCH64_ABS64                 257
#define R_AARCH64_ABS32                 258
#define R_AARCH64_ABS16                 259
#define R_AARCH64_PREL64                260
#define R_AARCH64_PREL32                261
#define R_AARCH64_PREL16                262
#define R_AARCH64_ADR_PREL_PG_HI21      275
#define R_AARCH64_ADD_ABS_LO12_NC       277
#define R_AARCH64_LDST8_ABS_LO12_NC     278
#define R_AARCH64_JUMP26                282
#define R_AARCH64_CALL26                283
#define R_AARCH64_LDST16_ABS_LO12_NC    284
#define R_AARCH64_LDST32_ABS_LO12_NC    285
#define R_AARCH64_LDST64_ABS_LO12_NC    286
#define R_AARCH64_LDST128_ABS_LO12_NC   299


static inline void kmod_flush_icache(void *start, size_t size)
{
    uintptr_t addr = (uintptr_t)start;
    uintptr_t end = addr + size;

    /* Clean data cache to PoU */
    for (uintptr_t p = addr & ~63ULL; p < end; p += 64) {
        __asm__ volatile("dc cvau, %0" :: "r"(p) : "memory");
    }
    __asm__ volatile("dsb ish" ::: "memory");

    /* Invalidate instruction cache */
    for (uintptr_t p = addr & ~63ULL; p < end; p += 64) {
        __asm__ volatile("ic ivau, %0" :: "r"(p) : "memory");
    }
    __asm__ volatile("dsb ish\nisb" ::: "memory");
}

static inline uintptr_t align_up(uintptr_t val, uintptr_t align)
{
    if (align <= 1) return val;
    return (val + align - 1) & ~(align - 1);
}

int kmod_elf_validate(const void *buf, size_t size)
{
    if (!buf || size < sizeof(Elf64_Ehdr)) {
        return -1;
    }
    const Elf64_Ehdr *eh = (const Elf64_Ehdr *)buf;
    if (eh->e_ident[0] != 0x7f || eh->e_ident[1] != 'E' ||
        eh->e_ident[2] != 'L'  || eh->e_ident[3] != 'F') {
        return -2;
    }
    if (eh->e_ident[4] != 2) { /* ELFCLASS64 */
        return -3;
    }
    if (eh->e_ident[5] != 1) { /* ELFDATA2LSB (Little Endian) */
        return -4;
    }
    if (eh->e_type != 1) { /* ET_REL (Relocatable object) */
        return -5;
    }
    if (eh->e_machine != 183) { /* EM_AARCH64 */
        return -6;
    }
    return 0;
}

int kmod_elf_load(const void *buf, size_t size, struct cherry_kmod *mod)
{
    if (kmod_elf_validate(buf, size) != 0) {
        return -1;
    }

    const uint8_t *raw = (const uint8_t *)buf;
    const Elf64_Ehdr *eh = (const Elf64_Ehdr *)raw;
    const Elf64_Shdr *sh_table = (const Elf64_Shdr *)(raw + eh->e_shoff);

    if (eh->e_shstrndx >= eh->e_shnum) {
        return -2;
    }
    const char *shstrtab = (const char *)(raw + sh_table[eh->e_shstrndx].sh_offset);
    (void)shstrtab;

    /* 1. Calculate layout & memory needed for all SHF_ALLOC sections */
    size_t total_alloc = 0;
    for (uint16_t i = 0; i < eh->e_shnum; i++) {
        const Elf64_Shdr *sh = &sh_table[i];
        if (sh->sh_flags & SHF_ALLOC) {
            uintptr_t align = sh->sh_addralign ? sh->sh_addralign : 8;
            total_alloc = align_up(total_alloc, align);
            total_alloc += sh->sh_size;
        }
    }

    if (total_alloc == 0) {
        return -3;
    }

    /* Allocate executable memory */
    uint8_t *load_mem = (uint8_t *)kzalloc(total_alloc + 64, GFP_KERNEL);
    if (!load_mem) {
        return -4;
    }
    uint8_t *sec_addrs[eh->e_shnum];
    memset(sec_addrs, 0, sizeof(sec_addrs));

    /* 2. Place sections into allocated memory */
    uintptr_t cur_offset = 0;
    for (uint16_t i = 0; i < eh->e_shnum; i++) {
        const Elf64_Shdr *sh = &sh_table[i];
        if (sh->sh_flags & SHF_ALLOC) {
            uintptr_t align = sh->sh_addralign ? sh->sh_addralign : 8;
            cur_offset = align_up(cur_offset, align);
            sec_addrs[i] = load_mem + cur_offset;

            if (sh->sh_type != SHT_NOBITS) {
                if (sh->sh_offset + sh->sh_size <= size) {
                    memcpy(sec_addrs[i], raw + sh->sh_offset, sh->sh_size);
                }
            } else {
                memset(sec_addrs[i], 0, sh->sh_size);
            }
            cur_offset += sh->sh_size;
        }
    }

    /* 3. Find Symbol Table and String Table */
    const Elf64_Shdr *symtab_sh = NULL;
    const Elf64_Shdr *strtab_sh = NULL;
    for (uint16_t i = 0; i < eh->e_shnum; i++) {
        if (sh_table[i].sh_type == SHT_SYMTAB) {
            symtab_sh = &sh_table[i];
            if (symtab_sh->sh_link < eh->e_shnum) {
                strtab_sh = &sh_table[symtab_sh->sh_link];
            }
            break;
        }
    }

    if (!symtab_sh || !strtab_sh) {
        kfree(load_mem);
        return -5;
    }

    const Elf64_Sym *syms = (const Elf64_Sym *)(raw + symtab_sh->sh_offset);
    uint32_t num_syms = symtab_sh->sh_size / sizeof(Elf64_Sym);
    const char *strtab = (const char *)(raw + strtab_sh->sh_offset);

    /* Allocate symbol values array */
    uintptr_t *sym_values = (uintptr_t *)kzalloc(num_syms * sizeof(uintptr_t), GFP_KERNEL);
    if (!sym_values) {
        kfree(load_mem);
        return -6;
    }

    /* Count potential exported symbols */
    size_t export_count = 0;

    /* 4. Resolve symbols */
    for (uint32_t i = 1; i < num_syms; i++) {
        const Elf64_Sym *sym = &syms[i];
        const char *sname = strtab + sym->st_name;

        if (sym->st_shndx == SHN_UNDEF) {
            /* Undefined symbol: resolve from kernel or other modules */
            uintptr_t resolved = cherry_kmod_lookup_symbol(sname);
            if (!resolved) {
                printk("kmod: undefined symbol '%s' in module '%s'\n", sname, mod->name);
                kfree(sym_values);
                kfree(load_mem);
                return -7;
            }
            sym_values[i] = resolved;
        } else if (sym->st_shndx < eh->e_shnum) {
            /* Defined in this module */
            if (sec_addrs[sym->st_shndx]) {
                sym_values[i] = (uintptr_t)sec_addrs[sym->st_shndx] + sym->st_value;
            } else {
                sym_values[i] = sym->st_value;
            }

            /* Detect entry points */
            if (strcmp(sname, "init_module") == 0 || strcmp(sname, "module_init") == 0) {
                mod->init = (kmod_init_fn)sym_values[i];
            } else if (strcmp(sname, "cleanup_module") == 0 || strcmp(sname, "module_exit") == 0) {
                mod->exit = (kmod_exit_fn)sym_values[i];
            }

            /* If global function or object, record for export */
            if (ELF64_ST_BIND(sym->st_info) == STB_GLOBAL && sym->st_name != 0) {
                export_count++;
            }
        } else if (sym->st_shndx == SHN_ABS) {
            sym_values[i] = sym->st_value;
        }
    }

    /* 5. Apply Relocations */
    for (uint16_t i = 0; i < eh->e_shnum; i++) {
        const Elf64_Shdr *sh = &sh_table[i];
        if (sh->sh_type != SHT_RELA) {
            continue;
        }

        uint16_t target_sec_idx = sh->sh_info;
        if (target_sec_idx >= eh->e_shnum || !sec_addrs[target_sec_idx]) {
            continue; /* Target section not loaded */
        }

        uint8_t *target_sec = sec_addrs[target_sec_idx];
        const Elf64_Rela *relas = (const Elf64_Rela *)(raw + sh->sh_offset);
        uint32_t num_relas = sh->sh_size / sizeof(Elf64_Rela);

        for (uint32_t r = 0; r < num_relas; r++) {
            const Elf64_Rela *rela = &relas[r];
            uint32_t sym_idx = ELF64_R_SYM(rela->r_info);
            uint32_t type = ELF64_R_TYPE(rela->r_info);

            if (sym_idx >= num_syms) {
                continue;
            }

            uintptr_t P = (uintptr_t)target_sec + rela->r_offset;
            uintptr_t S = sym_values[sym_idx];
            int64_t   A = rela->r_addend;

            switch (type) {
                case R_AARCH64_NONE:
                    break;

                case R_AARCH64_ABS64:
                    *(uint64_t *)P = (uint64_t)(S + A);
                    break;

                case R_AARCH64_ABS32:
                    *(uint32_t *)P = (uint32_t)(S + A);
                    break;

                case R_AARCH64_ABS16:
                    *(uint16_t *)P = (uint16_t)(S + A);
                    break;

                case R_AARCH64_PREL64:
                    *(uint64_t *)P = (uint64_t)(S + A - P);
                    break;

                case R_AARCH64_PREL32:
                    *(uint32_t *)P = (uint32_t)(S + A - P);
                    break;

                case R_AARCH64_PREL16:
                    *(uint16_t *)P = (uint16_t)(S + A - P);
                    break;

                case R_AARCH64_ADR_PREL_PG_HI21: {
                    uint64_t pg_p  = P & ~0xFFFULL;
                    uint64_t pg_sa = (S + A) & ~0xFFFULL;
                    int64_t diff = ((int64_t)(pg_sa - pg_p)) >> 12;
                    uint32_t immlo = (uint32_t)(diff & 3) << 29;
                    uint32_t immhi = (uint32_t)((diff >> 2) & 0x7FFFF) << 5;
                    uint32_t insn = *(uint32_t *)P;
                    insn = (insn & ~((3U << 29) | (0x7FFFFU << 5))) | immlo | immhi;
                    *(uint32_t *)P = insn;
                    break;
                }

                case R_AARCH64_ADD_ABS_LO12_NC: {
                    uint32_t imm12 = (uint32_t)((S + A) & 0xFFF);
                    uint32_t insn = *(uint32_t *)P;
                    insn = (insn & ~(0xFFFU << 10)) | (imm12 << 10);
                    *(uint32_t *)P = insn;
                    break;
                }

                case R_AARCH64_LDST8_ABS_LO12_NC: {
                    uint32_t imm12 = (uint32_t)((S + A) & 0xFFF);
                    uint32_t insn = *(uint32_t *)P;
                    insn = (insn & ~(0xFFFU << 10)) | (imm12 << 10);
                    *(uint32_t *)P = insn;
                    break;
                }

                case R_AARCH64_LDST16_ABS_LO12_NC: {
                    uint32_t imm12 = (uint32_t)(((S + A) & 0xFFE) >> 1);
                    uint32_t insn = *(uint32_t *)P;
                    insn = (insn & ~(0xFFFU << 10)) | (imm12 << 10);
                    *(uint32_t *)P = insn;
                    break;
                }

                case R_AARCH64_LDST32_ABS_LO12_NC: {
                    uint32_t imm12 = (uint32_t)(((S + A) & 0xFFC) >> 2);
                    uint32_t insn = *(uint32_t *)P;
                    insn = (insn & ~(0xFFFU << 10)) | (imm12 << 10);
                    *(uint32_t *)P = insn;
                    break;
                }

                case R_AARCH64_LDST64_ABS_LO12_NC: {
                    uint32_t imm12 = (uint32_t)(((S + A) & 0xFF8) >> 3);
                    uint32_t insn = *(uint32_t *)P;
                    insn = (insn & ~(0xFFFU << 10)) | (imm12 << 10);
                    *(uint32_t *)P = insn;
                    break;
                }

                case R_AARCH64_LDST128_ABS_LO12_NC: {
                    uint32_t imm12 = (uint32_t)(((S + A) & 0xFF0) >> 4);
                    uint32_t insn = *(uint32_t *)P;
                    insn = (insn & ~(0xFFFU << 10)) | (imm12 << 10);
                    *(uint32_t *)P = insn;
                    break;
                }

                case R_AARCH64_JUMP26:
                case R_AARCH64_CALL26: {
                    int64_t diff = ((int64_t)(S + A - P)) >> 2;
                    uint32_t imm26 = (uint32_t)(diff & 0x03FFFFFF);
                    uint32_t insn = *(uint32_t *)P;
                    insn = (insn & ~0x03FFFFFFU) | imm26;
                    *(uint32_t *)P = insn;
                    break;
                }

                default:
                    printk("kmod: unsupported relocation type %u in '%s'\n", type, mod->name);
                    break;
            }
        }
    }

    /* 6. Flush / invalidate CPU instruction caches across loaded code */
    kmod_flush_icache(load_mem, total_alloc);

    /* 7. Store module fields */
    mod->load_addr = load_mem;
    mod->load_size = total_alloc;

    /* 8. Export module's global symbols into kernel symbol table */
    if (export_count > 0) {
        mod->syms = (struct cherry_kmod_symbol *)kzalloc(export_count * sizeof(struct cherry_kmod_symbol), GFP_KERNEL);
        if (mod->syms) {
            size_t idx = 0;
            for (uint32_t i = 1; i < num_syms && idx < export_count; i++) {
                const Elf64_Sym *sym = &syms[i];
                if (sym->st_shndx != SHN_UNDEF && sym->st_shndx < eh->e_shnum &&
                    ELF64_ST_BIND(sym->st_info) == STB_GLOBAL && sym->st_name != 0) {
                    const char *sname = strtab + sym->st_name;
                    mod->syms[idx].name = sname;
                    mod->syms[idx].addr = sym_values[i];
                    cherry_kmod_export_symbol(sname, sym_values[i]);
                    idx++;
                }
            }
            mod->num_syms = idx;
        }
    }

    kfree(sym_values);
    return 0;
}
