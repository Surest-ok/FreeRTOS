#ifndef WORKSPACE_THREADX_ARCH_H
#define WORKSPACE_THREADX_ARCH_H

struct tx_thread_struct;
typedef struct tx_thread_struct k_thread_t;

void threadx_arch_thread_create(k_thread_t *thread);
void threadx_arch_thread_sync_affinity(k_thread_t *thread);
void threadx_arch_thread_sync_running(k_thread_t *thread, unsigned int core);

#endif
