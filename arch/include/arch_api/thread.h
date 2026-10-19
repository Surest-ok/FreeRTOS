#ifndef ARCH_THREAD_H
#define ARCH_THREAD_H

#include <stdint.h>

typedef enum {
    ARCH_KERNEL_FREERTOS = 1,
    ARCH_KERNEL_LK = 2,
    ARCH_KERNEL_THREADX = 3,
} arch_kernel_type_t;

typedef struct arch_thread_handle {
    arch_kernel_type_t kernel_type;
    void *native_handle;
    void *context;
    void *stack_base;
    unsigned long stack_size;
    void *entry;
    void *arg;
    uint32_t affinity_mask;
    int32_t current_cpu;
    const char *name;
} arch_thread_handle_t;

static inline void arch_thread_handle_init(arch_thread_handle_t *handle,
                                           arch_kernel_type_t kernel_type,
                                           void *native_handle,
                                           void *context,
                                           void *stack_base,
                                           unsigned long stack_size,
                                           void *entry,
                                           void *arg,
                                           uint32_t affinity_mask,
                                           int32_t current_cpu,
                                           const char *name)
{
    handle->kernel_type = kernel_type;
    handle->native_handle = native_handle;
    handle->context = context;
    handle->stack_base = stack_base;
    handle->stack_size = stack_size;
    handle->entry = entry;
    handle->arg = arg;
    handle->affinity_mask = affinity_mask;
    handle->current_cpu = current_cpu;
    handle->name = name;
}

static inline void arch_thread_handle_set_context(arch_thread_handle_t *handle,
                                                  void *context)
{
    handle->context = context;
}

static inline void arch_thread_handle_set_affinity(arch_thread_handle_t *handle,
                                                   uint32_t affinity_mask)
{
    handle->affinity_mask = affinity_mask;
}

static inline void arch_thread_handle_set_running(arch_thread_handle_t *handle,
                                                  int32_t current_cpu)
{
    handle->current_cpu = current_cpu;
}

static inline void arch_thread_handle_switch(arch_thread_handle_t *old_thread,
                                             arch_thread_handle_t *new_thread,
                                             int32_t current_cpu)
{
    if (old_thread != 0) {
        arch_thread_handle_set_running(old_thread, -1);
    }

    if (new_thread != 0) {
        arch_thread_handle_set_running(new_thread, current_cpu);
    }
}

#endif /* ARCH_THREAD_H */
