# Phase 2: GICv3 中断控制器、通用定时器与 EL2 启动处理

## 1. 目标

在 Phase 1（裸机 UART 输出）的基础上，构建完整的中断处理链路：

1. **异常向量表** — ARMv8-A 16 入口向量表，2KB 对齐
2. **GICv3 中断控制器** — Distributor + Redistributor + CPU Interface（系统寄存器）
3. **EL1 Physical Timer** — 周期性 100Hz tick 中断
4. **EL2 启动支持** — `virtualization=on` 时自动降级到 EL1

最终目标是为 FreeRTOS SMP 调度器提供可靠的时钟源和中断基础设施。

---

## 2. 架构总览

```
┌─────────────────────────────────────────────────────┐
│                   QEMU virt 平台                     │
│                                                      │
│  CPU (cortex-a53)        GICv3                       │
│  ┌──────────┐     ┌─────────────────────────────┐   │
│  │ EL2 入口  │     │  Distributor (GICD)          │   │
│  │  ↓ eret  │     │  0x08000000                  │   │
│  │ EL1 运行  │     │  - SPI 路由/优先级/使能      │   │
│  │          │◄────│                              │   │
│  │ VBAR_EL1 │     │  Redistributor (GICR)         │   │
│  │ +0x280   │     │  0x080A0000/core              │   │
│  │ = IRQ    │     │  - SGI/PPI 独立管理           │   │
│  │ handler  │     │                               │   │
│  │          │     │  CPU Interface (ICC_*_EL1)    │   │
│  │ CNTP     │     │  系统寄存器，非 MMIO          │   │
│  │ Timer    │─────│  INTID 30 (PPI 14)            │   │
│  │ → IRQ    │     └─────────────────────────────┘   │
│  └──────────┘                                        │
└─────────────────────────────────────────────────────┘
```

**中断处理流程**：

```
Timer 倒计时到 0 → GIC INTID 30 pending → CPU IRQ 异常
    → VBAR_EL1 + 0x280 (el1h_irq)
    → 保存 x0-x30
    → 读 ICC_IAR1_EL1 (应答，获取 INTID)
    → bl interrupt_dispatch (C 函数，按 INTID 查表)
    → gtimer_handler() (重载定时器，打印计数)
    → 写 ICC_EOIR1_EL1 (EOI，与 IAR 相同的 INTID)
    → 恢复 x0-x30
    → eret
```

---

## 3. 从 GICv2 到 GICv3 的演进

### 3.1 为什么要切换

Phase 2 最初使用 GICv2（QEMU 4.2.0 virt 默认），但 GICv3 是现代 ARM 平台的标准：

| 特性 | GICv2 | GICv3 |
|------|-------|-------|
| CPU Interface | MMIO（GICC 0x08010000） | 系统寄存器（ICC_*_EL1） |
| SGI/PPI 管理 | 在 Distributor 中 | Redistributor（每核独立） |
| 中断数量 | 最多 1024 | 最多 2^20 |
| 路由方式 | ITARGETSR（8 位） | IROUTER（64 位 affinity） |
| 应答/完成 | 读/写 MMIO | 读/写系统寄存器 |
| 亲和性路由 | 不支持 | ARE（Affinity Routing Enable） |

GICv3 的系统寄存器接口比 MMIO 快得多（无需内存访问），且是 ARMv8-A 的标准配置。FreeRTOS SMP 需要多核支持，GICv3 的 per-core Redistributor 天然适合。

### 3.2 调试过程中发现的两个关键 Bug

初版 GICv3 代码中断 pending 但无法到达 CPU，经过深入诊断发现了两个 Bug：

#### Bug 1：中断分组不匹配

**现象**：定时器中断在 GICR 中 pending（`ISPENDR0 = 0x40000000`，bit 30），但 CPU Interface 返回 spurious（`ICC_IAR1_EL1 = 1023`）。

**根因**：之前的 EL2 FIQ 路由实验将 INTID 30 从 Group 1 NS 清到了 Group 0：

```c
// 旧代码（错误）
GICR_REG(core_id, GICR_IGROUPR0) = 0xFFFFFFFF;        // 全部 Group 1 NS
GICR_REG(core_id, GICR_IGROUPR0) &= ~((1 << 26) | (1 << 30)); // timer 回 Group 0!
```

但 CPU Interface 只启用了 Group 1 NS（`ICC_IGRPEN1_EL1 = 1`）。Group 0 中断从 NS EL1 不可见。

**修复**：移除 Group 0 覆盖，保持 INTID 30 在 Group 1 NS。

#### Bug 2：EOI 写入被 C 函数调用破坏

**现象**：修复 Bug 1 后，中断被确认（`ISACTIVER0 = 0x40000000`）但没有后续 tick。

**根因**：vectors.S 中 `bl interrupt_dispatch` C 调用破坏了 `x0`（caller-saved），EOI 写入了垃圾值：

```asm
// 旧代码（错误）
mrs x0, S3_0_C12_C12_0     /* x0 = IAR = 30 */
bl interrupt_dispatch        /* C 函数破坏 x0! */
msr S3_0_C12_C12_1, x0      /* x0 = 垃圾 → EOI 失败 */
```

中断永远停留在 active 状态，GIC 不会再产生新的中断。

**修复**：用 callee-saved 寄存器 `x19` 保存 IAR 值。

---

## 4. 代码详解

### 4.1 启动代码 — startup.S

启动代码负责检测当前 EL 并完成必要的初始化：

```asm
_start:
    mrs     x5, MPIDR_EL1
    and     x6, x5, #0xFF
    cbnz    x6, park_secondary         /* 次核挂起 */

    /* 检测当前 EL */
    mrs     x10, CurrentEL
    lsr     x10, x10, #2
    and     x10, x10, #3
    cmp     x10, #2
    b.eq    el2_setup                   /* EL2 → 配置后降级到 EL1 */
    b       el1_setup                   /* EL1 → 直接初始化 */
```

#### EL2 降级到 EL1

这是关键设计。QEMU 使用 `-machine virt,gic-version=3,virtualization=on` 时，CPU 从 EL2 启动。裸机代码不需要在 EL2 运行（那是 hypervisor 的职责），标准做法是配置好 EL1 状态后 `eret` 降级：

```asm
el2_setup:
    /* 1. 启用 GICv3 系统寄存器访问（必须在 EL2 做） */
    mrs     x0, S3_4_C12_C9_5          /* ICC_SRE_EL2 */
    orr     x0, x0, #0x7               /* SRE + DFB + DIB */
    msr     S3_4_C12_C9_5, x0
    isb

    /* 2. HCR_EL2: RW=1（EL1 使用 AArch64），不路由 IRQ/FIQ */
    mov     x0, #(1 << 31)             /* RW bit */
    msr     hcr_el2, x0
    isb

    /* 3. 配置 EL1 状态（eret 前设置 EL1 的寄存器） */
    mrs     x6, CPACR_EL1
    orr     x6, x6, #0x00F00000        /* 启用 FP/SIMD */
    msr     CPACR_EL1, x6

    ldr     x6, =0x30C50838            /* MMU off, I-cache on, ... */
    msr     SCTLR_EL1, x6
    isb

    /* 4. 设置 eret 目标 */
    ldr     x0, =el1_entry
    msr     elr_el2, x0                /* 返回地址 */
    mov     x0, #0x3C5                 /* SPSR: EL1h, DAIF 全部 masked */
    msr     spsr_el2, x0
    eret                                /* → el1_entry at EL1 */
```

**为什么要降级而不是留在 EL2？**

ARMv8-A 中，物理 IRQ 的路由规则：
- `HCR_EL2.IMO = 0`：IRQ 路由到 EL1
- `HCR_EL2.IMO = 1`：IRQ 路由到 EL2，但通过**虚拟 GIC 接口**（ICH_*/ICV_* 寄存器）

留在 EL2 处理中断需要配置整个虚拟 GIC（List Registers、ICH_HCR_EL2、ICH_VMCR_EL2），这是为 hypervisor 设计的复杂机制。裸机代码的标准做法是降级到 EL1，直接使用物理 GIC 接口（ICC_*_EL1 系统寄存器）。

**SPSR_EL2 = 0x3C5 的含义**：

```
Bit [3:0] = 0101  → M[3:0] = EL1h（使用 SP_EL1）
Bit [6]   = 1     → D masked（Debug 异常屏蔽）
Bit [7]   = 1     → A masked（SError 屏蔽）
Bit [8]   = 1     → I masked（IRQ 屏蔽）
Bit [9]   = 1     → F masked（FIQ 屏蔽）
= 0x3C5
```

降级后 IRQ 仍然是 masked 的，在 `common_init` 中通过 `msr daifclr, #0x2` 显式解除屏蔽。

#### EL1 直接初始化

当 QEMU 不带 `virtualization=on` 时（如 `-machine virt,gic-version=3`），CPU 直接从 EL1 启动：

```asm
el1_setup:
    msr     spsel, #1                  /* 使用 SP_EL1 */
    ldr     x20, =stack_top
    add     x20, x20, #4096
    mov     sp, x20

    mrs     x6, CPACR_EL1
    orr     x6, x6, #0x00F00000
    msr     CPACR_EL1, x6

    ldr     x6, =0x30C50838
    msr     SCTLR_EL1, x6
    isb
    b       common_init
```

#### 公共初始化

```asm
common_init:
    /* 清零 BSS 段 */
    ldr     x0, =__bss_start
    ldr     x1, =__bss_end
    mov     x2, #0
clear_bss:
    cmp     x0, x1
    b.ge    bss_done
    str     x2, [x0], #8
    b       clear_bss
bss_done:
    /* 解除 IRQ 屏蔽 */
    msr     daifclr, #0x2              /* 清 DAIF.I 位 */
    bl      main
```

### 4.2 异常向量表 — vectors.S

ARMv8-A 向量表结构：16 个入口，每个 128 字节，共 2KB（2^11 对齐）：

```
VBAR_EL1 基地址
├── +0x000: Current EL with SP_EL0  ─── Sync / IRQ / FIQ / SError
├── +0x200: Current EL with SP_ELx  ─── Sync / IRQ / FIQ / SError  ← 我们用这组
├── +0x400: Lower EL (AArch64)      ─── Sync / IRQ / FIQ / SError
└── +0x600: Lower EL (AArch32)      ─── Sync / IRQ / FIQ / SError
```

我们使用 `spsel #1`（SP_EL1），所以中断进入 **Current EL with SP_ELx** 组。

```asm
.section ".text.vectors", "ax", %progbits
.align 11                              /* 2048 字节对齐 */

.macro ventry label
    .align 7                           /* 128 字节对齐 */
    b      \label
.endm

.global vector_table
vector_table:
    /* Current EL with SP_EL0 — 不使用 */
    ventry  sp0_stub                   /* +0x000: Sync */
    ventry  sp0_stub                   /* +0x080: IRQ */
    ventry  sp0_stub                   /* +0x100: FIQ */
    ventry  sp0_stub                   /* +0x180: SError */

    /* Current EL with SP_ELx (SP_EL1) — 主处理入口 */
    ventry  el1h_sync                  /* +0x200: Sync */
    ventry  el1h_irq                   /* +0x280: IRQ ← GICv3 中断 */
    ventry  el1h_fiq                   /* +0x300: FIQ */
    ventry  el1h_serror                /* +0x380: SError */

    /* Lower EL — 不使用 */
    ventry  lower_stub                 /* +0x400 ~ +0x780: 8 个 stub */
    /* ... */
```

#### IRQ Handler — GICv3 系统寄存器接口

这是整个中断链路的核心。GICv3 使用系统寄存器（非 MMIO）进行中断应答和完成：

```asm
el1h_irq:
    /* 保存所有通用寄存器 x0-x30 */
    stp     x29, x30, [sp, #-16]!
    stp     x27, x28, [sp, #-16]!
    /* ... 保存 x0-x26 ... */
    stp     x0, x1, [sp, #-16]!

    /* 应答中断：读 ICC_IAR1_EL1 (S3_0_C12_C12_0) */
    mrs     x0, S3_0_C12_C12_0         /* x0 = INTID */

    /* 保存 IAR 到 callee-saved 寄存器（C 调用会破坏 x0） */
    mov     x19, x0

    /* 调用 C 分发函数 */
    bl      interrupt_dispatch

    /* EOI：写入 ICC_EOIR1_EL1，必须用原始 IAR 值 */
    mov     x0, x19
    msr     S3_0_C12_C12_1, x0

    /* 恢复所有寄存器 x0-x30 */
    ldp     x0, x1, [sp], #16
    /* ... 恢复 x1-x30 ... */
    ldp     x29, x30, [sp], #16
    eret
```

**ICC_IAR1_EL1 / ICC_EOIR1_EL1 编码**：

系统寄存器编码格式为 `S3_<op1>_C<CRn>_C<CRm>_<op2>`：

| 寄存器 | 编码 | 用途 |
|--------|------|------|
| ICC_IAR1_EL1 | S3_0_C12_C12_0 | 读：应答中断，返回 INTID |
| ICC_EOIR1_EL1 | S3_0_C12_C12_1 | 写：通知中断处理完成 |
| ICC_SRE_EL1 | S3_0_C12_C12_5 | 系统寄存器接口使能 |
| ICC_IGRPEN1_EL1 | S3_0_C12_C12_7 | Group 1 中断使能 |
| ICC_PMR_EL1 | S3_0_C4_C6_0 | 优先级掩码 |
| ICC_BPR1_EL1 | S3_0_C12_C12_3 | 二进制点（优先级分组） |

### 4.3 GICv3 驱动 — gic.c

GICv3 有三层组件，初始化顺序为：Distributor → Redistributor → CPU Interface。

#### Distributor 初始化

管理全局 SPI（Shared Peripheral Interrupt，INTID ≥ 32）：

```c
static void gicd_init(void)
{
    /* 1. 禁用 Distributor */
    GICD_REG(GICD_CTLR) = 0;

    /* 2. 所有 SPI 设为 Group 1 NS（Non-secure Group 1） */
    for (int i = SPI_START; i < max_irq; i += 32)
        GICD_REG(GICD_IGROUPR(i)) = 0xFFFFFFFF;

    /* 3. 设置默认优先级 0xA0（中等） */
    for (int i = 0; i < max_irq; i += 4)
        GICD_REG(GICD_IPRIORITYR(i)) = 0xA0A0A0A0;

    /* 4. 路由所有 SPI 到 core 0 */
    for (int i = SPI_START; i < max_irq; i++)
        GICD_REG64(GICD_IROUTER(i)) = 0;

    /* 5. 启用：ARE_NS + EnableGrp1NS + EnableGrp1 */
    GICD_REG(GICD_CTLR) = GICD_CTLR_ARE_NS | GICD_CTLR_ENABLEGRP1NS | GICD_CTLR_ENABLEGRP1;
}
```

**GICD_CTLR 位域**：
- Bit 4 (ARE_NS) = 1：启用 Affinity Routing
- Bit 1 (EnableGrp1NS) = 1：启用 Non-secure Group 1 中断
- Bit 0 (EnableGrp1) = 1：启用 Group 1 中断

#### Redistributor 初始化

管理本核的 SGI（0-15）和 PPI（16-31）：

```c
static void gicr_init(uint32_t core_id)
{
    /* 1. 唤醒 Redistributor */
    GICR_REG(core_id, GICR_WAKER) &= ~GICR_WAKER_PS;
    /* 等待 ChildrenAsleep 清零 */

    /* 2. 禁用并清除所有 SGI/PPI */
    GICR_REG(core_id, GICR_ICENABLER0) = 0xFFFFFFFF;
    GICR_REG(core_id, GICR_ICPENDR0) = 0xFFFFFFFF;

    /* 3. 全部设为 Group 1 NS */
    GICR_REG(core_id, GICR_IGROUPR0) = 0xFFFFFFFF;

    /* 4. 设置优先级 */
    for (int i = 0; i < 32; i++)
        GICR_PRIO(core_id, i) = 0xA0;
}
```

**重要：Redistributor 有两个 64KB 帧**：

```
GICR_BASE (0x080A0000) + core * 0x20000
├── RD_base (+0x00000): CTLR, TYPER, WAKER
└── SGI_base (+0x10000): ISENABLER0, IGROUPR0, IPRIORITYR, ...
```

SGI/PPI 的使能、分组、优先级寄存器在 **SGI_base**（+0x10000），不是 RD_base！初版代码将所有偏移从 RD_base 计算，导致写入错误地址。

#### CPU Interface 初始化

始终在 EL1 运行（EL2 已在 startup.S 中降级），使用系统寄存器接口：

```c
static void gic_cpuif_init(void)
{
    /* 1. 启用系统寄存器访问 */
    ICC_SRE_EL1 |= 0x7;        /* SRE + DFB + DIB */

    /* 2. 优先级掩码：允许所有优先级 */
    ICC_PMR_EL1 = 0xFF;

    /* 3. 二进制点（优先级分组，0 = 不分组） */
    ICC_BPR1_EL1 = 0;

    /* 4. 启用 Group 1 NS 中断 */
    ICC_IGRPEN1_EL1 |= 0x1;
}
```

**为什么只用 Group 1 NS？**

在 Non-secure EL1 下：
- Group 0 中断 → FIQ，但 Secure 状态才能处理
- Group 1 NS 中断 → IRQ，Non-secure EL1 可以正常处理

GICv3 的安全模型将中断分为多个安全组。在裸机 `-kernel` 启动模式下，EL1 通常运行在 Non-secure 状态，只能使用 Group 1 NS。

#### 公共 API

```c
void gic_enable_irq(uint32_t irq)
{
    if (irq < 32)
        GICR_REG(0, GICR_ISENABLER0) = (1 << irq);   /* PPI/SGI → Redistributor */
    else
        GICD_REG(GICD_ISENABLER(irq)) = (1 << (irq % 32));  /* SPI → Distributor */
}
```

### 4.4 通用定时器驱动 — gtimer.c

使用 ARM Generic Timer 的 EL1 Physical Timer：

```c
static uint64_t timer_step;                   /* 每个 tick 的倒计数值 */
static volatile uint64_t gtimer_ticks;        /* 全局 tick 计数 */

void gtimer_init(void)
{
    uint64_t freq = read_sysreg("cntfrq_el0");       /* 62500000 Hz */
    timer_step = freq / GTIMER_TICK_RATE;             /* 625000 */

    write_sysreg("cntp_ctl_el0", 0);                  /* 禁用定时器 */
    write_sysreg("cntp_tval_el0", timer_step);         /* 设置倒计时 */

    gic_enable_irq(IRQ_TIMER);                         /* INTID 30 */
    gic_set_priority(IRQ_TIMER, 0x80);                 /* 中等优先级 */
}

void gtimer_start(void)
{
    write_sysreg("cntp_tval_el0", timer_step);
    write_sysreg("cntp_ctl_el0", 0x1);                 /* Enable */
}

void gtimer_handler(void)
{
    gtimer_ticks++;
    gtimer_reload();                                    /* 重载 CNTP_TVAL_EL0 */
    if (gtimer_ticks % 100 == 0)
        uart_printf("[Timer] tick %d (%d sec)\r\n", ...);
}
```

**CNTP_CTL_EL0 位域**：
- Bit 0 = Enable（使能定时器）
- Bit 1 = IMASK（中断屏蔽，0 = 不屏蔽）
- Bit 2 = ISTATUS（1 = 定时器已触发）

**注意**：定时器触发后（ISTATUS=1），必须重载 `CNTP_TVAL_EL0` 使 ISTATUS 回 0，否则 GIC 不会产生新的中断。

### 4.5 中断框架 — interrupt.c

简单的 ISR 注册表 + 分发函数：

```c
#define MAX_ISR_ENTRIES  256

static isr_handler_t isr_table[MAX_ISR_ENTRIES];

void interrupt_install(uint32_t irq, isr_handler_t handler)
{
    if (irq < MAX_ISR_ENTRIES)
        isr_table[irq] = handler;
}

void interrupt_dispatch(uint32_t iar)
{
    if (iar >= 1020) return;             /* Spurious (1020-1023) */
    if (iar < MAX_ISR_ENTRIES && isr_table[iar])
        isr_table[iar]();                /* 调用注册的 ISR */
}

void interrupt_init(void)
{
    /* 安装向量表到 VBAR_EL1 */
    asm volatile(
        "ldr x0, =vector_table\n"
        "msr vbar_el1, x0\n"
        "isb\n"
    );
}
```

### 4.6 主程序 — main.c

```c
int main(void)
{
    uint64_t el;
    asm volatile("mrs %0, CurrentEL" : "=r"(el));
    el = (el >> 2) & 3;

    uart_init();
    uart_printf("=== Phase 2: GICv3 + EL%d + Timer ===\r\n", (uint32_t)el);

    interrupt_init();                    /* [1] VBAR_EL1 */
    gic_init(0);                         /* [2] GICv3 三层初始化 */
    gtimer_init();                       /* [3] Timer + IRQ 注册 */
    asm volatile("msr daifclr, #0x2");   /* [4] 解除 IRQ 屏蔽 */
    gtimer_start();                      /* [5] 启动定时器 */

    while (1) asm volatile("wfi");
}
```

**初始化顺序很重要**：
1. 先安装向量表（否则中断无处可去）
2. 再初始化 GIC（否则中断无法路由）
3. 再配置 Timer 并注册 ISR
4. 最后解除 IRQ 屏蔽并启动定时器

### 4.7 链接脚本 — bsp/linker.ld

```
ENTRY(_start)

SECTIONS
{
    . = 0x40000000;                     /* QEMU virt RAM 基地址 */

    .text.entrypoint :
    {
        KEEP(*(.text.entrypoint))       /* startup.S 必须在最前 */
        *(.text*)
        *(.rodata*)
    }

    .data : { *(.data*) }

    .bss :
    {
        __bss_start = .;
        *(.bss*)
        *(COMMON)
        __bss_end = .;
    }

    _end = .;
}
```

### 4.8 平台定义 — include/platform.h

```c
#define UART0_BASE    0x09000000       /* PL011 UART0 */
#define GICD_BASE     0x08000000       /* GICv3 Distributor */
#define GICR_BASE     0x080A0000       /* GICv3 Redistributor (core 0) */

/* Generic Timer 系统寄存器 */
#define CNTP_TVAL_EL0  "cntp_tval_el0"
#define CNTP_CTL_EL0   "cntp_ctl_el0"
#define CNTFRQ_EL0     "cntfrq_el0"
#define CNTPCT_EL0     "cntpct_el0"
```

---

## 5. 编译与运行

### 5.1 编译

```bash
$ make clean && make
Cleaning...
mkdir -p build
mkdir -p build/src
AS  bsp/src/startup.S
AS  bsp/src/freertos_vectors.S
CC  bsp/src/main.c
CC  bsp/src/uart.c
CC  bsp/src/gic.c
CC  bsp/src/gtimer.c
CC  bsp/src/app_irq.c
LD  kernel.elf
OBJCOPY kernel.bin
```

工具链：`aarch64-linux-gnu-gcc`（交叉编译器），目标 `cortex-a53`。

编译选项关键点：
- `-mgeneral-regs-only`：不使用 FP/SIMD 寄存器（简化中断保存/恢复）
- `-ffreestanding -nostdlib -nostartfiles`：裸机环境，无标准库
- `-Iinclude`：头文件搜索路径

### 5.2 运行

支持三种 QEMU 配置：

```bash
# 配置 1: 默认 GICv3（Secure EL1）
$ qemu-system-aarch64 \
    -machine virt,gic-version=3 \
    -cpu cortex-a53 -smp 1 -m 512 \
    -kernel kernel.elf -nographic -monitor none -serial stdio

# 配置 2: GICv3 + 无安全状态（NS EL1）
$ qemu-system-aarch64 \
    -machine virt,gic-version=3,secure=off \
    -cpu cortex-a53 -smp 1 -m 512 \
    -kernel kernel.elf -nographic -monitor none -serial stdio

# 配置 3: GICv3 + EL2 启动（自动降级到 EL1）
$ qemu-system-aarch64 \
    -machine virt,gic-version=3,virtualization=on \
    -cpu cortex-a53 -smp 1 -m 512 \
    -kernel kernel.elf -nographic -monitor none -serial stdio
```

或使用 `make run`（默认使用配置 1）。

### 5.3 运行输出

三种配置输出一致（EL 打印不同）：

```
=== Phase 2: GICv3 + EL1 + Timer ===
[1] Vector table installed
[2] GICv3 initialized
[Timer] freq=62500000 Hz, step=625000
[3] Timer + ISR ready
[4] Timer started

[Timer] tick 100 (1 sec)
[Timer] tick 200 (2 sec)
[Timer] tick 300 (3 sec)
[Timer] tick 400 (4 sec)
```

✅ 100Hz 定时器中断，每秒打印一次。

---

## 6. 文件清单

| 文件 | 说明 | 行数 |
|------|------|------|
| `bsp/src/startup.S` | 启动代码：EL2→EL1 降级、BSS 清零、IRQ 解屏蔽 | ~97 |
| `bsp/src/freertos_vectors.S` | 异常向量表：16 入口、IRQ handler（GICv3 系统寄存器） | ~131 |
| `bsp/src/gic.c` | GICv3 驱动：Distributor + Redistributor + CPU Interface | ~200 |
| `include/gic.h` | GICv3 接口定义、INTID 常量 | ~34 |
| `bsp/src/gtimer.c` | ARM Generic Timer 驱动：EL1 Physical Timer，100Hz | ~65 |
| `include/gtimer.h` | Timer 接口定义 | ~53 |
| `bsp/src/app_irq.c` | ISR 注册表与分发 | ~46 |
| `include/interrupt.h` | 中断框架接口 | ~18 |
| `bsp/src/main.c` | 主程序：初始化序列 | ~46 |
| `include/platform.h` | 平台地址与寄存器定义 | ~113 |
| `bsp/linker.ld` | 链接脚本：RAM 0x40000000 | ~41 |
| `Makefile` | 构建系统 | ~146 |

---

## 7. 经验总结

1. **GICv3 分组是关键**：Group 0 → FIQ，Group 1 NS → IRQ。在 NS EL1 只能使用 Group 1 NS，必须确保 `IGROUPR0` 的对应位为 1。
2. **EOI 必须使用原始 IAR 值**：C 函数调用会破坏 caller-saved 寄存器（x0-x18），IAR 值必须保存到 callee-saved 寄存器（x19-x28）。
3. **Redistributor SGI_base 偏移 +0x10000**：SGI/PPI 的使能/优先级寄存器在第二个 64KB 帧，不在 RD_base。
4. **EL2 裸机标准做法是降级到 EL1**：不要尝试在 EL2 使用虚拟 GIC 接口（ICH_*/ICV_*），那是 hypervisor 的职责。设置 `HCR_EL2.RW=1` 后 `eret` 到 EL1 即可。
5. **ICC_SRE_EL2 必须在 EL2 设置**：这个寄存器控制 EL2/EL1 是否能访问 GICv3 系统寄存器，必须在 EL2 降级前使能。
6. **QEMU 4.2.0 GICv3 没有问题**：Linux 在相同 QEMU 上运行 GICv3 完全正常，问题一定在裸机代码中。

---

## 8. 下一步

Phase 3：集成 FreeRTOS V11.x 内核，以单核模式运行多任务调度。
