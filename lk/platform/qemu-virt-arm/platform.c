/*
 * Copyright (c) 2012-2015 Travis Geiselbrecht
 *
 * Use of this source code is governed by a MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT
 */
#include <sys/types.h>
#include <platform/qemu-virt.h>

#include <arch.h>
#include <arch/ops.h>
#include <lk/err.h>
#include <lk/debug.h>
#include <lk/trace.h>
#include <dev/interrupt/arm_gicv3.h>
#include <dev/power/psci.h>
#include <dev/timer/arm_generic.h>
#include <dev/uart/pl011.h>
#include <lk/init.h>
#include <kernel/vm.h>
#include <platform.h>
#include <platform/gic.h>
#include <platform/interrupts.h>
#include <arch_api/arch.h>

#define LOCAL_TRACE 0

#define DEFAULT_MEMORY_SIZE (MEMSIZE) /* try to fetch from the emulator via the fdt */

/* initial memory mappings. parsed by start.S */
struct mmu_initial_mapping mmu_initial_mappings[] = {
    /* all of memory */
    {
        .phys = MEMORY_BASE_PHYS,
        .virt = KERNEL_BASE,
        .size = MEMORY_APERTURE_SIZE,
        .flags = 0,
        .name = "memory"
    },

    /* 1GB of peripherals */
    {
        .phys = PERIPHERAL_BASE_PHYS,
        .virt = PERIPHERAL_BASE_VIRT,
        .size = PERIPHERAL_BASE_SIZE,
        .flags = MMU_INITIAL_MAPPING_FLAG_DEVICE,
        .name = "peripherals"
    },

    /* null entry to terminate the list */
    { 0 }
};

/* Forward declaration */
static void platform_init_memory(uint level);

void platform_early_init(void) {
    const struct pl011_config uart_config = {
        .base = UART_BASE,
        .irq = UART0_INT,
        .flag = PL011_FLAG_DEBUG_UART,
    };

    pl011_init_early(0, &uart_config);

    dprintf(ALWAYS, "LK starting...\n");

    /* initialize the interrupt controller */
    arm_gicv3_init();

    arm_generic_timer_init(ARM_GENERIC_TIMER_VIRTUAL_INT, 0);
}

void platform_init(void) {
    pl011_init(0);

#if WITH_SMP
    /* Print boot CPU MPIDR for reference */
    uint64_t boot_mpidr;
    __asm__ volatile("mrs %0, mpidr_el1" : "=r" (boot_mpidr));
    dprintf(ALWAYS, "SMP: Boot CPU MPIDR: 0x%lx\n", (ulong)boot_mpidr);
    /* TODO: SMP support is partially implemented but not fully working
     * Secondary CPUs can be started via PSCI, but there are issues with:
     * - percpu structure initialization
     * - Secondary CPU scheduler initialization
     * - Possible MMU/page table setup for secondary CPUs
     *
     * Current status: All threads run on CPU 0, which works correctly
     * for demonstrating threading, synchronization, and scheduling.
     *
     * Uncommenting the code below will attempt to start secondary CPUs,
     * but may cause faults during initialization.
     */
    dprintf(ALWAYS, "SMP: %d CPUs configured, attempting secondary CPU boot\n", SMP_MAX_CPUS);

#if 1  // Enable secondary CPU boot for debugging
    /* Initialize secondary CPU count and allocate percpu structures */
    extern void arm64_set_secondary_cpu_count(int count);
    dprintf(ALWAYS, "SMP: Allocating percpu structures for %d secondary CPUs\n", SMP_MAX_CPUS - 1);
    arm64_set_secondary_cpu_count(SMP_MAX_CPUS - 1);
    dprintf(ALWAYS, "SMP: Percpu structures allocated\n");

    /* Initialize secondary CPU threads (idle and bootstrap) */
    extern void lk_init_secondary_cpus(uint secondary_cpu_count);
    dprintf(ALWAYS, "SMP: Creating idle and bootstrap threads for secondary CPUs\n");
    lk_init_secondary_cpus(SMP_MAX_CPUS - 1);
    dprintf(ALWAYS, "SMP: Secondary CPU threads initialized\n");

    /* Start secondary CPUs using PSCI */
    extern void arm64_secondary_phys_entry(void);
    paddr_t virt_entry = (paddr_t)&arm64_secondary_phys_entry;
    paddr_t phys_entry = (virt_entry - KERNEL_BASE) + MEMORY_BASE_PHYS;

    dprintf(ALWAYS, "SMP: Starting secondary CPUs via PSCI\n");
    dprintf(ALWAYS, "SMP: Secondary entry point: virt=0x%lx phys=0x%lx\n", (ulong)virt_entry, (ulong)phys_entry);

    for (uint cpu = 1; cpu < SMP_MAX_CPUS; cpu++) {
        ulong mpidr = arch_linear_cpu_to_mpidr(cpu);
        dprintf(ALWAYS, "SMP: Starting CPU %u with MPIDR 0x%lx...\n", cpu, mpidr);
        int ret = psci_cpu_on(mpidr, phys_entry, cpu);
        if (ret != 0) {
            dprintf(ALWAYS, "SMP: Failed to start CPU %u: error %d\n", cpu, ret);
        } else {
            dprintf(ALWAYS, "SMP: CPU %u started successfully (PSCI returned 0)\n", cpu);
        }
        /* Give CPU some time to boot */
        for (volatile int i = 0; i < 10000000; i++);
    }
    dprintf(ALWAYS, "SMP: Requested %d secondary CPUs to start\n", SMP_MAX_CPUS - 1);
#endif // Disabled secondary CPU boot
#endif // WITH_SMP
}


void platform_halt_default(platform_halt_action suggested_action,
                           platform_halt_reason reason,
                           void (*reset_func)(void),
                           void (*off_func)(void)) {
    dprintf(ALWAYS, "platform_halt: action %d, reason %d\n", suggested_action, reason);

    switch (suggested_action) {
        case HALT_ACTION_SHUTDOWN:
            if (off_func) {
                dprintf(ALWAYS, "Shutting down...\n");
                off_func();
            }
            break;
        case HALT_ACTION_REBOOT:
            if (reset_func) {
                dprintf(ALWAYS, "Rebooting...\n");
                reset_func();
            }
            break;
        case HALT_ACTION_HALT:
        default:
            dprintf(ALWAYS, "Halting...\n");
            break;
    }

    // If we get here, halt/shutdown/reboot didn't work, so just loop forever
    for (;;) {
        arch_idle();
    }
}

void platform_halt(platform_halt_action suggested_action, platform_halt_reason reason) {
    // Use the default halt implementation using psci as the reset and shutdown implementation.
    platform_halt_default(suggested_action, reason, &psci_system_reset, &psci_system_off);
}

void platform_set_periodic_timer(enum handler_return (*callback)(void *arg, lk_time_t now), void *arg, lk_time_t interval) {
    // For now, do nothing - the platform uses dynamic timer through arm_generic_timer
}

void platform_init_mmu_mappings(void) {
    // MMU mappings are defined in mmu_initial_mappings[] above
    // Nothing to do here - the MMU code will read that table
}

/* Add physical memory arena for heap allocation */
static pmm_arena_t heap_arena = {
    .name = "ram",
    .base = MEMORY_BASE_PHYS,
    .size = MEMSIZE,
    .priority = 1,
    .flags = PMM_ARENA_FLAG_KMAP,
};

static void platform_init_memory(uint level) {
    /* Add the main memory arena to the PMM */
    dprintf(ALWAYS, "Adding PMM arena: base=0x%lx, size=0x%zx\n",
            heap_arena.base, heap_arena.size);
    status_t result = pmm_add_arena(&heap_arena);
    dprintf(ALWAYS, "PMM arena add result: %d, free_count=%zu\n",
            result, heap_arena.free_count);
}

/* Register before heap init - at target early level */
LK_INIT_HOOK(platform_mem, &platform_init_memory, LK_INIT_LEVEL_TARGET_EARLY);
