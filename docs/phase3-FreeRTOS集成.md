# Phase 3: FreeRTOS V11.x 集成 — 抢占式多任务调度

## 1. 目标

在 Phase 2（GICv3 中断 + Generic Timer）的基础上，集成 FreeRTOS V11.x 内核：

1. **选择正确的 FreeRTOS 移植** — ARM_AARCH64_SRE（GICv3 系统寄存器接口）
2. **替换裸机中断框架** — FreeRTOS 接管向量表、中断分发、上下文切换
3. **Tick 时钟对接** — Generic Timer 通过 FreeRTOS 宏接入调度器
4. **验证抢占式调度** — 两个任务以不同频率打印

最终目标是证明 FreeRTOS 抢占式调度在 QEMU virt AArch64 上正确工作。

---

## 2. 架构总览

```
┌──────────────────────────────────────────────────────────────────┐
│                      FreeRTOS on QEMU virt                       │
│                                                                   │
│  应用层                                                           │
│  ┌─────────┐  ┌─────────┐  ┌─────────┐                          │
│  │ TaskA   │  │ TaskB   │  │ Idle    │  ← 抢占式调度            │
│  │ 1s 打印 │  │ 700ms   │  │         │                          │
│  └────┬────┘  └────┬────┘  └────┬────┘                          │
│       │            │            │                                 │
│       ▼            ▼            ▼                                 │
│  ┌─────────────────────────────────────┐                         │
│  │        FreeRTOS V11.x 内核          │                         │
│  │  Scheduler / Context Switch / Queue │                         │
│  │  heap_4 (动态分配)                   │                         │
│  └──────────────┬──────────────────────┘                         │
│                 │                                                  │
│  ┌──────────────▼──────────────────────┐                         │
│  │     ARM_AARCH64_SRE 移植层          │                         │
│  │  portASM.S (上下文保存/恢复)         │                         │
│  │  port.c    (Tick, 临界区, 优先级)    │                         │
│  │  编译标志: -DGUEST -DQEMU           │                         │
│  └──────────────┬──────────────────────┘                         │
│                 │                                                  │
│  ┌──────────────▼──────────────────────┐                         │
│  │          硬件抽象 (HAL)              │                         │
│  │  gic.c + gtimer.c + uart.c          │                         │
│  │  vConfigureTickInterrupt()          │                         │
│  │  vClearTickInterrupt()              │                         │
│  │  vApplicationIRQHandler()           │                         │
│  └──────────────┬──────────────────────┘                         │
│                 │                                                  │
│  ┌──────────────▼──────────────────────┐                         │
│  │           QEMU virt 硬件            │                         │
│  │  GICv3 + EL1 Physical Timer         │                         │
│  └─────────────────────────────────────┘                         │
└──────────────────────────────────────────────────────────────────┘
```

**中断处理流程（FreeRTOS 接管后）**：

```
Timer 倒计时 → GIC INTID 30 pending → CPU IRQ 异常
    → VBAR_EL1 + 0x080 或 +0x280 → FreeRTOS_IRQ_Handler (portASM.S)
    → 保存上下文 (x0-x30, SPSR, ELR)
    → 读 ICC_IAR1_EL1 (应答)
    → vApplicationIRQHandler(INTID) (app_irq.c → 分发)
        → FreeRTOS_Tick_Handler (port.c)
        → vClearTickInterrupt() (gtimer.c → 重载定时器)
        → xTaskIncrementTick() (可能触发上下文切换)
    → 写 ICC_EOIR1_EL1 (EOI)
    → 检查上下文切换 → vTaskSwitchContext()
    → 恢复新任务上下文 → eret
```

---

## 3. FreeRTOS 移植选择

### 3.1 ARM_AARCH64_SRE 移植

FreeRTOS 官方提供了多个 AArch64 移植，我们选择 **ARM_AARCH64_SRE**：

| 移植 | 中断接口 | 适用场景 |
|------|----------|----------|
| ARM_AARCH64 | GICv2 MMIO (GICC) | 旧平台 |
| **ARM_AARCH64_SRE** | **GICv3 系统寄存器 (ICC_*)** | **现代平台 ✅** |

SRE = System Register Enable，即 GICv3 的系统寄存器接口。这正是我们在 Phase 2 中使用的 `ICC_IAR1_EL1` / `ICC_EOIR1_EL1` 接口。

### 3.2 编译标志

```
-DGUEST   → 移植使用 EL1 寄存器 (SPSR_EL1, ELR_EL1, VBAR_EL1, SVC)
-DQEMU    → 跳过 port.c 中 ICC_RPR_EL1 断言（QEMU 不实现此寄存器）
```

**为什么用 GUEST？** 在裸机 `-kernel` 启动模式下，我们运行在 EL1（非虚拟化），使用 SVC 而非 SMC 进行系统调用。portASM.S 通过 `#ifdef GUEST` 选择 EL1 或 EL3 寄存器。

**为什么用 QEMU？** FreeRTOS 的 `FreeRTOS_Tick_Handler` 在清除 tick 后会读 `ICC_RPR_EL1` 做断言，但 QEMU 的 GICv3 实现不支持此寄存器。`-DQEMU` 跳过这个检查。

### 3.3 移植提供的核心功能

portASM.S 定义了三个关键符号：

```asm
FreeRTOS_IRQ_Handler   ← IRQ 入口：保存上下文、读 IAR、调 vApplicationIRQHandler、写 EOI、可能切换任务
FreeRTOS_SWI_Handler   ← SVC 入口：保存上下文、调 vTaskSwitchContext、恢复新任务
vPortRestoreTaskContext ← 首次启动：安装向量表、恢复第一个任务上下文、eret
```

port.c 提供：
- `xPortStartScheduler()` — 禁用 IRQ、调 `vConfigureTickInterrupt()`、调 `vPortRestoreTaskContext()`
- `FreeRTOS_Tick_Handler()` — 清 tick、调 `xTaskIncrementTick()`
- `pxPortInitialiseStack()` — 初始化任务栈帧（模拟已保存的寄存器状态）
- `vPortEnterCritical()` / `vPortExitCritical()` — 临界区（PMR 屏蔽）

---

## 4. 代码详解

### 4.1 FreeRTOS 配置 — FreeRTOSConfig.h

```c
/* GICv3 优先级配置 */
#define configUNIQUE_INTERRUPT_PRIORITIES        32  /* QEMU virt: 32 个优先级 */
#define configMAX_API_CALL_INTERRUPT_PRIORITY     18  /* > 32/2=16 */
```

**优先级机制**：GICv3 有 32 个优先级（0 最高，31 最低）。FreeRTOS 的规则：
- 优先级 0-17：高于 `configMAX_API_CALL_INTERRUPT_PRIORITY`，不能调用 FreeRTOS API
- 优先级 18-30：可以安全调用 `...FromISR()` API
- 优先级 31：保留给 tick 中断（`portLOWEST_INTERRUPT_PRIORITY`）

对应 `portPRIORITY_SHIFT = 3`（因为 32 个优先级），GIC 优先级寄存器值 = 优先级 << 3。

Tick 中断优先级 = `portLOWEST_USABLE_INTERRUPT_PRIORITY << portPRIORITY_SHIFT` = 30 << 3 = 240 = **0xF0**。

```c
/* Tick 中断钩子 — 连接 Generic Timer 和 FreeRTOS */
extern void vConfigureTickInterrupt(void);
extern void vClearTickInterrupt(void);
#define configSETUP_TICK_INTERRUPT()   vConfigureTickInterrupt()
#define configCLEAR_TICK_INTERRUPT()   vClearTickInterrupt()
```

这两个宏让 FreeRTOS 移植层调用我们的硬件定时器驱动，实现平台无关的 tick 接入。

```c
/* 内存配置 */
#define configTOTAL_HEAP_SIZE          ((size_t)(128 * 1024))  /* 128KB */
#define configSUPPORT_DYNAMIC_ALLOCATION  1                     /* 使用 heap_4 */
```

heap_4 支持分配和释放，适合动态创建任务。

```c
/* 栈深度类型 */
#define configSTACK_DEPTH_TYPE         size_t     /* 64 位 */
#define configMINIMAL_STACK_SIZE       ((uint16_t)256)  /* 256 * 8 = 2KB */
```

**重要**：AArch64 上 `size_t` 是 8 字节，所以 `256` 字 = 2048 字节栈。

### 4.2 FreeRTOS 向量表 — freertos_vectors.S

portASM.S 要求 `_freertos_vector_table` 符号（2KB 对齐），提供 FreeRTOS 的中断入口：

```asm
.section ".text.vectors", "ax", %progbits
.align 11                              /* 2048 字节对齐 */

.global _freertos_vector_table

_freertos_vector_table:
    /* Current EL with SP_EL0 — FreeRTOS 任务使用 SP_EL0 */
    ventry  FreeRTOS_SWI_Handler       /* +0x000 Sync (SVC = task yield) */
    ventry  FreeRTOS_IRQ_Handler       /* +0x080 IRQ  (GICv3 interrupts)  */
    ventry  hang_stub                  /* +0x100 FIQ */
    ventry  hang_stub                  /* +0x180 SError */

    /* Current EL with SP_ELx (SP_EL1) — 中断处理期间使用 */
    ventry  FreeRTOS_SWI_Handler       /* +0x200 Sync */
    ventry  FreeRTOS_IRQ_Handler       /* +0x280 IRQ  */
    ventry  hang_stub                  /* +0x300 FIQ */
    ventry  hang_stub                  /* +0x380 SError */

    /* Lower EL — 未使用 */
    ...
```

**关键设计**：两组入口（SP_EL0 和 SP_ELx）都指向相同的 FreeRTOS 处理器。

这是因为在调试中发现的第三个关键 Bug（详见第 5 节）：
- FreeRTOS 任务运行在 `SPSEL=0`（SP_EL0）
- 异常发生时，CPU 使用 SP_EL0 组的向量（+0x080）
- 如果只配置 SP_ELx 组（+0x280），IRQ 会进入空的 hang_stub

### 4.3 应用 IRQ 分发 — app_irq.c

FreeRTOS 的 portASM.S 在读 `ICC_IAR1_EL1` 后调用 `vApplicationIRQHandler(uint32_t ulICCIAR)`，我们需要根据 INTID 分发：

```c
#define MAX_IRQ_ENTRIES 256
typedef void (*isr_fn_t)(void);
static isr_fn_t isr_table[MAX_IRQ_ENTRIES];

void app_irq_install(uint32_t irq, isr_fn_t handler)
{
    if (irq < MAX_IRQ_ENTRIES)
        isr_table[irq] = handler;
}

void vApplicationIRQHandler(uint32_t ulICCIAR)
{
    uint32_t irq = ulICCIAR;
    if (irq >= 1020) return;           /* Spurious */
    if (irq < MAX_IRQ_ENTRIES && isr_table[irq])
        isr_table[irq]();
}
```

**注意**：这里**不写 EOI**（`ICC_EOIR1_EL1`）。portASM.S 在 `vApplicationIRQHandler` 返回后才写 EOI。如果我们在分发中写 EOI，会导致中断提前完成，FreeRTOS 的上下文切换逻辑出错。

### 4.4 定时器驱动对接 — gtimer.c

Phase 2 的定时器驱动需要修改以对接 FreeRTOS：

```c
/* FreeRTOS tick handler (定义在 port.c) */
extern void FreeRTOS_Tick_Handler(void);

/*
 * vConfigureTickInterrupt() — FreeRTOS 调度器启动时调用
 * 通过 configSETUP_TICK_INTERRUPT() 宏接入
 */
void vConfigureTickInterrupt(void)
{
    write_sysreg("cntp_ctl_el0", 0);          /* 禁用定时器 */
    write_sysreg("cntp_tval_el0", timer_step); /* 设置倒计时 */

    gic_set_priority(IRQ_TIMER, 0xF0);         /* 最低可用优先级 */
    gic_enable_irq(IRQ_TIMER);                  /* GIC 使能 */
    app_irq_install(IRQ_TIMER, FreeRTOS_Tick_Handler); /* 注册 ISR */

    write_sysreg("cntp_tval_el0", timer_step); /* 重载 */
    write_sysreg("cntp_ctl_el0", 0x1);         /* 启动 */
}

/*
 * vClearTickInterrupt() — FreeRTOS_Tick_Handler 中调用
 * 通过 configCLEAR_TICK_INTERRUPT() 宏接入
 */
void vClearTickInterrupt(void)
{
    write_sysreg("cntp_tval_el0", timer_step);  /* 重载定时器 */
}
```

**Tick 频率从 100Hz → 1000Hz**：FreeRTOS 标准默认 `configTICK_RATE_HZ = 1000`。

**优先级 0xF0 的含义**：

```
portLOWEST_INTERRUPT_PRIORITY     = 31  (最低优先级，GIC 保留)
portLOWEST_USABLE_INTERRUPT_PRIORITY = 30
portLOWEST_USABLE << portPRIORITY_SHIFT = 30 << 3 = 240 = 0xF0
```

Tick 中断使用最低可用优先级，确保 FreeRTOS 临界区（通过 PMR 屏蔽）能正确阻塞 tick。

### 4.5 主程序 — main.c

```c
int main(void)
{
    uart_init();
    uart_printf("=== Phase 3: FreeRTOS V11.x on EL%d ===\r\n", el);

    gic_init(0);                     /* GICv3 初始化 */
    uart_puts("[1] GICv3 initialized\r\n");

    gtimer_init();                   /* 计算 timer_step，但不启动 */

    xTaskCreate(task_print_a, "TaskA", 256, NULL, 2, NULL);
    xTaskCreate(task_print_b, "TaskB", 256, NULL, 2, NULL);
    uart_puts("[2] Tasks created\r\n");

    uart_puts("[3] Starting scheduler...\r\n");
    vTaskStartScheduler();           /* 永不返回 */

    /* 不应该到达这里 */
    while (1) asm volatile("wfi");
}
```

**初始化顺序**：

1. `gic_init(0)` — GICv3 三层初始化（Distributor + Redistributor + CPU Interface）
2. `gtimer_init()` — 计算倒计数值，但**不启动定时器**
3. `xTaskCreate()` — 创建任务（分配栈和 TCB）
4. `vTaskStartScheduler()` — 启动调度器（内部会调 `vConfigureTickInterrupt()`）

**注意**：这里**不安装向量表**（不调 `interrupt_init()`）。FreeRTOS 在 `vPortRestoreTaskContext()` 中将 `_freertos_vector_table` 写入 `VBAR_EL1`。

**两个演示任务**：

```c
static void task_print_a(void *pvParameters)
{
    int count = 0;
    for (;;) {
        uart_printf("[TaskA] count=%d, tick=%d\r\n",
                     count, (uint32_t)xTaskGetTickCount());
        count++;
        vTaskDelay(pdMS_TO_TICKS(1000));  /* 每 1 秒 */
    }
}

static void task_print_b(void *pvParameters)
{
    int count = 0;
    for (;;) {
        uart_printf("[TaskB] count=%d, tick=%d\r\n",
                     count, (uint32_t)xTaskGetTickCount());
        count++;
        vTaskDelay(pdMS_TO_TICKS(700));   /* 每 700ms */
    }
}
```

### 4.6 裸机支持文件

#### include/stddef.h — 修正 size_t 定义

这是整个集成过程中最隐蔽的 Bug 的修复（详见第 5 节）：

```c
#ifndef _STDDEF_H
#define _STDDEF_H

/* 阻止 GCC 的 stddef.h 重新定义 */
#define ___int_size_t_h
#define _SIZE_T
#define __size_t

typedef long unsigned int  size_t;    /* 必须 8 字节！ */
typedef long int           ptrdiff_t;

#ifndef NULL
#define NULL ((void *)0)
#endif

typedef unsigned int wchar_t;
#endif
```

#### include/stdlib.h — FreeRTOS 兼容

```c
#ifndef _STDLIB_H
#define _STDLIB_H
#include <stddef.h>
#endif
```

FreeRTOS 内核源码无条件包含 `<stdlib.h>`，但只使用 `NULL`。在 `-ffreestanding` 模式下没有标准库，所以我们提供一个最小的头文件。

#### bsp/src/string.c — 基础内存操作

```c
void *memset(void *s, int c, size_t n) { ... }
void *memcpy(void *dest, const void *src, size_t n) { ... }
```

FreeRTOS heap_4 使用 `memset` 和 `memcpy`，裸机环境需要自己实现。

### 4.7 构建系统 — Makefile

```makefile
FREERTOS_DIR := FreeRTOS-Kernel

CFLAGS := ... -DGUEST -DQEMU \
          -Iinclude \
          -I$(FREERTOS_DIR)/include \
          -I$(FREERTOS_DIR)/portable/GCC/ARM_AARCH64_SRE

# FreeRTOS 内核源文件
SRCS_C += $(FREERTOS_DIR)/tasks.c \
          $(FREERTOS_DIR)/queue.c \
          $(FREERTOS_DIR)/list.c \
          $(FREERTOS_DIR)/timers.c \
          $(FREERTOS_DIR)/event_groups.c \
          $(FREERTOS_DIR)/stream_buffer.c \
          $(FREERTOS_DIR)/portable/GCC/ARM_AARCH64_SRE/port.c \
          $(FREERTOS_DIR)/portable/MemMang/heap_4.c

# FreeRTOS 移植汇编
SRCS_S += $(FREERTOS_DIR)/portable/GCC/ARM_AARCH64_SRE/portASM.S
```

**注意**：使用 `gcc` 作为汇编器和链接器（而非 `as`/`ld`），因为 portASM.S 需要 C 预处理器处理 `#ifdef GUEST`。

---

## 5. 调试过程中发现的关键 Bug

### Bug 1：StackType_t 只有 4 字节（size_t 定义错误）

**现象**：调度器启动后 ERET 到地址 0x00000000，触发 Undefined Instruction 异常。

**根因**：`pxPortInitialiseStack()` 中指针以 `sizeof(StackType_t)` 步进递减。在 AArch64 上，`StackType_t = size_t` 应该是 8 字节，但实际编译为 4 字节：

```asm
; 错误：sub x0, x0, #0x4  (4字节步进，应该是 8字节)
; 正确：sub x0, x0, #0x8
```

追踪到原因：GCC 交叉编译器的 `stddef.h` 在 `-Iinclude` 搜索路径下，sysroot 干扰导致 `size_t` 被定义为 `unsigned`（4 字节）而非 `long unsigned int`（8 字节）。

**验证方法**：

```bash
# 预处理 port.c，查看 size_t 定义
aarch64-linux-gnu-gcc -E ... -dD port.c | grep "typedef.*size_t"
# 输出：typedef unsigned size_t;  ← 错误！应该是 long unsigned int
```

**修复**：提供正确的 `include/stddef.h`，显式定义为 `typedef long unsigned int size_t`，并设置防护宏阻止 GCC 内置 stddef.h 重新定义。

### Bug 2：IRQ 进入错误的向量入口（SP_EL0 vs SP_ELx）

**现象**：任务启动后，第一次 IRQ 在 PC=VBAR+0x080（SP_EL0 IRQ）触发，进入空的 `hang_stub`，系统死锁。

**根因**：ARMv8-A 向量表有两组 "Current EL" 入口：
- **SP_EL0 组**（+0x000~0x180）：使用 SP_EL0，当 `SPSEL=0` 时触发
- **SP_ELx 组**（+0x200~0x380）：使用 SP_ELx，当 `SPSEL=1` 时触发

FreeRTOS 任务运行在 `SPSEL=0`（SP_EL0），因为 `portSAVE_CONTEXT` 宏的第一条指令就是 `MSR SPSEL, #0`。所以任务执行期间发生的 IRQ 进入 SP_EL0 组的 +0x080。

最初只配置了 SP_ELx 组（+0x280），SP_EL0 组指向空的 `hang_stub`。

**修复**：两组入口都指向相同的 FreeRTOS 处理器：

```asm
_freertos_vector_table:
    /* SP_EL0 组 — 任务运行时使用 */
    ventry  FreeRTOS_SWI_Handler  /* +0x000 */
    ventry  FreeRTOS_IRQ_Handler  /* +0x080 ← 关键！ */
    ...
    /* SP_ELx 组 — 中断处理期间使用 */
    ventry  FreeRTOS_SWI_Handler  /* +0x200 */
    ventry  FreeRTOS_IRQ_Handler  /* +0x280 */
```

**诊断技巧**：使用 QEMU 的 `-d int` 选项记录异常：

```bash
qemu-system-aarch64 ... -d int 2>/dev/null | head -30
# 看到 Taking exception 3 (IRQ) to EL1
# ... target EL1 SP0  ← SP0 = SP_EL0 组！
```

### Bug 3：启动栈太小

**现象**：`vTaskStartScheduler()` 执行期间栈溢出，导致未定义行为。

**根因**：Phase 2 的启动栈只有 4096 字节。FreeRTOS 调度器启动过程（创建 Idle/Timer 任务、初始化堆、首次上下文切换）需要更多栈空间。

**修复**：将 `startup.S` 中的栈从 4096 增大到 8192：

```asm
.section .bss.stack
stack_top:
    .space 8192           /* 8KB 启动栈 */
```

同时修正 SP 偏移：

```asm
ldr x20, =stack_top
add x20, x20, #8192     /* 之前错误地是 #4096 */
mov sp, x20
```

---

## 6. 启动流程详解

```
start   startup.S                    说明
──────  ─────────                    ─────
EL2 入口 → el2_setup                 检测 CurrentEL = 2
         → ICC_SRE_EL2 |= 0x7       启用 GICv3 系统寄存器
         → HCR_EL2 = (1<<31)        RW=1，EL1 使用 AArch64
         → SCTLR_EL1, CPACR_EL1     配置 EL1 状态
         → ELR_EL2 = el1_entry      设置返回地址
         → SPSR_EL2 = 0x3C5         EL1h, DAIF 全部 masked
         → eret                      → el1_entry @ EL1

EL1 入口 → el1_setup / el1_entry     检测 CurrentEL = 1 或从 EL2 降级
         → spsel #1, SP = stack_top + 8192
         → common_init               清零 BSS
         → daifclr #0x2              解除 IRQ 屏蔽
         → bl main

main()   → uart_init()               PL011 初始化
         → gic_init(0)               GICv3 三层初始化
         → gtimer_init()             计算 timer_step = 62500000/1000 = 62500
         → xTaskCreate(task_print_a) 分配栈 + TCB，初始化栈帧
         → xTaskCreate(task_print_b) 同上
         → vTaskStartScheduler()     ↓ 进入 FreeRTOS

FreeRTOS → xPortStartScheduler()     port.c
         → portDISABLE_INTERRUPTS()  DAIF.I = 1
         → vConfigureTickInterrupt() 配置定时器优先级 0xF0，注册 ISR，启动定时器
         → vPortRestoreTaskContext() portASM.S
         → VBAR_EL1 = _freertos_vector_table  安装向量表
         → 从 pxCurrentTCB 恢复上下文   加载 x0-x30, SPSR, ELR
         → eret                       → 第一个任务开始执行
```

---

## 7. 运行结果

```bash
$ make clean && make
Cleaning...
AS  bsp/src/startup.S
AS  bsp/src/freertos_vectors.S
AS  kernel/port/portASM.S
CC  bsp/src/main.c
CC  bsp/src/uart.c
CC  bsp/src/gic.c
CC  bsp/src/gtimer.c
CC  bsp/src/app_irq.c
CC  bsp/src/string.c
CC  kernel/tasks.c
CC  kernel/queue.c
CC  kernel/list.c
CC  kernel/timers.c
CC  kernel/event_groups.c
CC  kernel/stream_buffer.c
CC  kernel/port/port.c
CC  kernel/MemMang/heap_4.c
LD  kernel.elf
OBJCOPY kernel.bin
```

三种 QEMU 配置全部通过：

```bash
# 配置 1: 默认 GICv3（Secure EL1）
$ qemu-system-aarch64 -machine virt,gic-version=3 -cpu cortex-a53 -smp 1 \
    -m 512 -kernel kernel.elf -nographic -monitor none -serial stdio
=== Phase 3: FreeRTOS V11.x on EL1 ===
[1] GICv3 initialized
[2] Tasks created
[3] Starting scheduler...
[TaskA] count=0, tick=1
[TaskB] count=0, tick=2
[TaskB] count=1, tick=702
[TaskA] count=1, tick=1001
[TaskB] count=2, tick=1403
[TaskA] count=2, tick=2002
...

# 配置 2: GICv3 + secure=off（NS EL1）
# 配置 3: GICv3 + virtualization=on（EL2→EL1 drop）
# 输出相同 ✅
```

**时间精度**：TaskB 每 700ms 打印，tick 值为 702/1403/2104...（±2 tick 误差），这在 FreeRTOS 抢占式调度中是正常的（tick 对齐误差 + 打印耗时）。

---

## 8. 文件清单

| 文件 | 状态 | 说明 | 行数 |
|------|------|------|------|
| `include/FreeRTOSConfig.h` | **新增** | FreeRTOS 配置 | ~88 |
| `bsp/src/freertos_vectors.S` | **新增** | FreeRTOS 向量表 | ~49 |
| `bsp/src/app_irq.c` | **新增** | 应用 IRQ 分发 | ~44 |
| `include/app_irq.h` | **新增** | IRQ 分发接口 | ~14 |
| `bsp/src/string.c` | **新增** | memset/memcpy | ~22 |
| `include/stddef.h` | **新增** | 修正 size_t 为 8 字节 | ~18 |
| `include/stdlib.h` | **新增** | FreeRTOS 兼容 | ~12 |
| `bsp/src/main.c` | 修改 | FreeRTOS 任务 + 钩子 | ~95 |
| `bsp/src/gtimer.c` | 修改 | 对接 FreeRTOS tick | ~89 |
| `include/gtimer.h` | 修改 | tick 钩子接口 | ~29 |
| `bsp/src/startup.S` | 修改 | 栈 4K→8K | ~97 |
| `Makefile` | 修改 | FreeRTOS 构建集成 | ~146 |
| `bsp/src/freertos_vectors.S` | 移除 | 被 freertos_vectors.S 替代 | — |
| `bsp/src/app_irq.c` | 移除 | 被 app_irq.c 替代 | — |
| `kernel/` | **克隆** | V11.x 官方内核 | — |

---

## 9. 经验总结

1. **size_t 必须是 8 字节**：AArch64 的 `StackType_t = size_t = portSTACK_TYPE`。如果 `size_t` 被错误定义为 4 字节，任务栈帧会错位，ERET 后 PC 为 0。GCC 交叉编译器的 `stddef.h` 可能因 sysroot 问题输出错误的 `typedef`，必须在 `-Iinclude` 路径下提供正确的覆盖。

2. **SP_EL0 vs SP_ELx 向量入口**：FreeRTOS 任务使用 `SPSEL=0`（SP_EL0），异常时 CPU 自动使用 SP_EL0 组的向量。必须同时配置两组入口指向相同的处理器。

3. **EOI 由 portASM.S 负责**：`vApplicationIRQHandler` 中不能写 `ICC_EOIR1_EL1`。portASM.S 在 C 函数返回后才写 EOI，并在之后检查是否需要上下文切换。提前写 EOI 会导致 GIC 状态不一致。

4. **`-DGUEST` 选择 EL1 寄存器**：裸机 EL1 使用 SVC（非 SMC）、SPSR_EL1（非 SPSR_EL3）、ELR_EL1（非 ELR_EL3）。这个标志影响 portASM.S 中所有异常寄存器的选择。

5. **FreeRTOS 接管向量表**：在 `vTaskStartScheduler()` 之前不要安装自定义向量表。FreeRTOS 在 `vPortRestoreTaskContext()` 中将 `_freertos_vector_table` 写入 `VBAR_EL1`。

6. **优先级 0xF0 不是任意值**：`portLOWEST_USABLE_INTERRUPT_PRIORITY (30) << portPRIORITY_SHIFT (3) = 240 = 0xF0`。FreeRTOS 通过 PMR 屏蔽低优先级中断实现临界区，tick 必须在可屏蔽的范围内。

---

## 10. 下一步

Phase 4：FreeRTOS SMP 多核支持。
- `configNUMBER_OF_CORES > 1`
- 次核启动（Parked CPU → Secondary init → FreeRTOS SMP scheduler）
- 每核 GICR 初始化 + 每核定时器
- 演示：任务在不同核心上并行运行
