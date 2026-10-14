# CherryKAL - Kernel Abstraction Layer (OSAL)

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Standard: Linux-Style](https://img.shields.io/badge/API-Linux--Style%20Kernel%20Primitives-orange.svg)]()
[![SMP: Multicore](https://img.shields.io/badge/SMP-Multicore%20Safe-blue.svg)]()

`CherryKAL` 是为嵌入式和实时操作系统（RTOS）设计的**现代化、小而美、标准化**的内核抽象层（Kernel Abstraction Layer / OSAL）。它严格遵循 Linux 内核编程规范与命名习惯（`kal_mutex_*`, `kal_sem_*`, `kal_spin_*`, `kal_thread_*`, `kal_malloc`/`free`, `kal_atomic_*`），将上层组件（如 `CherryVFS`, `CherryShell`）与底层具体 RTOS（如 FreeRTOS SMP、ThreadX、RT-Thread、Zephyr 或 Bare-metal）完全解耦。

---

## 🌟 核心设计理念

1. **零底层耦合（Zero Underlying RTOS Lock-in）**：
   上层模块仅包含 `cherry_kal.h`，无需引入 `FreeRTOS.h`、`task.h` 或任何特定内核头文件。任何第三方嵌入式开发者均可单独移植并使用 Cherry 系列组件。
2. **Linux API 语义对齐**：
   遵循 Linux 互斥锁（Mutex）、自旋锁（Spinlock 带 IRQ 保存/恢复）、计数信号量（Semaphore）、线程与核心亲和性（Affinity）规范。
3. **SMP 多核与中断安全**：
   内置 SMP 安全的自旋锁与原子屏障（`kal_mb()`, `kal_rmb()`, `kal_wmb()`），原生支持多核并发环境。
4. **统一控制台与时间基准**：
   统一系统时钟（毫秒/微秒级高精时间基准 `kal_get_time_ms`, `kal_get_time_us`）与线程安全格式化日志输出（`kal_printf`）。

---

## 📋 API 架构规范

### 1. 内存管理（Memory Management）
```c
void *kal_malloc(size_t size);
void  kal_free(void *ptr);
void *kal_calloc(size_t nmemb, size_t size);
void *kal_realloc(void *ptr, size_t size);
```

### 2. 互斥锁（Recursive Mutex）
```c
int kal_mutex_init(kal_mutex_t *mutex);
int kal_mutex_lock(kal_mutex_t *mutex);
int kal_mutex_trylock(kal_mutex_t *mutex);
int kal_mutex_unlock(kal_mutex_t *mutex);
int kal_mutex_destroy(kal_mutex_t *mutex);
```

### 3. SMP 自旋锁（Spinlock with IRQ Save/Restore）
```c
int           kal_spin_lock_init(kal_spinlock_t *lock);
void          kal_spin_lock(kal_spinlock_t *lock);
void          kal_spin_unlock(kal_spinlock_t *lock);
unsigned long kal_spin_lock_irqsave(kal_spinlock_t *lock);
void          kal_spin_unlock_irqrestore(kal_spinlock_t *lock, unsigned long flags);
```

### 4. 信号量（Counting Semaphore）
```c
int kal_sem_init(kal_sem_t *sem, unsigned int initial_count, unsigned int max_count);
int kal_sem_wait(kal_sem_t *sem, uint32_t timeout_ms);
int kal_sem_post(kal_sem_t *sem);
int kal_sem_destroy(kal_sem_t *sem);
```

### 5. 线程与亲和性（Thread & Core Affinity）
```c
kal_thread_t kal_thread_create(const char *name,
                               kal_thread_func_t func,
                               void *arg,
                               size_t stack_size,
                               int priority);
int          kal_thread_set_affinity(kal_thread_t thread, uint32_t core_mask);
void         kal_thread_sleep_ms(uint32_t ms);
void         kal_thread_yield(void);
void         kal_thread_exit(void);
kal_thread_t kal_thread_self(void);
uint32_t     kal_get_core_id(void);
```

### 6. 时间基准与内存屏障（Time & Barriers）
```c
uint64_t kal_get_time_ms(void);
uint64_t kal_get_time_us(void);
uint32_t kal_get_tick(void);
uint32_t kal_tick_rate_hz(void);

void kal_mb(void);   /* Data Memory Barrier full */
void kal_rmb(void);  /* Data Memory Barrier read */
void kal_wmb(void);  /* Data Memory Barrier write */
```

---

## 🛠️ FreeRTOS SMP 适配层实现

当前仓库提供基于 **FreeRTOS V11 SMP** 与 **ARMv8-A (AArch64)** 的官方参考实现：
- `components/cherry_kal/src/kal_freertos.c`
  - 互斥锁映射至 `xSemaphoreCreateRecursiveMutex`
  - 自旋锁利用 AArch64 `__atomic_test_and_set` 与 `mrs daif` 关中断状态字
  - 高精度时间通过 AArch64 通用计数器寄存器 `cntpct_el0` 与 `cntfrq_el0` 实现微秒级计时
  - 核心 ID 通过 `mpidr_el1` 寄存器零开销硬件读取
