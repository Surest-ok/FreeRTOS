# CherryKmod - Dynamic Kernel Module Subsystem for FreeRTOS AArch64

`CherryKmod` is a lightweight, high-performance loadable kernel module subsystem designed for the FreeRTOS AArch64 SMP Unikernel environment. It provides full ELF64 relocatable object (`.ko` / `ET_REL`) loading, dynamic relocation resolution, kernel symbol export management (`EXPORT_SYMBOL`), instruction cache coherency, and Linux-compatible shell utilities (`insmod`, `rmmod`, `lsmod`).

---

## 1. Key Features

1. **AArch64 Relocatable ELF64 Loader (`ET_REL`)**:
   - Parses standard AArch64 ELF object files compiled with `aarch64-linux-gnu-gcc -c -fno-pie`.
   - Allocates contiguous memory matching section alignment constraints (`sh_addralign`).
   - Copies `.text`, `.rodata`, `.data` and zeroes out `.bss` (`SHT_NOBITS`).
   - Computes symbols and entry points (`init_module`, `cleanup_module`).

2. **Complete AArch64 Relocation Engine**:
   - `R_AARCH64_ABS64` (257) & `R_AARCH64_ABS32` (258) & `R_AARCH64_ABS16` (259)
   - `R_AARCH64_PREL64` (260) & `R_AARCH64_PREL32` (261) & `R_AARCH64_PREL16` (262)
   - `R_AARCH64_ADR_PREL_PG_HI21` (275): PC-relative page addressing for `ADRP` instructions.
   - `R_AARCH64_ADD_ABS_LO12_NC` (277): Immediate 12-bit addend for `ADD` instructions.
   - `R_AARCH64_LDST8_ABS_LO12_NC` (278), `R_AARCH64_LDST16_ABS_LO12_NC` (284), `R_AARCH64_LDST32_ABS_LO12_NC` (285), `R_AARCH64_LDST64_ABS_LO12_NC` (286), `R_AARCH64_LDST128_ABS_LO12_NC` (299)
   - `R_AARCH64_JUMP26` (282) & `R_AARCH64_CALL26` (283): 26-bit PC-relative branches (`B` and `BL`).

3. **Multi-tier Kernel Symbol Export (`EXPORT_SYMBOL`)**:
   - Compile-time section export via `.ksymtab` (`EXPORT_SYMBOL(sym)`).
   - Dynamic kernel symbol hash table with O(1) average lookup and spinlock protection.
   - Fallback resolution into `CherryShell` exported symbol table.
   - Exported symbols from loaded modules become available to subsequently loaded modules (module dependencies).

4. **Architecture Cache Management**:
   - Automatically cleans data cache lines to Point of Unification (`dc cvau`) and invalidates instruction cache (`ic ivau`) across loaded module memory, followed by `dsb ish; isb`.

5. **Shell Command Integration**:
   - `insmod <path.ko>`: Loads module from VFS into kernel memory and runs `init_module()`.
   - `rmmod <name>`: Runs `cleanup_module()`, removes exported symbols, and reclaims memory.
   - `lsmod`: Displays loaded modules, size, and reference counts.

---

## 2. Kernel Module Example

```c
#include "cherry_kmod.h"

extern void printk(const char *fmt, ...);
extern void *kmalloc(size_t size, int flags);
extern void kfree(void *ptr);

static int g_counter = 100;

/* Function exported to the kernel and other modules */
int sample_calc(int a, int b)
{
    return a * 10 + b + g_counter;
}

int init_module(void)
{
    g_counter = 500;
    printk("[sample_mod] module loaded! counter=%d\n", g_counter);
    return 0;
}

void cleanup_module(void)
{
    printk("[sample_mod] module unloaded!\n");
    g_counter = 0;
}

MODULE_LICENSE("GPL");
```

---

## 3. Core API Reference

```c
/* Initialize kernel module subsystem */
int cherry_kmod_init(void);

/* Symbol export & lookup */
int cherry_kmod_export_symbol(const char *name, uintptr_t addr);
int cherry_kmod_unexport_symbol(const char *name);
uintptr_t cherry_kmod_lookup_symbol(const char *name);

/* Module lifecycle */
struct cherry_kmod *cherry_kmod_load_buffer(const void *buf, size_t size, const char *name);
struct cherry_kmod *cherry_kmod_load_file(const char *path);
int cherry_kmod_unload(const char *name);
struct cherry_kmod *cherry_kmod_find(const char *name);
int cherry_kmod_list(char *buf, size_t buf_len);
```

---

## 4. Verification & Testing

Run acceptance tests:
```bash
# Default EL1 Execution
scons test

# Native EL2 Hypervisor Execution
scons RUN_EL=2 test

# Cross-core Stress Tests
scons SCHED_MODE=cross test
scons RUN_EL=2 SCHED_MODE=cross test
```

Verification marker:
```
[kmod] [kmod-verify] init=pass syms=pass load_buf=pass exec_func=pass vfs_file=pass shell_cmds=pass unload=pass all=pass
```
