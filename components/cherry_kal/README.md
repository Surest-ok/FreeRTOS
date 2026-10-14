# CherryKAL - Kernel Abstraction Layer (OSAL)

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Standard: Linux-Style](https://img.shields.io/badge/API-Linux--Style%20Kernel%20Primitives-orange.svg)]()
[![SMP: Multicore](https://img.shields.io/badge/SMP-Multicore%20Safe-blue.svg)]()

`CherryKAL` 是为嵌入式和实时操作系统（RTOS）设计的**现代化、小而美、标准化**的内核抽象层（Kernel Abstraction Layer / OSAL）。它严格遵循 Linux 内核编程规范与命名习惯（`kal_mutex_*`, `kal_sem_*`, `kal_spin_*`, `kal_thread_*`, `kal_malloc`/`free`, `kal_atomic_*`），将上层组件（如 `CherryVFS`, `CherryShell`, `CherryTimer`, `CherryPrintk`）与底层具体 RTOS（如 FreeRTOS SMP、ThreadX、RT-Thread、Zephyr 或 Bare-metal）完全解耦。

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

## 📋 100% Linux Kernel API 规范（Pure Linux Standard）

`CherryKAL` 提供与 Linux 内核头文件完全一致的数据结构与函数命名规范，无需任何自定义前缀：

### 1. 标准日志级别与 printk（Linux Printk & Log Levels，底层由 CherryPrintk 驱动）
```c
int printk(const char *fmt, ...);

pr_emerg(fmt, ...);   /* <0> system is unusable */
pr_alert(fmt, ...);   /* <1> action must be taken immediately */
pr_crit(fmt, ...);    /* <2> critical conditions */
pr_err(fmt, ...);     /* <3> error conditions */
pr_warn(fmt, ...);    /* <4> warning conditions */
pr_notice(fmt, ...);  /* <5> normal but significant condition */
pr_info(fmt, ...);    /* <6> informational */
pr_debug(fmt, ...);   /* <7> debug-level messages */
```

### 2. 内存管理（Memory Management）
```c
void *kmalloc(size_t size, gfp_t flags);
void  kfree(const void *ptr);
void *kzalloc(size_t size, gfp_t flags);
void *kcalloc(size_t nmemb, size_t size, gfp_t flags);
void *krealloc(const void *ptr, size_t size, gfp_t flags);
```

### 3. 互斥锁（struct mutex）
```c
void mutex_init(struct mutex *lock);
void mutex_lock(struct mutex *lock);
int  mutex_trylock(struct mutex *lock);
void mutex_unlock(struct mutex *lock);
void mutex_destroy(struct mutex *lock);
```

### 4. SMP 自旋锁（spinlock_t）
```c
void          spin_lock_init(spinlock_t *lock);
void          spin_lock(spinlock_t *lock);
void          spin_unlock(spinlock_t *lock);
unsigned long _spin_lock_irqsave(spinlock_t *lock);
void          spin_unlock_irqrestore(spinlock_t *lock, unsigned long flags);

#define spin_lock_irqsave(lock, flags) \
    do { (flags) = _spin_lock_irqsave(lock); } while (0)
```

### 5. 信号量（struct semaphore）
```c
void sema_init(struct semaphore *sem, int val);
void down(struct semaphore *sem);
int  down_interruptible(struct semaphore *sem);
int  down_trylock(struct semaphore *sem);
int  down_timeout(struct semaphore *sem, long timeout_ms);
void up(struct semaphore *sem);
void sema_destroy(struct semaphore *sem);
```

### 6. 内核线程与调度（kthread & Scheduling）
```c
struct task_struct *kthread_create(kthread_fn_t threadfn, void *data, const char *name);
struct task_struct *kthread_run(kthread_fn_t threadfn, void *data, const char *name);
int                 kthread_bind(struct task_struct *task, unsigned int cpu);
void                schedule(void);
void                msleep(unsigned int msecs);
void                usleep_range(unsigned long min, unsigned long max);
unsigned int        smp_processor_id(void);
```

### 7. 时钟与 Jiffies（Time & Jiffies）
```c
#define HZ 1000U
extern volatile unsigned long jiffies;

unsigned long msecs_to_jiffies(const unsigned int m);
unsigned int  jiffies_to_msecs(const unsigned long j);
uint64_t      ktime_get_ns(void);
uint64_t      ktime_get_ms(void);
```

### 8. 原子操作与内存屏障（Atomics & Memory Barriers）
```c
atomic_t v = ATOMIC_INIT(0);
int  atomic_read(const atomic_t *v);
void atomic_set(atomic_t *v, int i);
void atomic_add(int i, atomic_t *v);
void atomic_sub(int i, atomic_t *v);
void atomic_inc(atomic_t *v);
void atomic_dec(atomic_t *v);
int  atomic_inc_return(atomic_t *v);
int  atomic_dec_return(atomic_t *v);

mb();       /* Full memory barrier */
rmb();      /* Read memory barrier */
wmb();      /* Write memory barrier */
smp_mb();   smp_rmb();   smp_wmb();
```

*(注：同时提供 `kal_*` 宏作为向后兼容映射。)*

---

## 🛠️ FreeRTOS SMP 适配层实现

当前仓库提供基于 **FreeRTOS V11 SMP** 与 **ARMv8-A (AArch64)** 的官方参考实现：
- `components/cherry_kal/src/kal_freertos.c`
  - 互斥锁映射至 `xSemaphoreCreateRecursiveMutex`
  - 自旋锁利用 AArch64 `__atomic_test_and_set` 与 `mrs daif` 关中断状态字
  - 高精度时间通过 AArch64 通用计数器寄存器 `cntpct_el0` 与 `cntfrq_el0` 实现微秒级计时
  - 核心 ID 通过 `mpidr_el1` 寄存器零开销硬件读取
