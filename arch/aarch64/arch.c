#include "arch_api/context.h"
#include "arch_api/exception.h"
#include "arch_api/irq.h"
#include "arch_api/syscall.h"
#include "arch_api/vector.h"

#define ARCH_IRQ_MAX_VECTORS 1024U
#define ARCH_LPI_BASE        8192U
#define ARCH_LPI_MAX_VECTORS 256U

typedef struct arch_irq_entry {
    arch_irq_handler_t handler;
    void *arg;
} arch_irq_entry_t;

static arch_syscall_handler_t g_syscall_handler;
static arch_irq_entry_t g_irq_table[ARCH_IRQ_MAX_VECTORS];
static arch_irq_entry_t g_lpi_table[ARCH_LPI_MAX_VECTORS];
static arch_irq_stats_t g_irq_stats;
static arch_exception_stats_t g_exception_stats;

void arch_thread_context_init(arch_thread_handle_t *thread,
                              void *stack_base,
                              unsigned long stack_size,
                              arch_thread_entry_t entry,
                              void *arg)
{
    (void)thread;
    (void)stack_base;
    (void)stack_size;
    (void)entry;
    (void)arg;
}

void arch_thread_context_switch(arch_thread_handle_t *old_thread,
                                arch_thread_handle_t *new_thread)
{
    (void)old_thread;
    (void)new_thread;
}

void arch_exception_dispatch(arch_exception_frame_t *frame)
{
    arch_exception_record(frame);
    (void)arch_syscall_dispatch(frame);
}

void arch_exception_record(const arch_exception_frame_t *frame)
{
    if (frame != 0) {
        __atomic_fetch_add(&g_exception_stats.count, 1U, __ATOMIC_RELAXED);
        __atomic_store_n(&g_exception_stats.last_elr, frame->elr, __ATOMIC_RELAXED);
        __atomic_store_n(&g_exception_stats.last_spsr, frame->spsr, __ATOMIC_RELAXED);
        __atomic_store_n(&g_exception_stats.last_esr, frame->esr, __ATOMIC_RELAXED);
        __atomic_store_n(&g_exception_stats.last_far, frame->far, __ATOMIC_RELAXED);
    }
}

const arch_exception_stats_t *arch_exception_get_stats(void)
{
    return &g_exception_stats;
}

void arch_vector_install(void *table)
{
    uint64_t current_el;
    __asm__ volatile("mrs %0, CurrentEL" : "=r"(current_el));
    if (((current_el >> 2) & 0x3) == 2) {
        __asm__ volatile("msr vbar_el2, %0\nisb" :: "r"(table) : "memory");
    } else {
        __asm__ volatile("msr vbar_el1, %0\nisb" :: "r"(table) : "memory");
    }
}

void arch_syscall_set_handler(arch_syscall_handler_t handler)
{
    g_syscall_handler = handler;
}

int arch_syscall_dispatch(arch_exception_frame_t *frame)
{
    if (g_syscall_handler != 0) {
        return g_syscall_handler(frame);
    }
    return -1;
}

int arch_irq_register(uint32_t vector, arch_irq_handler_t handler, void *arg)
{
    if (vector < ARCH_IRQ_MAX_VECTORS) {
        g_irq_table[vector].handler = handler;
        g_irq_table[vector].arg = arg;
        return 0;
    }
    if (vector >= ARCH_LPI_BASE && vector < (ARCH_LPI_BASE + ARCH_LPI_MAX_VECTORS)) {
        uint32_t idx = vector - ARCH_LPI_BASE;
        g_lpi_table[idx].handler = handler;
        g_lpi_table[idx].arg = arg;
        return 0;
    }
    return -1;
}

int arch_irq_dispatch(uint32_t vector)
{
    arch_irq_entry_t *entry;

    if (vector < ARCH_IRQ_MAX_VECTORS) {
        entry = &g_irq_table[vector];
        if (entry->handler == 0) {
            return 0;
        }
        return entry->handler(vector, entry->arg) != 0;
    }
    if (vector >= ARCH_LPI_BASE && vector < (ARCH_LPI_BASE + ARCH_LPI_MAX_VECTORS)) {
        uint32_t idx = vector - ARCH_LPI_BASE;
        entry = &g_lpi_table[idx];
        if (entry->handler == 0) {
            return 0;
        }
        return entry->handler(vector, entry->arg) != 0;
    }
    return 0;
}

void arch_irq_note_enter(uint32_t vector, uint32_t cpu)
{
    __atomic_fetch_add(&g_irq_stats.entries, 1U, __ATOMIC_RELAXED);
    __atomic_store_n(&g_irq_stats.last_vector, vector, __ATOMIC_RELAXED);
    __atomic_store_n(&g_irq_stats.last_cpu, cpu, __ATOMIC_RELAXED);
}

void arch_irq_note_exit(uint32_t vector, uint32_t cpu, int handled)
{
    (void)vector;
    (void)cpu;

    __atomic_fetch_add(&g_irq_stats.exits, 1U, __ATOMIC_RELAXED);
    if (handled != 0) {
        __atomic_fetch_add(&g_irq_stats.handled, 1U, __ATOMIC_RELAXED);
    } else {
        __atomic_fetch_add(&g_irq_stats.unhandled, 1U, __ATOMIC_RELAXED);
    }
}

const arch_irq_stats_t *arch_irq_get_stats(void)
{
    return &g_irq_stats;
}
