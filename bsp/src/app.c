#include <stdint.h>
#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"
#include "queue.h"
#include "semphr.h"
#include "port_smp.h"
#include "uart.h"
#include "ukernel.h"

#define APP_WORKER_STACK_WORDS  256
#define APP_MONITOR_STACK_WORDS 512
#define APP_PROBE_STACK_WORDS   512
#define APP_PROBE_ITERATIONS    16U

#ifndef APP_ENABLE_CROSS_CORE_STRESS
#define APP_ENABLE_CROSS_CORE_STRESS 0
#endif

#ifndef APP_ENABLE_BASE_PROBES
#define APP_ENABLE_BASE_PROBES 1
#endif

#ifndef APP_ENABLE_SMP_DIAG
#define APP_ENABLE_SMP_DIAG 0
#endif

#ifndef APP_XSYNC_MASK
#define APP_XSYNC_MASK 0xFU
#endif

#define APP_XSYNC_SEM    0x1U
#define APP_XSYNC_QUEUE  0x2U
#define APP_XSYNC_MUTEX  0x4U
#define APP_XSYNC_SPIN   0x8U

static volatile uint32_t g_affinity_faults;
static volatile uint32_t g_secondary_cores_woken;
#if !APP_ENABLE_CROSS_CORE_STRESS
static TaskHandle_t g_worker_tasks[configNUMBER_OF_CORES];
#endif
static TaskHandle_t g_monitor_task;
#if APP_ENABLE_BASE_PROBES
static TaskHandle_t g_priority_high_task;
static TaskHandle_t g_priority_low_task;
static TaskHandle_t g_rr_tasks[2];
static TaskHandle_t g_sem_wait_task;
static TaskHandle_t g_sem_give_task;
static TaskHandle_t g_queue_recv_task;
static TaskHandle_t g_queue_send_task;
static TaskHandle_t g_mutex_tasks[2];
static TaskHandle_t g_spin_tasks[2];
#endif
#if APP_ENABLE_BASE_PROBES
static SemaphoreHandle_t g_probe_sem;
static QueueHandle_t g_probe_queue;
static SemaphoreHandle_t g_probe_mutex;
static RawSpinlock_t g_probe_raw_lock = {0};
#endif
#if APP_ENABLE_CROSS_CORE_STRESS
static TaskHandle_t g_xsync_sem_wait_task;
static TaskHandle_t g_xsync_sem_give_task;
static TaskHandle_t g_xsync_queue_recv_task;
static TaskHandle_t g_xsync_queue_send_task;
static TaskHandle_t g_xsync_mutex_tasks[2];
static TaskHandle_t g_xsync_spin_tasks[2];
static SemaphoreHandle_t g_xsync_sem;
static QueueHandle_t g_xsync_queue;
static SemaphoreHandle_t g_xsync_mutex;
static RawSpinlock_t g_xsync_raw_lock = {0};
static volatile uint32_t g_xsync_sem_gives;
static volatile uint32_t g_xsync_sem_takes;
static volatile uint32_t g_xsync_queue_sent;
static volatile uint32_t g_xsync_queue_received;
static volatile uint32_t g_xsync_queue_errors;
static volatile uint32_t g_xsync_mutex_counter;
static volatile uint32_t g_xsync_mutex_entries;
static volatile uint32_t g_xsync_mutex_guard;
static volatile uint32_t g_xsync_mutex_errors;
static volatile uint32_t g_xsync_spin_counter;
static volatile uint32_t g_xsync_spin_entries;
static volatile uint32_t g_xsync_spin_guard;
static volatile uint32_t g_xsync_spin_errors;
static volatile uint32_t g_xsync_task_stage[8];
static volatile uint32_t g_xsync_task_core[8];
#endif
static volatile uint32_t g_priority_first;
static volatile uint32_t g_priority_high_runs;
static volatile uint32_t g_priority_high_before_delete;
static volatile uint32_t g_priority_high_after_delete;
static volatile uint32_t g_priority_low_runs;
static volatile uint32_t g_rr_runs[2];
static volatile uint32_t g_sem_gives;
static volatile uint32_t g_sem_takes;
static volatile uint32_t g_queue_sent;
static volatile uint32_t g_queue_received;
static volatile uint32_t g_queue_errors;
static volatile uint32_t g_mutex_counter;
static volatile uint32_t g_mutex_entries;
static volatile uint32_t g_mutex_guard;
static volatile uint32_t g_mutex_errors;
static volatile uint32_t g_spin_counter;
static volatile uint32_t g_spin_entries;
static volatile uint32_t g_spin_guard;
static volatile uint32_t g_spin_errors;

extern volatile uint32_t g_task_delete_enter_count;
extern volatile uint32_t g_task_delete_stage;
extern volatile uint32_t g_task_delete_self_count;
extern volatile uint32_t g_task_delete_exit_count;
extern volatile uint32_t g_task_delete_switch_count;
#if APP_ENABLE_SMP_DIAG
extern volatile uint32_t g_task_switch_stage[];
extern volatile uint32_t g_task_switch_enter_count[];
extern volatile uint32_t g_task_switch_exit_count[];
extern volatile uint32_t g_task_enter_critical_stage;
extern volatile uint32_t g_task_enter_critical_core;
extern volatile uint32_t g_queue_take_stage[];
extern volatile uint64_t ullPortSvcEntries[];
extern volatile uint64_t ullPortSvcExits[];
extern volatile uint64_t ullPortSvcStages[];
extern volatile uint64_t ullPortSvcEsr[];
extern volatile uint64_t ullPortSyncAbortEsr[];
extern volatile uint64_t ullPortSyncAbortElr[];
extern volatile uint64_t ullPortSyncAbortFar[];
extern volatile uint64_t ullPortIrqSwitches[];
#endif
#if APP_ENABLE_SMP_DIAG
extern UBaseType_t uxTaskGetCriticalNestingForCore(BaseType_t xCoreID);
#endif

static uint64_t expected_core_mask(uint32_t cores)
{
    if (cores >= 64U) {
        return ~0ULL;
    }
    return (1ULL << cores) - 1ULL;
}

static const char *passfail(int ok)
{
    return ok ? "PASS" : "FAIL";
}

#if APP_ENABLE_SMP_DIAG
static uint32_t first_missing_core(uint64_t missing, uint32_t cores)
{
    for (uint32_t core = 0; core < cores; core++) {
        if ((missing & (1ULL << core)) != 0ULL) {
            return core;
        }
    }
    return 0xFFFFFFFFU;
}

static uint32_t task_kind_on_core(uint32_t core)
{
    if (core >= configNUMBER_OF_CORES) {
        return 0U;
    }

    TaskHandle_t current = xTaskGetCurrentTaskHandleForCore((BaseType_t)core);

    if (current == NULL) {
        return 0U;
    }
#if !APP_ENABLE_CROSS_CORE_STRESS
    if (current == g_worker_tasks[core]) {
        return 1U;
    }
#endif
    if (current == xTaskGetIdleTaskHandleForCore((BaseType_t)core)) {
        return 2U;
    }
    if (current == g_monitor_task) {
        return 3U;
    }
#if (configUSE_TIMERS == 1)
    if (current == xTimerGetTimerDaemonTaskHandle()) {
        return 4U;
    }
#endif
#if APP_ENABLE_BASE_PROBES
    if (current == g_priority_high_task) {
        return 10U;
    }
    if (current == g_priority_low_task) {
        return 11U;
    }
    if ((current == g_rr_tasks[0]) || (current == g_rr_tasks[1])) {
        return 12U;
    }
    if ((current == g_sem_wait_task) || (current == g_sem_give_task)) {
        return 13U;
    }
    if ((current == g_queue_recv_task) || (current == g_queue_send_task)) {
        return 14U;
    }
    if ((current == g_mutex_tasks[0]) || (current == g_mutex_tasks[1])) {
        return 15U;
    }
    if ((current == g_spin_tasks[0]) || (current == g_spin_tasks[1])) {
        return 16U;
    }
#endif
#if APP_ENABLE_CROSS_CORE_STRESS
    if ((current == g_xsync_sem_wait_task) || (current == g_xsync_sem_give_task)) {
        return 20U;
    }
    if ((current == g_xsync_queue_recv_task) || (current == g_xsync_queue_send_task)) {
        return 21U;
    }
    if ((current == g_xsync_mutex_tasks[0]) || (current == g_xsync_mutex_tasks[1])) {
        return 22U;
    }
    if ((current == g_xsync_spin_tasks[0]) || (current == g_xsync_spin_tasks[1])) {
        return 23U;
    }
#endif
    return 5U;
}
#endif

static UBaseType_t affinity_for_core(uint32_t core)
{
    if (core >= (uint32_t)configNUMBER_OF_CORES) {
        core = 0;
    }
    return (UBaseType_t)1U << core;
}

static uint32_t data_core(const UkernelBootInfo *boot, uint32_t index)
{
    if (boot->cores <= 1U) {
        return 0U;
    }
    if (boot->cores > 3U) {
        return 2U + (index % (boot->cores - 2U));
    }
    return 1U + (index % (boot->cores - 1U));
}

#if APP_ENABLE_BASE_PROBES
static void priority_high_probe(void *arg)
{
    (void)arg;
    uint32_t expected = 0U;

    (void)__atomic_compare_exchange_n(&g_priority_first,
                                      &expected,
                                      1U,
                                      0,
                                      __ATOMIC_ACQ_REL,
                                      __ATOMIC_RELAXED);
    for (uint32_t i = 0; i < APP_PROBE_ITERATIONS; i++) {
        __atomic_fetch_add(&g_priority_high_runs, 1U, __ATOMIC_RELAXED);
        __asm__ volatile("yield");
    }
    __atomic_fetch_add(&g_priority_high_before_delete, 1U, __ATOMIC_RELAXED);
    vTaskDelete(NULL);
    __atomic_fetch_add(&g_priority_high_after_delete, 1U, __ATOMIC_RELAXED);
    for (;;) {
        __asm__ volatile("yield");
    }
}

static void priority_low_probe(void *arg)
{
    (void)arg;
    uint32_t expected = 0U;

    (void)__atomic_compare_exchange_n(&g_priority_first,
                                      &expected,
                                      2U,
                                      0,
                                      __ATOMIC_ACQ_REL,
                                      __ATOMIC_RELAXED);
    for (uint32_t i = 0; i < APP_PROBE_ITERATIONS; i++) {
        __atomic_fetch_add(&g_priority_low_runs, 1U, __ATOMIC_RELAXED);
        __asm__ volatile("yield");
    }
    vTaskDelete(NULL);
}

static void rr_probe(void *arg)
{
    uint32_t index = (uint32_t)(uintptr_t)arg;

    for (uint32_t i = 0; i < APP_PROBE_ITERATIONS; i++) {
        __atomic_fetch_add(&g_rr_runs[index], 1U, __ATOMIC_RELAXED);
        taskYIELD();
    }
    vTaskDelete(NULL);
}

static void sem_wait_probe(void *arg)
{
    (void)arg;
    if (xSemaphoreTake(g_probe_sem, (TickType_t)2000) == pdTRUE) {
        __atomic_fetch_add(&g_sem_takes, 1U, __ATOMIC_RELAXED);
    }
    vTaskDelete(NULL);
}

static void sem_give_probe(void *arg)
{
    (void)arg;
    for (uint32_t i = 0; i < 8U; i++) {
        taskYIELD();
    }
    if (xSemaphoreGive(g_probe_sem) == pdTRUE) {
        __atomic_fetch_add(&g_sem_gives, 1U, __ATOMIC_RELAXED);
    }
    vTaskDelete(NULL);
}

static void queue_send_probe(void *arg)
{
    (void)arg;
    for (uint32_t value = 1U; value <= 4U; value++) {
        if (xQueueSend(g_probe_queue, &value, (TickType_t)100) == pdPASS) {
            __atomic_fetch_add(&g_queue_sent, 1U, __ATOMIC_RELAXED);
        } else {
            __atomic_fetch_add(&g_queue_errors, 1U, __ATOMIC_RELAXED);
        }
        taskYIELD();
    }
    vTaskDelete(NULL);
}

static void queue_recv_probe(void *arg)
{
    (void)arg;
    for (uint32_t expected = 1U; expected <= 4U; expected++) {
        uint32_t value = 0U;
        if (xQueueReceive(g_probe_queue, &value, (TickType_t)2000) == pdPASS) {
            if (value != expected) {
                __atomic_fetch_add(&g_queue_errors, 1U, __ATOMIC_RELAXED);
            }
            __atomic_fetch_add(&g_queue_received, 1U, __ATOMIC_RELAXED);
        } else {
            __atomic_fetch_add(&g_queue_errors, 1U, __ATOMIC_RELAXED);
        }
    }
    vTaskDelete(NULL);
}

static void mutex_probe(void *arg)
{
    (void)arg;
    for (uint32_t i = 0; i < APP_PROBE_ITERATIONS; i++) {
        if (xSemaphoreTake(g_probe_mutex, (TickType_t)2000) == pdTRUE) {
            if (g_mutex_guard != 0U) {
                __atomic_fetch_add(&g_mutex_errors, 1U, __ATOMIC_RELAXED);
            }
            g_mutex_guard++;
            uint32_t value = g_mutex_counter;
            for (volatile uint32_t spin = 0; spin < 128U; spin++) {
                __asm__ volatile("yield");
            }
            g_mutex_counter = value + 1U;
            g_mutex_guard--;
            __atomic_fetch_add(&g_mutex_entries, 1U, __ATOMIC_RELAXED);
            (void)xSemaphoreGive(g_probe_mutex);
        } else {
            __atomic_fetch_add(&g_mutex_errors, 1U, __ATOMIC_RELAXED);
        }
        taskYIELD();
    }
    vTaskDelete(NULL);
}

static void spin_probe(void *arg)
{
    (void)arg;
    for (uint32_t i = 0; i < APP_PROBE_ITERATIONS; i++) {
        vPortRawSpinLockAcquire(&g_probe_raw_lock);
        if (g_spin_guard != 0U) {
            __atomic_fetch_add(&g_spin_errors, 1U, __ATOMIC_RELAXED);
        }
        g_spin_guard++;
        uint32_t value = g_spin_counter;
        for (volatile uint32_t spin = 0; spin < 128U; spin++) {
            __asm__ volatile("yield");
        }
        g_spin_counter = value + 1U;
        g_spin_guard--;
        __atomic_fetch_add(&g_spin_entries, 1U, __ATOMIC_RELAXED);
        vPortRawSpinLockRelease(&g_probe_raw_lock);
        taskYIELD();
    }
    vTaskDelete(NULL);
}
#endif

#if APP_ENABLE_CROSS_CORE_STRESS
static void xsync_note(uint32_t task, uint32_t stage)
{
    __atomic_store_n(&g_xsync_task_core[task], (uint32_t)portGET_CORE_ID(), __ATOMIC_RELAXED);
    __atomic_store_n(&g_xsync_task_stage[task], stage, __ATOMIC_RELAXED);
}

static void xsync_sem_wait_probe(void *arg)
{
    (void)arg;
    xsync_note(0U, 1U);
    for (uint32_t i = 0; i < APP_PROBE_ITERATIONS; i++) {
        xsync_note(0U, 10U + i);
        if (xSemaphoreTake(g_xsync_sem, (TickType_t)2000) == pdTRUE) {
            __atomic_fetch_add(&g_xsync_sem_takes, 1U, __ATOMIC_RELAXED);
        }
        xsync_note(0U, 40U + i);
        taskYIELD();
    }
    xsync_note(0U, 90U);
    vTaskDelete(NULL);
}

static void xsync_sem_give_probe(void *arg)
{
    (void)arg;
    xsync_note(1U, 1U);
    for (uint32_t i = 0; i < APP_PROBE_ITERATIONS; i++) {
        xsync_note(1U, 10U + i);
        if (xSemaphoreGive(g_xsync_sem) == pdTRUE) {
            __atomic_fetch_add(&g_xsync_sem_gives, 1U, __ATOMIC_RELAXED);
        }
        xsync_note(1U, 40U + i);
        taskYIELD();
    }
    xsync_note(1U, 90U);
    vTaskDelete(NULL);
}

static void xsync_queue_send_probe(void *arg)
{
    (void)arg;
    xsync_note(3U, 1U);
    for (uint32_t value = 1U; value <= APP_PROBE_ITERATIONS; value++) {
        xsync_note(3U, 10U + value);
        if (xQueueSend(g_xsync_queue, &value, (TickType_t)2000) == pdPASS) {
            __atomic_fetch_add(&g_xsync_queue_sent, 1U, __ATOMIC_RELAXED);
        } else {
            __atomic_fetch_add(&g_xsync_queue_errors, 1U, __ATOMIC_RELAXED);
        }
        xsync_note(3U, 40U + value);
        taskYIELD();
    }
    xsync_note(3U, 90U);
    vTaskDelete(NULL);
}

static void xsync_queue_recv_probe(void *arg)
{
    (void)arg;
    xsync_note(2U, 1U);
    for (uint32_t expected = 1U; expected <= APP_PROBE_ITERATIONS; expected++) {
        uint32_t value = 0U;
        xsync_note(2U, 10U + expected);
        if (xQueueReceive(g_xsync_queue, &value, (TickType_t)2000) == pdPASS) {
            if (value != expected) {
                __atomic_fetch_add(&g_xsync_queue_errors, 1U, __ATOMIC_RELAXED);
            }
            __atomic_fetch_add(&g_xsync_queue_received, 1U, __ATOMIC_RELAXED);
        } else {
            __atomic_fetch_add(&g_xsync_queue_errors, 1U, __ATOMIC_RELAXED);
        }
        xsync_note(2U, 40U + expected);
        taskYIELD();
    }
    xsync_note(2U, 90U);
    vTaskDelete(NULL);
}

static void xsync_mutex_probe(void *arg)
{
    uint32_t index = (uint32_t)(uintptr_t)arg;
    xsync_note(4U + index, 1U);
    for (uint32_t i = 0; i < APP_PROBE_ITERATIONS; i++) {
        xsync_note(4U + index, 10U + i);
        if (xSemaphoreTake(g_xsync_mutex, (TickType_t)2000) == pdTRUE) {
            xsync_note(4U + index, 30U + i);
            if (g_xsync_mutex_guard != 0U) {
                __atomic_fetch_add(&g_xsync_mutex_errors, 1U, __ATOMIC_RELAXED);
            }
            g_xsync_mutex_guard++;
            uint32_t value = g_xsync_mutex_counter;
            for (volatile uint32_t spin = 0; spin < 128U; spin++) {
                __asm__ volatile("yield");
            }
            g_xsync_mutex_counter = value + 1U;
            g_xsync_mutex_guard--;
            __atomic_fetch_add(&g_xsync_mutex_entries, 1U, __ATOMIC_RELAXED);
            (void)xSemaphoreGive(g_xsync_mutex);
        } else {
            __atomic_fetch_add(&g_xsync_mutex_errors, 1U, __ATOMIC_RELAXED);
        }
        xsync_note(4U + index, 50U + i);
        taskYIELD();
    }
    xsync_note(4U + index, 90U);
    vTaskDelete(NULL);
}

static void xsync_spin_probe(void *arg)
{
    uint32_t index = (uint32_t)(uintptr_t)arg;
    xsync_note(6U + index, 1U);
    for (uint32_t i = 0; i < APP_PROBE_ITERATIONS; i++) {
        xsync_note(6U + index, 10U + i);
        vPortRawSpinLockAcquire(&g_xsync_raw_lock);
        xsync_note(6U + index, 30U + i);
        if (g_xsync_spin_guard != 0U) {
            __atomic_fetch_add(&g_xsync_spin_errors, 1U, __ATOMIC_RELAXED);
        }
        g_xsync_spin_guard++;
        uint32_t value = g_xsync_spin_counter;
        for (volatile uint32_t spin = 0; spin < 128U; spin++) {
            __asm__ volatile("yield");
        }
        g_xsync_spin_counter = value + 1U;
        g_xsync_spin_guard--;
        __atomic_fetch_add(&g_xsync_spin_entries, 1U, __ATOMIC_RELAXED);
        vPortRawSpinLockRelease(&g_xsync_raw_lock);
        xsync_note(6U + index, 50U + i);
        taskYIELD();
    }
    xsync_note(6U + index, 90U);
    vTaskDelete(NULL);
}
#endif

#if !APP_ENABLE_CROSS_CORE_STRESS
static void scheduler_worker(void *arg)
{
    uint32_t expected_core = (uint32_t)(uintptr_t)arg;

    for (;;) {
        uint32_t current_core = (uint32_t)portGET_CORE_ID();

        if (current_core != expected_core) {
            __atomic_fetch_add(&g_affinity_faults, 1U, __ATOMIC_RELAXED);
        }

        ukernel_note_core_alive(current_core);
        for (volatile uint32_t spin = 0; spin < 200000U; spin++) {
            __asm__ volatile("yield");
        }
        taskYIELD();
    }
}
#endif

static void scheduler_monitor(void *arg)
{
    const UkernelBootInfo *boot = (const UkernelBootInfo *)arg;
    uint64_t last_hb0 = 0;
    uint64_t expected = expected_core_mask(boot->cores);

    for (;;) {
        if (__atomic_exchange_n(&g_secondary_cores_woken, 1U, __ATOMIC_ACQ_REL) == 0U) {
            ukernel_wake_secondary_cores();
        }

        ukernel_note_core_alive(0);

        uint64_t active = vPortGetActiveCoreMask();
        uint64_t idle = vPortGetIdleCoreMask();
        uint64_t seen = ukernel_core_seen_mask();
        uint64_t hb0 = ukernel_core_heartbeat(0);
        uint64_t hb1 = ukernel_core_heartbeat(boot->cores > 1U ? 1U : 0U);
        uint64_t missing = expected & ~seen;
#if APP_ENABLE_SMP_DIAG
        uint32_t miss_core = first_missing_core(missing, boot->cores);
        uint32_t miss_task = task_kind_on_core(miss_core);
#endif
        uint32_t faults = __atomic_load_n(&g_affinity_faults, __ATOMIC_RELAXED);
        int active_ok = ((active & expected) == expected);
        int idle_ok = ((idle & expected) == 0ULL);
        int affinity_ok = (faults == 0U);
        int coverage_ok = ((seen & expected) == expected);
        int progress_ok = (hb0 > last_hb0) &&
                          ((boot->cores <= 1U) || coverage_ok);
        int prio_ok = (__atomic_load_n(&g_priority_first, __ATOMIC_RELAXED) == 1U) &&
                          (__atomic_load_n(&g_priority_high_runs, __ATOMIC_RELAXED) >= APP_PROBE_ITERATIONS) &&
                          (__atomic_load_n(&g_priority_low_runs, __ATOMIC_RELAXED) >= APP_PROBE_ITERATIONS);
        int rr_ok = (__atomic_load_n(&g_rr_runs[0], __ATOMIC_RELAXED) >= APP_PROBE_ITERATIONS) &&
                    (__atomic_load_n(&g_rr_runs[1], __ATOMIC_RELAXED) >= APP_PROBE_ITERATIONS);
        int sem_ok = (__atomic_load_n(&g_sem_gives, __ATOMIC_RELAXED) >= 1U) &&
                     (__atomic_load_n(&g_sem_takes, __ATOMIC_RELAXED) >= 1U);
        int queue_ok = (__atomic_load_n(&g_queue_sent, __ATOMIC_RELAXED) == 4U) &&
                       (__atomic_load_n(&g_queue_received, __ATOMIC_RELAXED) == 4U) &&
                       (__atomic_load_n(&g_queue_errors, __ATOMIC_RELAXED) == 0U);
        int mutex_ok = (__atomic_load_n(&g_mutex_entries, __ATOMIC_RELAXED) == (APP_PROBE_ITERATIONS * 2U)) &&
                       (__atomic_load_n(&g_mutex_counter, __ATOMIC_RELAXED) == (APP_PROBE_ITERATIONS * 2U)) &&
                       (__atomic_load_n(&g_mutex_errors, __ATOMIC_RELAXED) == 0U);
        int spin_ok = (__atomic_load_n(&g_spin_entries, __ATOMIC_RELAXED) == (APP_PROBE_ITERATIONS * 2U)) &&
                      (__atomic_load_n(&g_spin_counter, __ATOMIC_RELAXED) == (APP_PROBE_ITERATIONS * 2U)) &&
                      (__atomic_load_n(&g_spin_errors, __ATOMIC_RELAXED) == 0U);

        uart_log(6, "freertos",
                 "[sched-verify] active=%s idle=%s affinity=%s coverage=%s progress=%s prio=%s rr=%s sem=%s queue=%s mutex=%s spin=%s cores=%d seen=0x%x missing=0x%x hb0=%d hb1=%d\r\n",
                 passfail(active_ok),
                 passfail(idle_ok),
                 passfail(affinity_ok),
                 passfail(coverage_ok),
                 passfail(progress_ok),
                 passfail(prio_ok),
                 passfail(rr_ok),
                 passfail(sem_ok),
                 passfail(queue_ok),
                 passfail(mutex_ok),
                 passfail(spin_ok),
                 (int)boot->cores,
                 (int)seen,
                 (int)missing,
                 (int)hb0,
                 (int)hb1);
#if APP_ENABLE_SMP_DIAG
        uart_log(7, "freertos",
                 "[sched-debug] faults=%d activeMask=0x%x idleMask=0x%x missCore=%d missTask=%d missIpi=%d missSwitch=%d ec=%d/%d cn=%d task=%d/%d isr=%d/%d sw=%d/%d/%d\r\n",
                 (int)faults,
                 (int)active,
                 (int)idle,
                 (int)miss_core,
                 (int)miss_task,
                 (int)vPortGetYieldIPICount((long)miss_core),
                 (int)vPortGetSwitchCount((long)miss_core),
                 (int)__atomic_load_n(&g_task_enter_critical_core, __ATOMIC_RELAXED),
                 (int)__atomic_load_n(&g_task_enter_critical_stage, __ATOMIC_RELAXED),
                 (int)uxTaskGetCriticalNestingForCore((BaseType_t)miss_core),
                 (int)__atomic_load_n(&xTaskSpinlock.owner, __ATOMIC_RELAXED),
                 (int)__atomic_load_n(&xTaskSpinlock.count, __ATOMIC_RELAXED),
                 (int)__atomic_load_n(&xISRSpinlock.owner, __ATOMIC_RELAXED),
                 (int)__atomic_load_n(&xISRSpinlock.count, __ATOMIC_RELAXED),
                 (int)__atomic_load_n(&g_task_switch_stage[miss_core], __ATOMIC_RELAXED),
                 (int)__atomic_load_n(&g_task_switch_enter_count[miss_core], __ATOMIC_RELAXED),
                 (int)__atomic_load_n(&g_task_switch_exit_count[miss_core], __ATOMIC_RELAXED));
#endif
#if APP_ENABLE_CROSS_CORE_STRESS
        int xsync_sem_ok = ((APP_XSYNC_MASK & APP_XSYNC_SEM) == 0U) ||
                           ((__atomic_load_n(&g_xsync_sem_gives, __ATOMIC_RELAXED) == APP_PROBE_ITERATIONS) &&
                           (__atomic_load_n(&g_xsync_sem_takes, __ATOMIC_RELAXED) == APP_PROBE_ITERATIONS));
        int xsync_queue_ok = ((APP_XSYNC_MASK & APP_XSYNC_QUEUE) == 0U) ||
                             ((__atomic_load_n(&g_xsync_queue_sent, __ATOMIC_RELAXED) == APP_PROBE_ITERATIONS) &&
                             (__atomic_load_n(&g_xsync_queue_received, __ATOMIC_RELAXED) == APP_PROBE_ITERATIONS) &&
                             (__atomic_load_n(&g_xsync_queue_errors, __ATOMIC_RELAXED) == 0U));
        int xsync_mutex_ok = ((APP_XSYNC_MASK & APP_XSYNC_MUTEX) == 0U) ||
                             ((__atomic_load_n(&g_xsync_mutex_entries, __ATOMIC_RELAXED) == (APP_PROBE_ITERATIONS * 2U)) &&
                             (__atomic_load_n(&g_xsync_mutex_counter, __ATOMIC_RELAXED) == (APP_PROBE_ITERATIONS * 2U)) &&
                             (__atomic_load_n(&g_xsync_mutex_errors, __ATOMIC_RELAXED) == 0U));
        int xsync_spin_ok = ((APP_XSYNC_MASK & APP_XSYNC_SPIN) == 0U) ||
                            ((__atomic_load_n(&g_xsync_spin_entries, __ATOMIC_RELAXED) == (APP_PROBE_ITERATIONS * 2U)) &&
                            (__atomic_load_n(&g_xsync_spin_counter, __ATOMIC_RELAXED) == (APP_PROBE_ITERATIONS * 2U)) &&
                            (__atomic_load_n(&g_xsync_spin_errors, __ATOMIC_RELAXED) == 0U));

        uart_log(6, "freertos",
                 "[xsync-verify] sem=%s queue=%s mutex=%s spin=%s sg=%d st=%d qs=%d qr=%d qe=%d me=%d mc=%d merr=%d se=%d sc=%d serr=%d\r\n",
                 passfail(xsync_sem_ok),
                 passfail(xsync_queue_ok),
                 passfail(xsync_mutex_ok),
                 passfail(xsync_spin_ok),
                 (int)__atomic_load_n(&g_xsync_sem_gives, __ATOMIC_RELAXED),
                 (int)__atomic_load_n(&g_xsync_sem_takes, __ATOMIC_RELAXED),
                 (int)__atomic_load_n(&g_xsync_queue_sent, __ATOMIC_RELAXED),
                 (int)__atomic_load_n(&g_xsync_queue_received, __ATOMIC_RELAXED),
                 (int)__atomic_load_n(&g_xsync_queue_errors, __ATOMIC_RELAXED),
                 (int)__atomic_load_n(&g_xsync_mutex_entries, __ATOMIC_RELAXED),
                 (int)__atomic_load_n(&g_xsync_mutex_counter, __ATOMIC_RELAXED),
                 (int)__atomic_load_n(&g_xsync_mutex_errors, __ATOMIC_RELAXED),
                 (int)__atomic_load_n(&g_xsync_spin_entries, __ATOMIC_RELAXED),
                 (int)__atomic_load_n(&g_xsync_spin_counter, __ATOMIC_RELAXED),
                 (int)__atomic_load_n(&g_xsync_spin_errors, __ATOMIC_RELAXED));
#if APP_ENABLE_SMP_DIAG
        int spin_core_a = (int)__atomic_load_n(&g_xsync_task_core[6], __ATOMIC_RELAXED);
        int spin_core_b = (int)__atomic_load_n(&g_xsync_task_core[7], __ATOMIC_RELAXED);
        int task_lock_owner = (int)__atomic_load_n(&xTaskSpinlock.owner, __ATOMIC_RELAXED);
        int owner_core = (task_lock_owner > 0) ? (task_lock_owner - 1) : 0;
        uart_log(7, "freertos",
                 "[xsync-debug] semStage=%d/%d queueStage=%d/%d mutexStage=%d/%d spinStage=%d/%d semCore=%d/%d queueCore=%d/%d mutexCore=%d/%d spinCore=%d/%d\r\n",
                    (int)__atomic_load_n(&g_xsync_task_stage[0], __ATOMIC_RELAXED),
                    (int)__atomic_load_n(&g_xsync_task_stage[1], __ATOMIC_RELAXED),
                    (int)__atomic_load_n(&g_xsync_task_stage[2], __ATOMIC_RELAXED),
                    (int)__atomic_load_n(&g_xsync_task_stage[3], __ATOMIC_RELAXED),
                    (int)__atomic_load_n(&g_xsync_task_stage[4], __ATOMIC_RELAXED),
                    (int)__atomic_load_n(&g_xsync_task_stage[5], __ATOMIC_RELAXED),
                    (int)__atomic_load_n(&g_xsync_task_stage[6], __ATOMIC_RELAXED),
                    (int)__atomic_load_n(&g_xsync_task_stage[7], __ATOMIC_RELAXED),
                    (int)__atomic_load_n(&g_xsync_task_core[0], __ATOMIC_RELAXED),
                    (int)__atomic_load_n(&g_xsync_task_core[1], __ATOMIC_RELAXED),
                    (int)__atomic_load_n(&g_xsync_task_core[2], __ATOMIC_RELAXED),
                    (int)__atomic_load_n(&g_xsync_task_core[3], __ATOMIC_RELAXED),
                    (int)__atomic_load_n(&g_xsync_task_core[4], __ATOMIC_RELAXED),
                    (int)__atomic_load_n(&g_xsync_task_core[5], __ATOMIC_RELAXED),
                    spin_core_a,
                    spin_core_b);
        uart_log(7, "freertos",
                 "[xsync-port] svc=%d/%d/%d/%d svcst=%d/%d esr=0x%x/0x%x abort=0x%x/0x%x/0x%x/0x%x lock=%d/%d/%d/%d owner=%d\r\n",
                    (int)__atomic_load_n(&ullPortSvcEntries[spin_core_a], __ATOMIC_RELAXED),
                    (int)__atomic_load_n(&ullPortSvcExits[spin_core_a], __ATOMIC_RELAXED),
                    (int)__atomic_load_n(&ullPortSvcEntries[spin_core_b], __ATOMIC_RELAXED),
                    (int)__atomic_load_n(&ullPortSvcExits[spin_core_b], __ATOMIC_RELAXED),
                    (int)__atomic_load_n(&ullPortSvcStages[spin_core_a], __ATOMIC_RELAXED),
                    (int)__atomic_load_n(&ullPortSvcStages[spin_core_b], __ATOMIC_RELAXED),
                    (int)__atomic_load_n(&ullPortSvcEsr[spin_core_a], __ATOMIC_RELAXED),
                    (int)__atomic_load_n(&ullPortSvcEsr[spin_core_b], __ATOMIC_RELAXED),
                    (int)__atomic_load_n(&ullPortSyncAbortEsr[spin_core_a], __ATOMIC_RELAXED),
                    (int)__atomic_load_n(&ullPortSyncAbortElr[spin_core_a], __ATOMIC_RELAXED),
                    (int)__atomic_load_n(&ullPortSyncAbortEsr[spin_core_b], __ATOMIC_RELAXED),
                    (int)__atomic_load_n(&ullPortSyncAbortElr[spin_core_b], __ATOMIC_RELAXED),
                    task_lock_owner,
                    (int)__atomic_load_n(&xTaskSpinlock.count, __ATOMIC_RELAXED),
                    (int)__atomic_load_n(&xISRSpinlock.owner, __ATOMIC_RELAXED),
                    (int)__atomic_load_n(&xISRSpinlock.count, __ATOMIC_RELAXED),
                    owner_core);
        uart_log(7, "freertos",
                 "[xsync-switch] irq=%d/%d sw=%d/%d/%d/%d/%d/%d ownerSw=%d/%d/%d ownerSvc=%d/%d/%d enterCrit=%d/%d qtake=%d\r\n",
                    (int)__atomic_load_n(&ullPortIrqSwitches[spin_core_a], __ATOMIC_RELAXED),
                    (int)__atomic_load_n(&ullPortIrqSwitches[spin_core_b], __ATOMIC_RELAXED),
                    (int)__atomic_load_n(&g_task_switch_stage[spin_core_a], __ATOMIC_RELAXED),
                    (int)__atomic_load_n(&g_task_switch_enter_count[spin_core_a], __ATOMIC_RELAXED),
                    (int)__atomic_load_n(&g_task_switch_exit_count[spin_core_a], __ATOMIC_RELAXED),
                    (int)__atomic_load_n(&g_task_switch_stage[spin_core_b], __ATOMIC_RELAXED),
                    (int)__atomic_load_n(&g_task_switch_enter_count[spin_core_b], __ATOMIC_RELAXED),
                    (int)__atomic_load_n(&g_task_switch_exit_count[spin_core_b], __ATOMIC_RELAXED),
                    (int)__atomic_load_n(&g_task_switch_stage[owner_core], __ATOMIC_RELAXED),
                    (int)__atomic_load_n(&g_task_switch_enter_count[owner_core], __ATOMIC_RELAXED),
                    (int)__atomic_load_n(&g_task_switch_exit_count[owner_core], __ATOMIC_RELAXED),
                    (int)__atomic_load_n(&ullPortSvcStages[owner_core], __ATOMIC_RELAXED),
                    (int)__atomic_load_n(&ullPortSvcEntries[owner_core], __ATOMIC_RELAXED),
                    (int)__atomic_load_n(&ullPortSvcExits[owner_core], __ATOMIC_RELAXED),
                    (int)__atomic_load_n(&g_task_enter_critical_stage, __ATOMIC_RELAXED),
                    (int)__atomic_load_n(&g_task_enter_critical_core, __ATOMIC_RELAXED),
                    (int)__atomic_load_n(&g_queue_take_stage[owner_core], __ATOMIC_RELAXED));
#endif
#endif
        last_hb0 = hb0;
        for (volatile uint32_t spin = 0; spin < 20000000U; spin++) {
            __asm__ volatile("yield");
        }
        taskYIELD();
    }
}

int ukernel_app_init(const UkernelBootInfo *boot)
{
#if APP_ENABLE_BASE_PROBES
    g_probe_sem = xSemaphoreCreateBinary();
    g_probe_queue = xQueueCreate(4, sizeof(uint32_t));
    g_probe_mutex = xSemaphoreCreateMutex();
    if ((g_probe_sem == NULL) || (g_probe_queue == NULL) || (g_probe_mutex == NULL)) {
        return -1;
    }
#endif
#if APP_ENABLE_CROSS_CORE_STRESS
    g_xsync_sem = xSemaphoreCreateCounting(APP_PROBE_ITERATIONS, 0);
    g_xsync_queue = xQueueCreate(4, sizeof(uint32_t));
    g_xsync_mutex = xSemaphoreCreateMutex();
    if ((g_xsync_sem == NULL) || (g_xsync_queue == NULL) || (g_xsync_mutex == NULL)) {
        return -1;
    }
#endif

#if APP_ENABLE_BASE_PROBES
    uint32_t core_a = data_core(boot, 0U);
    uint32_t core_b = core_a;
    uint32_t core_c = core_a;

    if (xTaskCreateAffinitySet(priority_low_probe, "prio-l", APP_PROBE_STACK_WORDS, NULL,
                               tskIDLE_PRIORITY + 2, affinity_for_core(core_a), &g_priority_low_task) != pdPASS) {
        return -1;
    }
    if (xTaskCreateAffinitySet(priority_high_probe, "prio-h", APP_PROBE_STACK_WORDS, NULL,
                               tskIDLE_PRIORITY + 3, affinity_for_core(core_a), &g_priority_high_task) != pdPASS) {
        return -1;
    }
    if (xTaskCreateAffinitySet(rr_probe, "rr-a", APP_PROBE_STACK_WORDS, (void *)(uintptr_t)0U,
                               tskIDLE_PRIORITY + 2, affinity_for_core(core_a), &g_rr_tasks[0]) != pdPASS) {
        return -1;
    }
    if (xTaskCreateAffinitySet(rr_probe, "rr-b", APP_PROBE_STACK_WORDS, (void *)(uintptr_t)1U,
                               tskIDLE_PRIORITY + 2, affinity_for_core(core_a), &g_rr_tasks[1]) != pdPASS) {
        return -1;
    }
    if (xTaskCreateAffinitySet(sem_wait_probe, "sem-w", APP_PROBE_STACK_WORDS, NULL,
                               tskIDLE_PRIORITY + 2, affinity_for_core(core_b), &g_sem_wait_task) != pdPASS) {
        return -1;
    }
    if (xTaskCreateAffinitySet(sem_give_probe, "sem-g", APP_PROBE_STACK_WORDS, NULL,
                               tskIDLE_PRIORITY + 2, affinity_for_core(core_c), &g_sem_give_task) != pdPASS) {
        return -1;
    }
    if (xTaskCreateAffinitySet(queue_recv_probe, "q-rx", APP_PROBE_STACK_WORDS, NULL,
                               tskIDLE_PRIORITY + 2, affinity_for_core(core_b), &g_queue_recv_task) != pdPASS) {
        return -1;
    }
    if (xTaskCreateAffinitySet(queue_send_probe, "q-tx", APP_PROBE_STACK_WORDS, NULL,
                               tskIDLE_PRIORITY + 2, affinity_for_core(core_c), &g_queue_send_task) != pdPASS) {
        return -1;
    }
    if (xTaskCreateAffinitySet(mutex_probe, "mtx-a", APP_PROBE_STACK_WORDS, NULL,
                               tskIDLE_PRIORITY + 2, affinity_for_core(core_a), &g_mutex_tasks[0]) != pdPASS) {
        return -1;
    }
    if (xTaskCreateAffinitySet(mutex_probe, "mtx-b", APP_PROBE_STACK_WORDS, NULL,
                               tskIDLE_PRIORITY + 2, affinity_for_core(core_b), &g_mutex_tasks[1]) != pdPASS) {
        return -1;
    }
    if (xTaskCreateAffinitySet(spin_probe, "spn-a", APP_PROBE_STACK_WORDS, NULL,
                               tskIDLE_PRIORITY + 2, affinity_for_core(core_a), &g_spin_tasks[0]) != pdPASS) {
        return -1;
    }
    if (xTaskCreateAffinitySet(spin_probe, "spn-b", APP_PROBE_STACK_WORDS, NULL,
                               tskIDLE_PRIORITY + 2, affinity_for_core(core_b), &g_spin_tasks[1]) != pdPASS) {
        return -1;
    }
#endif

#if APP_ENABLE_CROSS_CORE_STRESS
    uint32_t xsync_sem_a = data_core(boot, 0U);
    uint32_t xsync_sem_b = data_core(boot, 1U);
    uint32_t xsync_queue_a = data_core(boot, 2U);
    uint32_t xsync_queue_b = data_core(boot, 3U);
    uint32_t xsync_mutex_a = data_core(boot, 4U);
    uint32_t xsync_mutex_b = data_core(boot, 5U);
    uint32_t xsync_spin_a = data_core(boot, 6U);
    uint32_t xsync_spin_b = data_core(boot, 7U);

    if ((APP_XSYNC_MASK & APP_XSYNC_SEM) != 0U) {
        if (xTaskCreateAffinitySet(xsync_sem_wait_probe, "xs-sem-w", APP_PROBE_STACK_WORDS, NULL,
                                   tskIDLE_PRIORITY + 2, affinity_for_core(xsync_sem_a), &g_xsync_sem_wait_task) != pdPASS) {
            return -1;
        }
        if (xTaskCreateAffinitySet(xsync_sem_give_probe, "xs-sem-g", APP_PROBE_STACK_WORDS, NULL,
                                   tskIDLE_PRIORITY + 2, affinity_for_core(xsync_sem_b), &g_xsync_sem_give_task) != pdPASS) {
            return -1;
        }
    }
    if ((APP_XSYNC_MASK & APP_XSYNC_QUEUE) != 0U) {
        if (xTaskCreateAffinitySet(xsync_queue_recv_probe, "xs-q-rx", APP_PROBE_STACK_WORDS, NULL,
                                   tskIDLE_PRIORITY + 2, affinity_for_core(xsync_queue_a), &g_xsync_queue_recv_task) != pdPASS) {
            return -1;
        }
        if (xTaskCreateAffinitySet(xsync_queue_send_probe, "xs-q-tx", APP_PROBE_STACK_WORDS, NULL,
                                   tskIDLE_PRIORITY + 2, affinity_for_core(xsync_queue_b), &g_xsync_queue_send_task) != pdPASS) {
            return -1;
        }
    }
    if ((APP_XSYNC_MASK & APP_XSYNC_MUTEX) != 0U) {
        if (xTaskCreateAffinitySet(xsync_mutex_probe, "xs-mtx-a", APP_PROBE_STACK_WORDS, (void *)(uintptr_t)0U,
                                   tskIDLE_PRIORITY + 2, affinity_for_core(xsync_mutex_a), &g_xsync_mutex_tasks[0]) != pdPASS) {
            return -1;
        }
        if (xTaskCreateAffinitySet(xsync_mutex_probe, "xs-mtx-b", APP_PROBE_STACK_WORDS, (void *)(uintptr_t)1U,
                                   tskIDLE_PRIORITY + 2, affinity_for_core(xsync_mutex_b), &g_xsync_mutex_tasks[1]) != pdPASS) {
            return -1;
        }
    }
    if ((APP_XSYNC_MASK & APP_XSYNC_SPIN) != 0U) {
        if (xTaskCreateAffinitySet(xsync_spin_probe, "xs-spn-a", APP_PROBE_STACK_WORDS, (void *)(uintptr_t)0U,
                                   tskIDLE_PRIORITY + 2, affinity_for_core(xsync_spin_a), &g_xsync_spin_tasks[0]) != pdPASS) {
            return -1;
        }
        if (xTaskCreateAffinitySet(xsync_spin_probe, "xs-spn-b", APP_PROBE_STACK_WORDS, (void *)(uintptr_t)1U,
                                   tskIDLE_PRIORITY + 2, affinity_for_core(xsync_spin_b), &g_xsync_spin_tasks[1]) != pdPASS) {
            return -1;
        }
    }
#endif

#if !APP_ENABLE_CROSS_CORE_STRESS
    for (uint32_t core = 1; core < boot->cores; core++) {
        TaskHandle_t task = NULL;
        UBaseType_t affinity = ((UBaseType_t)1U << core);
        BaseType_t ok = xTaskCreateAffinitySet(scheduler_worker,
                                               "sched-w",
                                               APP_WORKER_STACK_WORDS,
                                               (void *)(uintptr_t)core,
                                               tskIDLE_PRIORITY + 1,
                                               affinity,
                                               &task);
        if (ok != pdPASS) {
            return -1;
        }
        g_worker_tasks[core] = task;
    }
#endif

    BaseType_t ok = xTaskCreateAffinitySet(scheduler_monitor,
                                           "sched-mon",
                                           APP_MONITOR_STACK_WORDS,
                                           (void *)boot,
                                           configMAX_PRIORITIES - 1,
                                           (UBaseType_t)1U,
                                           &g_monitor_task);
    return ok == pdPASS ? 0 : -1;
}
