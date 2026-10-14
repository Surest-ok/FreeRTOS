/*
 * CherryKAL - FreeRTOS SMP Implementation
 *
 * Copyright (c) 2026 ForgeStitch
 * SPDX-License-Identifier: MIT
 *
 * Implements Linux-style KAL APIs on FreeRTOS V11 SMP
 */

#include "cherry_kal.h"
#include <string.h>
#include <stdarg.h>

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "port_smp.h"

extern void uart_printf(const char *fmt, ...);

/* =========================================================================
 * 1. Memory Management
 * ========================================================================= */
void *kal_malloc(size_t size)
{
    return pvPortMalloc(size);
}

void kal_free(void *ptr)
{
    if (ptr) {
        vPortFree(ptr);
    }
}

void *kal_calloc(size_t nmemb, size_t size)
{
    size_t total = nmemb * size;
    void *ptr = pvPortMalloc(total);
    if (ptr) {
        memset(ptr, 0, total);
    }
    return ptr;
}

void *kal_realloc(void *ptr, size_t size)
{
    if (!ptr) return pvPortMalloc(size);
    if (size == 0) {
        vPortFree(ptr);
        return NULL;
    }
    void *new_ptr = pvPortMalloc(size);
    if (new_ptr) {
        /* Conservative copy; in real allocator heap_4 doesn't store chunk size */
        memcpy(new_ptr, ptr, size);
        vPortFree(ptr);
    }
    return new_ptr;
}

/* =========================================================================
 * 2. Mutex Synchronization
 * ========================================================================= */
int kal_mutex_init(kal_mutex_t *mutex)
{
    if (!mutex) return KAL_INVAL;
    SemaphoreHandle_t sem = xSemaphoreCreateRecursiveMutex();
    if (!sem) return KAL_NOMEM;
    mutex->handle = (void *)sem;
    return KAL_OK;
}

int kal_mutex_lock(kal_mutex_t *mutex)
{
    if (!mutex || !mutex->handle) return KAL_INVAL;
    if (xTaskGetSchedulerState() == taskSCHEDULER_NOT_STARTED) {
        return KAL_OK; /* Early boot bypass */
    }
    BaseType_t ret = xSemaphoreTakeRecursive((SemaphoreHandle_t)mutex->handle, portMAX_DELAY);
    return (ret == pdTRUE) ? KAL_OK : KAL_ERROR;
}

int kal_mutex_trylock(kal_mutex_t *mutex)
{
    if (!mutex || !mutex->handle) return KAL_INVAL;
    if (xTaskGetSchedulerState() == taskSCHEDULER_NOT_STARTED) {
        return KAL_OK;
    }
    BaseType_t ret = xSemaphoreTakeRecursive((SemaphoreHandle_t)mutex->handle, 0);
    return (ret == pdTRUE) ? KAL_OK : KAL_BUSY;
}

int kal_mutex_unlock(kal_mutex_t *mutex)
{
    if (!mutex || !mutex->handle) return KAL_INVAL;
    if (xTaskGetSchedulerState() == taskSCHEDULER_NOT_STARTED) {
        return KAL_OK;
    }
    BaseType_t ret = xSemaphoreGiveRecursive((SemaphoreHandle_t)mutex->handle);
    return (ret == pdTRUE) ? KAL_OK : KAL_ERROR;
}

int kal_mutex_destroy(kal_mutex_t *mutex)
{
    if (!mutex || !mutex->handle) return KAL_INVAL;
    vSemaphoreDelete((SemaphoreHandle_t)mutex->handle);
    mutex->handle = NULL;
    return KAL_OK;
}

/* =========================================================================
 * 3. Spinlock (SMP safe)
 * ========================================================================= */
int kal_spin_lock_init(kal_spinlock_t *lock)
{
    if (!lock) return KAL_INVAL;
    lock->lock = 0;
    lock->owner_core = 0xFFFFFFFFU;
    lock->nesting = 0;
    return KAL_OK;
}

void kal_spin_lock(kal_spinlock_t *lock)
{
    if (!lock) return;
    uint32_t core = kal_get_core_id();

    if (lock->owner_core == core) {
        lock->nesting++;
        return;
    }

    while (__atomic_test_and_set(&lock->lock, __ATOMIC_ACQUIRE)) {
        __asm__ volatile("yield");
    }
    lock->owner_core = core;
    lock->nesting = 1;
}

void kal_spin_unlock(kal_spinlock_t *lock)
{
    if (!lock) return;
    uint32_t core = kal_get_core_id();
    if (lock->owner_core == core) {
        if (--lock->nesting == 0) {
            lock->owner_core = 0xFFFFFFFFU;
            __atomic_clear(&lock->lock, __ATOMIC_RELEASE);
        }
    }
}

unsigned long kal_spin_lock_irqsave(kal_spinlock_t *lock)
{
    unsigned long daif;
    __asm__ volatile("mrs %0, daif\nmsr daifset, #2" : "=r"(daif) :: "memory");
    kal_spin_lock(lock);
    return daif;
}

void kal_spin_unlock_irqrestore(kal_spinlock_t *lock, unsigned long flags)
{
    kal_spin_unlock(lock);
    __asm__ volatile("msr daif, %0" :: "r"(flags) : "memory");
}

/* =========================================================================
 * 4. Semaphore Synchronization
 * ========================================================================= */
int kal_sem_init(kal_sem_t *sem, unsigned int initial_count, unsigned int max_count)
{
    if (!sem) return KAL_INVAL;
    SemaphoreHandle_t h = xSemaphoreCreateCounting((UBaseType_t)max_count, (UBaseType_t)initial_count);
    if (!h) return KAL_NOMEM;
    sem->handle = (void *)h;
    return KAL_OK;
}

int kal_sem_wait(kal_sem_t *sem, uint32_t timeout_ms)
{
    if (!sem || !sem->handle) return KAL_INVAL;
    TickType_t ticks = (timeout_ms == KAL_WAIT_FOREVER) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
    BaseType_t ret = xSemaphoreTake((SemaphoreHandle_t)sem->handle, ticks);
    return (ret == pdTRUE) ? KAL_OK : KAL_TIMEOUT;
}

int kal_sem_post(kal_sem_t *sem)
{
    if (!sem || !sem->handle) return KAL_INVAL;
    BaseType_t ret = xSemaphoreGive((SemaphoreHandle_t)sem->handle);
    return (ret == pdTRUE) ? KAL_OK : KAL_ERROR;
}

int kal_sem_destroy(kal_sem_t *sem)
{
    if (!sem || !sem->handle) return KAL_INVAL;
    vSemaphoreDelete((SemaphoreHandle_t)sem->handle);
    sem->handle = NULL;
    return KAL_OK;
}

/* =========================================================================
 * 5. Thread / Task Management
 * ========================================================================= */
kal_thread_t kal_thread_create(const char *name,
                               kal_thread_func_t func,
                               void *arg,
                               size_t stack_size,
                               int priority)
{
    TaskHandle_t task = NULL;
    uint32_t stack_words = (uint32_t)(stack_size / sizeof(StackType_t));
    if (stack_words < configMINIMAL_STACK_SIZE) {
        stack_words = configMINIMAL_STACK_SIZE;
    }

    BaseType_t ret = xTaskCreate(func,
                                 name ? name : "kal_th",
                                 (configSTACK_DEPTH_TYPE)stack_words,
                                 arg,
                                 (UBaseType_t)priority,
                                 &task);
    if (ret != pdPASS) {
        return NULL;
    }
    return (kal_thread_t)task;
}

int kal_thread_set_affinity(kal_thread_t thread, uint32_t core_mask)
{
    if (!thread) return KAL_INVAL;
#if ( configUSE_CORE_AFFINITY == 1 )
    vTaskCoreAffinitySet((TaskHandle_t)thread, (UBaseType_t)core_mask);
    return KAL_OK;
#else
    (void)thread;
    (void)core_mask;
    return KAL_OK;
#endif
}

void kal_thread_sleep_ms(uint32_t ms)
{
    if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED) {
        vTaskDelay(pdMS_TO_TICKS(ms));
    } else {
        for (volatile uint32_t i = 0; i < ms * 10000; i++) {
            __asm__ volatile("yield");
        }
    }
}

void kal_thread_yield(void)
{
    taskYIELD();
}

void kal_thread_exit(void)
{
    vTaskDelete(NULL);
}

kal_thread_t kal_thread_self(void)
{
    return (kal_thread_t)xTaskGetCurrentTaskHandle();
}

uint32_t kal_get_core_id(void)
{
    uint64_t mpidr;
    __asm__ volatile("mrs %0, mpidr_el1" : "=r"(mpidr));
    return (uint32_t)((mpidr & 0xFFU) + (((mpidr >> 8) & 0xFFU) << 4));
}

/* =========================================================================
 * 6. Time and Ticks
 * ========================================================================= */
uint32_t kal_get_tick(void)
{
    return (uint32_t)xTaskGetTickCount();
}

uint32_t kal_tick_rate_hz(void)
{
    return (uint32_t)configTICK_RATE_HZ;
}

uint64_t kal_get_time_ms(void)
{
    uint32_t ticks = kal_get_tick();
    return (uint64_t)ticks * 1000ULL / (uint64_t)configTICK_RATE_HZ;
}

uint64_t kal_get_time_us(void)
{
    uint64_t cnt;
    uint64_t frq;
    __asm__ volatile("mrs %0, cntpct_el0" : "=r"(cnt));
    __asm__ volatile("mrs %0, cntfrq_el0" : "=r"(frq));
    if (frq == 0) return 0;
    return (cnt * 1000000ULL) / frq;
}

/* =========================================================================
 * 7. Critical Sections
 * ========================================================================= */
void kal_enter_critical(void)
{
    taskENTER_CRITICAL();
}

void kal_exit_critical(void)
{
    taskEXIT_CRITICAL();
}

/* =========================================================================
 * 8. Console Logging Output
 * ========================================================================= */
extern int vsnprintf(char *str, size_t size, const char *format, va_list ap);
extern void uart_puts(const char *str);

void kal_printf(const char *fmt, ...)
{
    char buf[256];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    uart_puts(buf);
}

