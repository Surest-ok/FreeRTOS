# CherryPrintk - 高性能多核并发 Linux 风格日志引擎

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Format: Linux Kernel dmesg](https://img.shields.io/badge/Format-Linux%20Kernel%20dmesg-green.svg)]()
[![Multi-Core: Non-interleaving](https://img.shields.io/badge/SMP-Atomic%20Line%20Buffer-blue.svg)]()
[![IRQ: <100ns Critical Section](https://img.shields.io/badge/IRQ%20Disabled-%3C100ns-orange.svg)]()

`CherryPrintk` 是为嵌入式多核系统和实时操作系统（FreeRTOS 等）量身设计的**高性能、多核并发、无乱序** Linux 风格日志与格式化输出引擎。它完美兼顾了高并发控制台输出的原子性与实时操作系统对中断延迟的严苛要求。

---

## 🌟 核心特性与架构设计

### 1. 100% Linux 内核 Dmesg 日志规范
每条日志均包含 Linux 标准日志级别、纳秒级硬件时间戳、CPU 核心编号以及线程信息：
```text
<6>[    0.012345] [CPU0:sched-mon:1] cherry_vfs: registered /dev/console
<3>[    0.015678] [CPU1:pr_w1:2] disk_driver: read timeout
```
- `<level>`：保留标准 Linux 日志等级 `<0>`~`<7>`；
- `[seconds.microseconds]`：由硬件通用定时器（ARM Generic Timer `cntpct_el0`）直接驱动，微秒/纳秒级精准时间对齐；
- `[CPUx:task_name:tid]`：自动捕获执行打印的 CPU 核心、FreeRTOS 任务名称（或 `swapper`、`irq`）及任务 ID；
- 消息内容：自动过滤多余换行符并规整输出。

### 2. 多核并发打印无错乱（Multi-Core Non-Interleaved Logging）
在多核并发环境下，传统的逐字符 UART 输出会导致不同核心的字符交织错乱。`CherryPrintk` 引入结构化环形记录缓冲区（Record Ring Buffer）：
- 每次 `printk` 调用先在局部栈中完成格式化，整条记录作为一个独立的原子事件入队；
- 控制台输出引擎逐行（Line-by-Line）完整发送至硬件终端，**不同核心间的日志永远整行输出，绝对不出现字符交错**。

### 3. 关中断时间极短（< 100ns），绝不影响硬件定时器
传统日志模块在关中断情况下等待慢速 UART FIFO 发送（100 字节在 115200 波特率下需 ~8.6 毫秒），会导致定时器中断严重丢帧甚至系统假死。`CherryPrintk` 采用极简两阶段分离架构：
1. **入队阶段（Enqueue Phase）**：仅需微秒级以下的极短自旋锁（`spin_lock_irqsave`）将元数据拷贝入 RAM 环形队列，耗时小于 **100 纳秒**，随即立刻恢复中断；
2. **输出阶段（Console Drain Phase）**：在**完全开启中断（Interrupts ENABLED）**的状态下执行 UART 发送；即使串口处于发送等待状态，硬件定时器中断（`GTIMER` / `IRQ_TIMER`）仍能随到随响应，抖动趋近于零！

### 4. 纯 Linux 标准 API
```c
int printk(const char *fmt, ...);
int vprintk(const char *fmt, va_list args);

pr_emerg(fmt, ...);
pr_alert(fmt, ...);
pr_crit(fmt, ...);
pr_err(fmt, ...);
pr_warn(fmt, ...);
pr_notice(fmt, ...);
pr_info(fmt, ...);
pr_debug(fmt, ...);
```

---

## 🧪 自动化验收断言

在系统启动时，`CherryPrintk` 自动进行基准测试与多核并发压力测试：
```text
<6>[    0.001200] [CPU0:sched-mon:1] ==> Starting CherryPrintk Multi-Core Acceptance Test Suite...
<6>[    0.002340] [CPU0:sched-mon:1] [printk-verify] basic=PASS multicore=PASS irq_safe=PASS all=PASS
```
