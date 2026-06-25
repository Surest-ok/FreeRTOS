#define TX_SOURCE_CODE
#define TX_THREAD_SMP_SOURCE_CODE

#include "k_api.h"
#include "k_thread.h"
#include "k_timer.h"

#include "arch_api/arch.h"
#include "arch_api/barrier.h"
#include "arch_api/ipi.h"
#include "arch_api/irq.h"
#include "arch_api/stack.h"

#define THREADX_INVALID_CORE 0xffffffffUL
#define THREADX_TCB_READY_BIT_OFFSET 260U
#define THREADX_PROTECT_IN_FORCE 0U
#define THREADX_PROTECT_CORE     1U
#define THREADX_PROTECT_COUNT    2U

static volatile uint32_t *threadx_protect_slots(void)
{
    return (volatile uint32_t *)&_k_thread_smp_protection;
}

static uint32_t threadx_wait_list_size(void)
{
    uint32_t size = __atomic_load_n(&_k_thread_smp_protect_wait_list_size,
                                    __ATOMIC_ACQUIRE);

    if ((size == 0U) || (size > TX_THREAD_SMP_PROTECT_WAIT_LIST_SIZE)) {
        size = TX_THREAD_SMP_PROTECT_WAIT_LIST_SIZE;
        __atomic_store_n(&_k_thread_smp_protect_wait_list_size,
                         size,
                         __ATOMIC_RELEASE);
    }

    return size;
}

static uint32_t threadx_wait_list_next(uint32_t index)
{
    uint32_t size = threadx_wait_list_size();

    index++;
    if (index >= size) {
        index = 0U;
    }

    return index;
}

static void threadx_wait_list_lock(void)
{
    uint32_t expected;

    for (;;) {
        expected = 0U;
        if (__atomic_compare_exchange_n(&_k_thread_smp_protect_wait_list_lock_protect_in_force,
                                        &expected,
                                        1U,
                                        0,
                                        __ATOMIC_ACQUIRE,
                                        __ATOMIC_RELAXED)) {
            return;
        }
        arch_cpu_relax();
    }
}

static void threadx_wait_list_unlock(void)
{
    arch_smp_mb();
    __atomic_store_n(&_k_thread_smp_protect_wait_list_lock_protect_in_force,
                     0U,
                     __ATOMIC_RELEASE);
}

static uint32_t threadx_wait_list_front(void)
{
    uint32_t head = __atomic_load_n(&_k_thread_smp_protect_wait_list_head,
                                    __ATOMIC_ACQUIRE);
    uint32_t tail = __atomic_load_n(&_k_thread_smp_protect_wait_list_tail,
                                    __ATOMIC_ACQUIRE);

    if (head == tail) {
        return (uint32_t)THREADX_INVALID_CORE;
    }

    return __atomic_load_n(&_k_thread_smp_protect_wait_list[head],
                           __ATOMIC_ACQUIRE);
}

static void threadx_wait_list_enqueue(uint32_t core)
{
    uint32_t tail;
    uint32_t next_tail;

    if (__atomic_load_n(&_k_thread_smp_protect_wait_counts[core],
                        __ATOMIC_ACQUIRE) != 0U) {
        return;
    }

    tail = __atomic_load_n(&_k_thread_smp_protect_wait_list_tail,
                           __ATOMIC_ACQUIRE);
    next_tail = threadx_wait_list_next(tail);
    if (next_tail == __atomic_load_n(&_k_thread_smp_protect_wait_list_head,
                                     __ATOMIC_ACQUIRE)) {
        return;
    }

    __atomic_store_n(&_k_thread_smp_protect_wait_list[tail],
                     core,
                     __ATOMIC_RELEASE);
    __atomic_store_n(&_k_thread_smp_protect_wait_counts[core],
                     1U,
                     __ATOMIC_RELEASE);
    __atomic_store_n(&_k_thread_smp_protect_wait_list_tail,
                     next_tail,
                     __ATOMIC_RELEASE);
}

static void threadx_wait_list_pop_front(uint32_t core)
{
    uint32_t head = __atomic_load_n(&_k_thread_smp_protect_wait_list_head,
                                    __ATOMIC_ACQUIRE);

    __atomic_store_n(&_k_thread_smp_protect_wait_list[head],
                     (uint32_t)THREADX_INVALID_CORE,
                     __ATOMIC_RELEASE);
    __atomic_store_n(&_k_thread_smp_protect_wait_counts[core],
                     0U,
                     __ATOMIC_RELEASE);
    __atomic_store_n(&_k_thread_smp_protect_wait_list_head,
                     threadx_wait_list_next(head),
                     __ATOMIC_RELEASE);
}

uint32_t _k_thread_smp_core_get(void)
{
    return (uint32_t)arch_curr_cpu_id();
}

uint32_t _k_thread_interrupt_disable(void)
{
    return (uint32_t)arch_irq_save_all();
}

uint32_t _k_thread_interrupt_restore(uint32_t old_posture)
{
    arch_irq_restore_all((arch_irq_flags_t)old_posture);
    return old_posture;
}

uint32_t _k_thread_interrupt_control(uint32_t new_posture)
{
    arch_irq_flags_t old_posture = arch_irq_save_all();
    arch_irq_restore_all((arch_irq_flags_t)new_posture);
    return (uint32_t)old_posture;
}

uint32_t _k_thread_smp_time_get(void)
{
    return _k_timer_system_clock;
}

uint32_t _k_thread_smp_current_state_get(void)
{
    arch_irq_flags_t flags = arch_irq_save_all();
    uint32_t core = (uint32_t)arch_curr_cpu_id();
    uint32_t state = _k_thread_system_state[core];

    arch_irq_restore_all(flags);
    return state;
}

k_thread_t *_k_thread_smp_current_thread_get(void)
{
    arch_irq_flags_t flags = arch_irq_save_all();
    uint32_t core = (uint32_t)arch_curr_cpu_id();
    k_thread_t *thread = _k_thread_current_ptr[core];

    arch_irq_restore_all(flags);
    return thread;
}

void _k_thread_smp_low_level_initialize(uint32_t number_of_cores)
{
    (void)number_of_cores;
}

void _k_thread_smp_initialize_wait(void)
{
    uint32_t core;
    unsigned long sp;

    (void)arch_irq_save_all();
    core = (uint32_t)arch_curr_cpu_id();

    while (_k_thread_system_state[core] != TX_INITIALIZE_IN_PROGRESS) {
        arch_cpu_relax();
    }

    sp = arch_stack_align_down(arch_current_stack_pointer() - 15U);
    _k_thread_system_stack_ptr[core] = (void *)sp;

    while (_k_thread_smp_release_cores_flag == TX_FALSE) {
        arch_cpu_relax();
    }

    _k_thread_system_state[core] = TX_INITIALIZE_IS_FINISHED;

    while (_k_thread_system_state[0] != TX_INITIALIZE_IS_FINISHED) {
        arch_cpu_relax();
    }

    _k_thread_schedule();

    for (;;) {
        arch_cpu_wait_for_interrupt();
    }
}

#ifdef THREADX_LOWLEVEL_C_CORE_PREEMPT
void _k_thread_smp_core_preempt(uint32_t core)
{
    arch_ipi_send_reschedule(core);
}
#endif

#ifdef THREADX_LOWLEVEL_C_TIMER_INTERRUPT
void _k_timer_interrupt(void)
{
    uint32_t old_posture;

    if (arch_curr_cpu_id() != 0U) {
        return;
    }

    old_posture = _k_thread_smp_protect();

    _k_timer_system_clock++;

    if (*_k_timer_current_ptr != TX_NULL) {
        _k_timer_expired = TX_TRUE;
    } else {
        _k_timer_current_ptr++;
        if (_k_timer_current_ptr == _k_timer_list_end) {
            _k_timer_current_ptr = _k_timer_list_start;
        }
    }

    if (_k_timer_expired != TX_FALSE) {
        _k_timer_expiration_process();
    }

    _k_thread_time_slice();
    _k_thread_smp_unprotect(old_posture);
}
#endif

uint32_t _k_thread_smp_protect(void)
{
    arch_irq_flags_t flags;
    uint32_t core;
    volatile uint32_t *protect = threadx_protect_slots();

    for (;;) {
        flags = arch_irq_save_all();
        core = (uint32_t)arch_curr_cpu_id();

        if (__atomic_load_n(&protect[THREADX_PROTECT_CORE], __ATOMIC_ACQUIRE) == core) {
            __atomic_fetch_add(&protect[THREADX_PROTECT_COUNT],
                               1U,
                               __ATOMIC_RELAXED);
            return (uint32_t)flags;
        }

        threadx_wait_list_lock();
        threadx_wait_list_enqueue(core);
        if ((threadx_wait_list_front() == core) &&
            (__atomic_load_n(&protect[THREADX_PROTECT_IN_FORCE],
                             __ATOMIC_ACQUIRE) == 0U)) {
            __atomic_store_n(&protect[THREADX_PROTECT_IN_FORCE],
                             1U,
                             __ATOMIC_RELEASE);
            __atomic_store_n(&protect[THREADX_PROTECT_CORE],
                             core,
                             __ATOMIC_RELEASE);
            __atomic_store_n(&protect[THREADX_PROTECT_COUNT],
                             1U,
                             __ATOMIC_RELEASE);
            threadx_wait_list_pop_front(core);
            threadx_wait_list_unlock();
            arch_smp_mb();
            return (uint32_t)flags;
        }
        threadx_wait_list_unlock();

        arch_irq_restore_all(flags);
        arch_cpu_relax();
    }
}

void _k_thread_smp_unprotect(uint32_t old_posture)
{
    uint32_t core;
    volatile uint32_t *protect = threadx_protect_slots();

    (void)arch_irq_save_all();
    core = (uint32_t)arch_curr_cpu_id();

    if (__atomic_load_n(&protect[THREADX_PROTECT_CORE], __ATOMIC_ACQUIRE) == core) {
        uint32_t count = __atomic_load_n(&protect[THREADX_PROTECT_COUNT], __ATOMIC_ACQUIRE);
        if (count > 0U) {
            count = __atomic_sub_fetch(&protect[THREADX_PROTECT_COUNT],
                                       1U,
                                       __ATOMIC_RELEASE);
            if ((count == 0U) &&
                (__atomic_load_n(&_k_thread_preempt_disable, __ATOMIC_ACQUIRE) == 0U)) {
                __atomic_store_n(&protect[THREADX_PROTECT_CORE],
                                 (uint32_t)THREADX_INVALID_CORE,
                                 __ATOMIC_RELEASE);
                arch_smp_mb();
                __atomic_store_n(&protect[THREADX_PROTECT_IN_FORCE],
                                 0U,
                                 __ATOMIC_RELEASE);
                arch_mb();
                arch_cpu_send_event();
            }
        }
    }

    arch_irq_restore_all((arch_irq_flags_t)old_posture);
}

#ifdef THREADX_LOWLEVEL_C_STACK_BUILD
void _k_thread_stack_build(k_thread_t *thread_ptr, void (*function_ptr)(void))
{
    ALIGN_TYPE *sp;
    ALIGN_TYPE *stack_end;
    unsigned int i;

    stack_end = (ALIGN_TYPE *)arch_stack_align_down((unsigned long)thread_ptr->tx_thread_stack_end);
    sp = stack_end;

    for (i = 0; i < 17U; ++i) {
        *--sp = 0U;
        *--sp = 0U;
    }

    *--sp = (ALIGN_TYPE)function_ptr;
    *--sp = 0x5U;

    thread_ptr->tx_thread_stack_ptr = sp;
    *(volatile uint32_t *)((uint8_t *)thread_ptr + THREADX_TCB_READY_BIT_OFFSET) = 1U;
}
#endif
