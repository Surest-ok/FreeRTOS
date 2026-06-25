#include "threadx_arch.h"

#include "k_api.h"
#include "arch_api/thread.h"

#ifndef THREADX_ARCH_MAX_HANDLES
#define THREADX_ARCH_MAX_HANDLES 128U
#endif

static arch_thread_handle_t thread_handles[THREADX_ARCH_MAX_HANDLES];
static unsigned int next_thread_handle;

static arch_thread_handle_t *threadx_arch_handle(k_thread_t *thread)
{
    return (arch_thread_handle_t *)thread->tx_arch_thread;
}

void threadx_arch_thread_create(k_thread_t *thread)
{
    arch_thread_handle_t *handle;

    if (thread == TX_NULL) {
        return;
    }

    if (next_thread_handle >= THREADX_ARCH_MAX_HANDLES) {
        thread->tx_arch_thread = TX_NULL;
        return;
    }

    handle = &thread_handles[next_thread_handle++];
    thread->tx_arch_thread = handle;

    arch_thread_handle_init(handle,
                            ARCH_KERNEL_THREADX,
                            thread,
                            thread->tx_thread_stack_ptr,
                            thread->tx_thread_stack_start,
                            thread->tx_thread_stack_size,
                            (void *)thread->tx_thread_entry,
                            (void *)(ALIGN_TYPE)thread->tx_thread_entry_parameter,
                            thread->tx_thread_smp_cores_allowed,
                            -1,
                            (const char *)thread->tx_thread_name);
    arch_thread_handle_set_context(handle, thread->tx_thread_stack_ptr);
    arch_thread_handle_set_affinity(handle, thread->tx_thread_smp_cores_allowed);
    arch_thread_handle_set_running(handle, -1);
}

void threadx_arch_thread_sync_affinity(k_thread_t *thread)
{
    arch_thread_handle_t *handle;

    if (thread == TX_NULL) {
        return;
    }

    handle = threadx_arch_handle(thread);
    if (handle != TX_NULL) {
        arch_thread_handle_set_affinity(handle, thread->tx_thread_smp_cores_allowed);
    }
}

void threadx_arch_thread_sync_running(k_thread_t *thread, unsigned int core)
{
    arch_thread_handle_t *handle;

    if (thread == TX_NULL) {
        return;
    }

    handle = threadx_arch_handle(thread);
    if (handle != TX_NULL) {
        arch_thread_handle_set_context(handle, thread->tx_thread_stack_ptr);
        arch_thread_handle_set_running(handle, core);
    }
}
