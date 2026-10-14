# CherryTimer - 多核并发高精度 Linux 风格定时器子系统

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Precision: Hardware Nanosecond](https://img.shields.io/badge/Precision-ARM%20Generic%20Timer-green.svg)]()
[![Multi-Core: Per--CPU Base](https://img.shields.io/badge/SMP-Per--CPU%20Timer%20Base-blue.svg)]()
[![Standard: Linux Kernel](https://img.shields.io/badge/Standard-timer__list%20%2B%20hrtimer-orange.svg)]()

`CherryTimer` 是专为嵌入式与实时操作系统量身设计的高精度、多核并发定时器引擎。它严格遵循 Linux 内核经典定时器架构，提供低精度内核定时器（`struct timer_list`）与高精度高分辨率定时器（`struct hrtimer`）双套标准接口，时间基准直接接入 BSP 提供的硬件时钟计数器，实现微秒乃至纳秒级的精准计时。

---

## 🌟 核心特性与架构设计

### 1. 硬件级精准时基（BSP Hardware Timer Integration）
- 计时核心直接接入 BSP 的 ARM Generic Timer 物理计数器 `cntpct_el0` 与频率寄存器 `cntfrq_el0`；
- 在 QEMU virt AArch64 环境下以 62.5MHz 频率连续自增，单周期精度达 **~16 纳秒**；
- 硬件寄存器全局单调递增，跨核心硬件同步，杜绝多核时间漂移。

### 2. 多核并发独立队列（Per-CPU Timer Base）
- 借鉴 Linux 内核 Per-CPU 设计理念，为每个 CPU 核心分配独立的 `struct timer_base`；
- 每个核心拥有专属的独立自旋锁（`spinlock_t`）、`hrtimer` 链表与 `timer_list` 链表；
- **多核并发添加、修改与删除定时器时完全无锁竞争**，具备优异的多核并行吞吐能力。

### 3. Linux 标准定时器 API

#### 标准内核定时器（`struct timer_list`）
```c
struct timer_list my_timer;

void timer_setup(struct timer_list *timer, void (*callback)(struct timer_list *), unsigned int flags);
void add_timer(struct timer_list *timer);
void add_timer_on(struct timer_list *timer, int cpu);
int  mod_timer(struct timer_list *timer, unsigned long expires);
int  del_timer(struct timer_list *timer);
int  del_timer_sync(struct timer_list *timer);
int  timer_pending(const struct timer_list *timer);
```

#### 高分辨率定时器（`struct hrtimer`）
```c
struct hrtimer my_hrtimer;

void hrtimer_init(struct hrtimer *timer, int clock_id, enum hrtimer_mode mode);
void hrtimer_start(struct hrtimer *timer, ktime_t tim, enum hrtimer_mode mode);
int  hrtimer_cancel(struct hrtimer *timer);
int  hrtimer_try_to_cancel(struct hrtimer *timer);
bool hrtimer_active(const struct hrtimer *timer);

/* 纳秒高精度时间接口 */
ktime_t ktime_get(void);
ktime_t ktime_set(const int64_t secs, const unsigned long nsecs);
```

---

## 🧪 自动化验收断言

在系统启动阶段自动运行 `cherry_timer_run_tests()`，覆盖单次标准定时、纳秒 hrtimer、多核并发定时、周期性自动重启与定时器取消：
```text
[    0.015000] [6:cpu0:sched-mon:1] ==> starting cherrytimer multi-core acceptance test suite...
[    0.025000] [6:cpu0:sched-mon:1] [timer-verify] standard=pass hrtimer=pass multi_core=pass restart=pass cancel=pass all=pass
```
