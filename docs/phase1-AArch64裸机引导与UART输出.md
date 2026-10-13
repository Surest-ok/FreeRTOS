# Phase 1: ARMv8-A (AArch64) 裸机引导与 PL011 UART 输出

## 1. 目标与背景

本阶段是整个 FreeRTOS SMP 操作系统移植的最基石阶段。在嵌入式与多核实时操作系统研发中，任何上层调度算法、IPC 机制和驱动框架，都必须建立在一个稳定、可靠的裸机运行环境之上。

本阶段的目标是在 **QEMU virt AArch64**（ARM Cortex-A53）虚拟化平台上，实现从 CPU 上电复位到 C 语言运行时环境的最小裸机启动系统，并在控制台通过 PL011 UART 驱动输出日志：

- **裸机引导（Bootstrapping）**：编写汇编启动代码 `startup.S`，完成系统初始化、CPU 核心识别与异常级别配置
- **栈与内存布局**：设计并编写链接脚本 `linker.ld`，规划代码段、数据段、BSS 段和 Per-Core 独立栈空间
- **控制寄存器配置**：配置 ARMv8-A 系统控制寄存器 `SCTLR_EL1`，确保在无页表环境下关闭 MMU 与 Cache，安全运行
- **PL011 串口驱动**：实现 ARM PrimeCell PL011 UART 驱动与轻量级格式化打印输出（`uart_printf`），验证裸机环境运行正常

---

## 2. 前置知识与硬件模型

### 2.1 ARMv8-A 异常级别与执行状态

ARMv8-A 体系结构定义了 4 个特权级别（Exception Levels, EL）：

```
┌───────────────────────────────────────────────┐
│  EL0: 用户态应用 (User Applications)            │
├───────────────────────────────────────────────┤
│  EL1: 操作系统内核 (OS Kernel - FreeRTOS 运行于此)│
├───────────────────────────────────────────────┤
│  EL2: Hypervisor 虚拟化层 (QEMU virt 可选支持) │
├───────────────────────────────────────────────┤
│  EL3: Secure Monitor 安全监控模式 (TrustZone)   │
└───────────────────────────────────────────────┘
```

- **执行状态**：采用 64 位执行状态（AArch64），拥有 31 个 64 位通用寄存器（`X0`~`X30`）、程序计数器（`PC`）以及独立的栈指针（`SP_EL0`、`SP_EL1` 等）。
- **QEMU 启动行为**：当使用 `-kernel` 选项直接加载 ELF 二进制文件时，QEMU virt 模拟器通常直接将 CPU 置于 **EL1**（或当开启 `virtualization=on` 时置于 **EL2**）启动，跳过复杂的固件流程。

### 2.2 核心标识（MPIDR_EL1）

在多核 SMP 系统中，每个 CPU 核心在启动时必须能够确定自己的身份：

```
MPIDR_EL1 (Multiprocessor Affinity Register):
 63      39 38    32 31 30 24 23    16 15     8 7      0
┌──────────┬────────┬──┬─────┬────────┬────────┬────────┐
│ Res0     │  Aff3  │U │ Res0│  Aff2  │  Aff1  │  Aff0  │
└──────────┴────────┴──┴─────┴────────┴────────┴────────┘
```

- `Aff0`（Bits [7:0]）：通常表示簇（Cluster）内的 CPU 核心编号（0, 1, 2, ...）。
- `Aff1`（Bits [15:8]）：表示簇编号。
- 主核（Primary Core 0）的 `Aff2:Aff1:Aff0` 均为 0。在 Phase 1 阶段，主核负责进行全局硬件初始化、BSS 清零并进入 `main()`；而副核（Secondary Cores）暂时置于等待状态（WFE）。

### 2.3 QEMU virt 平台内存与外设映射

QEMU `virt` 平台为 ARMv8-A 提供了标准的物理地址空间映射：

| 物理地址范围 | 外设 / 内存 | 作用 |
|-------------|------------|------|
| `0x08000000` - `0x0800FFFF` | GICv2 Distributor / GICv3 GICD | 中断分发器（Phase 2 使用） |
| `0x09000000` - `0x09000FFF` | **PL011 UART 0** | 串口输出（Phase 1 核心） |
| `0x40000000` - `0x7FFFFFFF` | **System RAM (DRAM, 512MB+)** | 代码、数据与操作系统堆栈 |

QEMU 在加载内核时，默认将镜像加载到 RAM 起始地址 **`0x40000000`**。

### 2.4 ARM PrimeCell PL011 UART 控制器模型

PL011 是 ARM 架构上广泛采用的标准 UART IP，基地址为 `0x09000000`。其常用 MMIO 寄存器定义如下：

```
UART0_BASE = 0x09000000
├── +0x00: UARTDR   (Data Register)          ── 读写数据缓冲区 (低8位为字符)
├── +0x18: UARTFR   (Flag Register)          ── 状态标志 (Bit 5: TXFF 满, Bit 3: BUSY 忙)
├── +0x24: UARTIBRD (Integer Baud Rate Div)  ── 整数波特率分频
├── +0x28: UARTFBRD (Fract Baud Rate Div)    ── 小数波特率分频
├── +0x2C: UARTLCR_H(Line Control Register)  ── 线控制 (Bit 4: FEN 使能 FIFO, Bits 6:5: 字长)
├── +0x30: UARTCR   (Control Register)       ── 控制使能 (Bit 0: UARTEN, Bit 8: TXE, Bit 9: RXE)
└── +0x44: UARTICR  (Interrupt Clear Reg)    ── 中断清除
```

---

## 3. 逐步实施与代码详解

### Step 1: 内存布局与链接脚本 (`linker.ld`)

链接脚本负责将编译后的目标文件组装成正确的物理内存镜像：

```ld
ENTRY(_start)

SECTIONS
{
    /* QEMU virt 平台 RAM 起始地址 */
    . = 0x40000000;

    /* 启动入口代码与只读数据 */
    .text.entrypoint : {
        KEEP(*(.text.entrypoint))
        *(.text*)
        *(.rodata*)
    }

    /* 已初始化的可读写数据段 */
    .data : {
        *(.data*)
    }

    /* 未初始化数据段（BSS），需在启动时清零 */
    .bss : {
        __bss_start = .;
        *(.bss*)
        *(COMMON)
        . = ALIGN(16);
        __bss_end = .;
    }

    /* 栈空间（向上预留，栈向下生长） */
    .stack (NOLOAD) : {
        . = ALIGN(16);
        stack_top = .;
        . = . + (8192 * 4);  /* 为多核预留栈空间，每核 8KB */
    }

    _end = .;
}
```

**关键设计点**：
- 入口符号为 `_start`，放置在 `.text.entrypoint` 段最前端，确保位于镜像首地址 `0x40000000`。
- `stack_top` 标记栈底物理地址，后文将详细分析栈指针设置规则。

---

### Step 2: 启动汇编 (`startup.S`)

启动汇编是 CPU 上电执行的第一行指令，负责将处理器状态从未知状态配置为标准 C 语言运行环境：

```assembly
#define STACK_SIZE 8192

.section .text.entrypoint
.global _start

_start:
    /* 1. 读取多核亲和性寄存器 MPIDR_EL1 */
    mrs     x5, MPIDR_EL1
    and     x6, x5, #0x00FFFFFF       /* 检查 Aff2:Aff1:Aff0 是否为 0 */
    cbnz    x6, park_secondary        /* 副核进入睡眠等待 */

    /* === 主核（Core 0）初始化流程 === */

    /* 2. 检查当前异常级别 CurrentEL */
    mrs     x10, CurrentEL
    lsr     x10, x10, #2
    and     x10, x10, #3
    cmp     x10, #2
    b.eq    el2_drop_to_el1           /* 如果当前在 EL2，则降级至 EL1 */

el1_init:
    /* 3. 设置主核栈指针 SP_EL1 */
    ldr     x20, =stack_top
    mov     x7, #STACK_SIZE
    add     x20, x20, x7              /* SP = stack_top + STACK_SIZE (栈向下生长) */
    msr     spsel, #1                 /* 使用 SP_EL1 作为当前栈指针 */
    mov     sp, x20

    /* 4. 浮点/SIMD 单元使能 (CPACR_EL1) */
    mrs     x6, CPACR_EL1
    orr     x6, x6, #0x00F00000       /* 允许 EL1/EL0 访问 FP/SIMD 指令 */
    msr     CPACR_EL1, x6

    /* 5. 配置系统控制寄存器 SCTLR_EL1 */
    /* 0x30C50838 包含了 ARM 架构规定的保留置 1 位 */
    /* 明确关闭 MMU (Bit 0) 和 D-Cache (Bit 2)，因当前尚未构建页表 */
    ldr     x6, =0x30C50838
    msr     SCTLR_EL1, x6
    isb

    /* 6. 清空 BSS 段（8 字节对齐快速清零） */
    ldr     x0, =__bss_start
    ldr     x1, =__bss_end
1:  cmp     x0, x1
    b.ge    bss_done
    str     xzr, [x0], #8
    b       1b
bss_done:

    /* 7. 跳转至 C 语言主函数 */
    bl      main

    /* 兜底死循环 */
1:  wfe
    b       1b

/* 副核暂停等待 mailbox */
park_secondary:
1:  wfe
    b       1b
```

---

### Step 3: PL011 UART 驱动与格式化输出 (`uart.c`)

在 QEMU virt 平台上，PL011 模拟驱动不需要严格根据物理晶振分频配置波特率，只需正确开启 FIFO 并使能发送和接收单元：

```c
#include "platform.h"
#include "uart.h"
#include <stdarg.h>

#define UART_REG(offset) (*(volatile uint32_t *)(UART0_BASE + (offset)))

void uart_init(void)
{
    /* 1. 先关闭 UART 进行配置 */
    UART_REG(UART0_CR_OFFSET) = 0;

    /* 2. 清除所有未决中断 */
    UART_REG(UART0_ICR_OFFSET) = 0x7FF;

    /* 3. 配置线控制寄存器 LCRH：
     *    Bit 4 (FEN) = 1: 使能 FIFO
     *    Bits 6:5 (WLEN) = 11: 8 位字长，无校验，1 停止位
     */
    UART_REG(UART0_LCRH_OFFSET) = (1U << 4) | (3U << 5);

    /* 4. 使能 UART：
     *    Bit 0 (UARTEN) = 1: 使能控制器
     *    Bit 8 (TXE)    = 1: 使能发送器
     *    Bit 9 (RXE)    = 1: 使能接收器
     */
    UART_REG(UART0_CR_OFFSET) = (1U << 0) | (1U << 8) | (1U << 9);
}

void uart_putc(char c)
{
    /* 检查 Flag Register 中的 TXFF (Transmit FIFO Full) 位 */
    while (UART_REG(UART0_FR_OFFSET) & (1U << 5)) {
        __asm__ volatile("yield");
    }
    UART_REG(UART0_DR_OFFSET) = (uint32_t)c;
}

void uart_puts(const char *str)
{
    while (*str) {
        if (*str == '\n') {
            uart_putc('\r');  /* 自动补充回车符，防止终端阶梯显示 */
        }
        uart_putc(*str++);
    }
}
```

为了方便调试，实现了轻量级 `vsnprintf` 与 `uart_printf`：

```c
int uart_printf(const char *fmt, ...)
{
    char buf[256];
    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    if (len > 0) {
        uart_puts(buf);
    }
    return len;
}
```

---

### Step 4: C 语言入口验证 (`main.c`)

```c
#include "platform.h"
#include "uart.h"

static inline uint32_t get_current_core_id(void)
{
    uint64_t mpidr;
    __asm__ volatile("mrs %0, mpidr_el1" : "=r"(mpidr));
    return (uint32_t)(mpidr & 0xFFU);
}

int main(void)
{
    uart_init();

    uart_puts("\r\n========================================\r\n");
    uart_puts("  FreeRTOS SMP - Phase 1 Baremetal Boot \r\n");
    uart_puts("========================================\r\n");

    uart_printf("Platform: QEMU ARMv8-A virt (Cortex-A53)\r\n");
    uart_printf("Current CPU Core ID: %d\r\n", (int)get_current_core_id());
    uart_puts("[OK] UART driver and C runtime ready.\r\n");

    while (1) {
        __asm__ volatile("wfi");
    }
    return 0;
}
```

---

## 4. 遇到的问题与关键 Bug 分析

在 Phase 1 的调试开发中，曾遇到了四个非常隐蔽但典型的底层系统 Bug（见历史 commit `ef614e2`）：

### 4.1 SCTLR_EL1 误开 MMU 导致系统复位崩溃

- **现象**：汇编启动代码执行到 `msr SCTLR_EL1, x6` 后，QEMU 立即报错退出或触发 Fatal Exception 复位。
- **原因剖析**：ARMv8-A 的 `SCTLR_EL1` 寄存器中，Bit 0 为 `M`（MMU 使能位）。在裸机初期，系统尚未在物理内存中建立页表（Page Tables）和配置 `TTBR0_EL1` / `TCR_EL1`。如果在初始化 `SCTLR_EL1` 时不慎置位了 Bit 0，下一条指令取指立即被 MMU 拦截，由于缺少有效映射直接触发 Translation Fault 严重中止。
- **解决方案**：严格查阅 ARMv8-A 体系结构手册，写入固定掩码 `0x30C50838`（这些位在 ARM 规范中要求写 1），并且必须将 Bit 0（MMU）和 Bit 2（D-Cache）强制置 0。

### 4.2 栈指针偏移方向 BUG（Stack Pointer Overflow）

- **现象**：调用 `uart_init()` 返回时发生死循环或踩坏全局变量。
- **原因剖析**：在链接脚本中，`.stack` 段定义如下：
  ```ld
  .stack : { stack_top = .; . = . + STACK_SIZE; }
  ```
  原始汇编代码直接将栈指针设置为 `mov sp, stack_top`。然而，AArch64 采用的是 **Full Descending（向下生长）栈模型**！当发生函数调用 `bl` 或执行 `stp x29, x30, [sp, #-16]!` 时，`sp` 会向下减小，直接写到了 `stack_top` 下方的 `.bss` 或 `.data` 段，造成内存踩踏。
- **解决方案**：栈指针必须初始化为该段的高地址端（Top of Stack）：
  ```assembly
  ldr x20, =stack_top
  add x20, x20, #STACK_SIZE
  mov sp, x20
  ```

### 4.3 `vsnprintf` %d 数字反转指针越界 BUG

- **现象**：打印整数时，输出乱码或者后续打印内容被吞掉。
- **原因剖析**：在将整数转为字符串时，采用对 10 取模计算各位数字：
  ```c
  char *tmp = p;
  do {
      *p++ = '0' + (i % 10);
      i /= 10;
  } while (i > 0);
  /* 此时得到的是倒序数字，需原地双指针翻转 */
  while (tmp < p) {
      char c = *tmp;
      *tmp = *(p - 1);
      *(p - 1) = c;
      tmp++;
      p--;  /* BUG: 这里直接递减了字符串写指针 p！ */
  }
  ```
  双指针翻转结束后，写指针 `p` 缩退回了中间位置，导致后续追加的新字符直接覆盖了当前打印的数字。
- **解决方案**：引入独立的 `end` 指针保存末尾位置，翻转结束后将写指针恢复为 `p = end`：
  ```c
  char *end = p;
  while (tmp < end_swap) { ... }
  p = end; /* 恢复写指针位置 */
  ```

### 4.4 QEMU PL011 模拟器的波特率设置陷阱

- **现象**：真实硬件移植中常用的波特率除数计算（如 `baud_divisor = 100000000 / 115200`）在不同 QEMU 版本下可能因时钟假设不同产生意外行为。
- **分析**：QEMU 的 `hw/char/pl011.c` 模拟器将字符流直接重定向到虚拟控制台 `stdio`，并不严格依赖硬件波特率分频。精简这些不必要的 MMIO 写操作，仅配置必需的控制与 FIFO 寄存器，使驱动更加简洁健壮。

---

## 5. 验证与测试

### 5.1 编译与链接

使用 AArch64 交叉编译器进行编译：

```bash
aarch64-linux-gnu-gcc -mcpu=cortex-a53 -mgeneral-regs-only -mno-outline-atomics \
    -ffreestanding -nostdlib -nostartfiles -g -Iinclude -c src/startup.S -o startup.o
aarch64-linux-gnu-gcc -mcpu=cortex-a53 -mgeneral-regs-only -mno-outline-atomics \
    -ffreestanding -nostdlib -nostartfiles -g -Iinclude -c src/uart.c -o uart.o
aarch64-linux-gnu-gcc -mcpu=cortex-a53 -mgeneral-regs-only -mno-outline-atomics \
    -ffreestanding -nostdlib -nostartfiles -g -Iinclude -c src/main.c -o main.o
aarch64-linux-gnu-gcc -mcpu=cortex-a53 -nostdlib -T linker.ld startup.o uart.o main.o -lgcc -o kernel.elf
```

### 5.2 QEMU 仿真运行

执行启动命令：

```bash
qemu-system-aarch64 \
    -machine virt \
    -cpu cortex-a53 \
    -smp 2 \
    -m 512 \
    -kernel kernel.elf \
    -nographic \
    -serial stdio
```

### 5.3 预期终端输出

```text
========================================
  FreeRTOS SMP - Phase 1 Baremetal Boot 
========================================
Platform: QEMU ARMv8-A virt (Cortex-A53)
Current CPU Core ID: 0
[OK] UART driver and C runtime ready.
```

✅ **验证通过**：主核成功自引导，栈空间与 BSS 清零正确，PL011 串口正常收发字符，格式化输出准确无误。

---

## 6. 文件清单

| 文件 | 路径 | 说明 |
|------|------|------|
| `linker.ld` | `bsp/linker.ld` | 内存映射与段定义（RAM 基址 0x40000000） |
| `startup.S` | `bsp/src/startup.S` | 裸机复位入口、异常级别处理与栈指针设置 |
| `uart.c` | `bsp/src/uart.c` | PL011 串口驱动与 `vsnprintf` 打印引擎 |
| `uart.h` | `include/uart.h` | 串口驱动导出接口 |
| `platform.h`| `include/platform.h` | QEMU virt 外设基地址与硬件宏定义 |
| `main.c` | `bsp/src/main.c` | 裸机测试主入口函数 |

---

## 7. 阶段总结与演进关系

Phase 1 成功构建了可复现的 ARMv8-A 裸机执行基座：

```
Phase 1: 裸机引导与 UART 输出 (已完成)
   │
   ▼
Phase 2: 异常向量表 (VBAR_EL1)、GICv3 中断控制器与通用定时器 (SysTick)
   │
   ▼
Phase 3: FreeRTOS 内核单核移植与任务抢占式调度
   │
   ▼
Phase 4: 多核 SMP 调度框架、核间中断 (SGI/IPI) 与自旋锁同步
```

有了 Phase 1 的稳定引导与日志能力，在 Phase 2 引入异常向量表与 GIC 中断时，任何内核异常与硬件调试信息都能第一时间直接输出至屏幕，为整个操作系统的演进奠定了坚实的基础。
