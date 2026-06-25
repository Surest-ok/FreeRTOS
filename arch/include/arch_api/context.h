#ifndef ARCH_CONTEXT_H
#define ARCH_CONTEXT_H

#include "arch_api/thread.h"

typedef void (*arch_thread_entry_t)(void *);

void arch_thread_context_init(arch_thread_handle_t *thread,
                              void *stack_base,
                              unsigned long stack_size,
                              arch_thread_entry_t entry,
                              void *arg);
void arch_thread_context_switch(arch_thread_handle_t *old_thread,
                                arch_thread_handle_t *new_thread);

#endif /* ARCH_CONTEXT_H */
