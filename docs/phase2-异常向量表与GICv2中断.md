# Phase 2: 异常向量表、GICv2 中断控制器与通用定时器

## 1. 目标与背景

本阶段在 Phase 1（裸机 UART 输出）的基础上，实现完整的异常处理机制：

- **异常向量表**：ARMv8-A 定义了 16 个异常入口，我们在 EL1 安装向量表
- **中断控制器**：管理硬件中断的优先级、路由和分发
- **通用定时器**：产生周期性 tick 中断，为后续 FreeRTOS 调度器提供时钟源

**关键发现**：QEMU 4.2.0 virt 平台默认使用 **GICv2**（非 GICv3）。尝试使用 GICv3 系统寄存器接口（ICC_IAR1_EL1 等）在 Non-secure EL1 下无法收到中断，最终切换到 GICv2 内存映射接口后成功。

---

## 2. 前置知识

### 2.1 ARMv8-A 异常模型

ARMv8-A 在每个异常级别有 16 个向量入口，分为 4 组：

```
VBAR_EL1 基地址
├── +0x000: Current EL with SP_EL0 ──── Synchronous / IRQ / FIQ / SError
├── +0x200: Current EL with SP_ELx ──── Synchronous / IRQ / FIQ / SError  ← 我们用这个
├── +0x400: Lower EL (AArch64) ──────── Synchronous / IRQ / FIQ / SError
└── +0x600: Lower EL (AArch32) ──────── Synchronous / IRQ / FIQ / SError
```

每个入口占 **128 字节**，整个向量表 **2048 字节**（2KB）对齐。我们使用 SP_EL1（`spsel #1`），所以中断进入 **Current EL with SP_ELx** 组，IRQ 入口偏移为 **0x280**。

### 2.2 GICv2 架构

```
┌─────────────┐       ┌──────────────┐       ┌─────────────┐
│   外设中断   │──────▶│  Distributor │──────▶│ CPU Interface│──▶ CPU IRQ
│ UART, Timer │       │  (GICD)      │       │  (GICC)     │
└─────────────┘       │              │       │             │
                      │ - 优先级排序  │       │ - IAR 应答   │
                      │ - 路由目标   │       │ - EOIR 完成   │
                      │ - 使能/禁用   │       │ - PMR 优先级屏蔽│
                      └──────────────┘       └─────────────┘
                       0x08000000              0x08010000
```

GICv2 关键特性：
- **Distributor（GICD）**：管理 SPI（Shared Peripheral Interrupt）的路由和优先级
- **CPU Interface（GICC）**：每核一个，通过 MMIO 应答中断（IAR）和通知完成（EOIR）
- **中断类型**：SGI（0-15，核间）、PPI（16-31，私有）、SPI（32+，共享）

### 2.3 ARM Generic Timer

ARMv8-A 提供了系统计数器和一组定时器：

```
┌──────────────────────────────────┐
│       System Counter             │  ← CNTFRQ_EL0 = 62.5 MHz
│         (共享计数器)              │  ← CNTPCT_EL0 = 当前计数值
└───────────┬──────────────────────┘
            │
    ┌───────▼────────┐
    │ EL1 Physical    │  ← CNTP_TVAL_EL0 (倒计时值)
    │ Timer           │  ← CNTP_CTL_EL0 (使能位)
    │                 │
    │ 倒计时到 0 →    │
    │ 产生 IRQ 中断   │  ← INTID 30 (PPI 14)
    └─────────────────┘
```

---

## 3. 逐步实施

### Step 1: 异常向量表 (vectors.S)

```assembly
.section ".text.vectors", "ax", %progbits
.align 11  /* 2048 字节对齐 */

.macro ventry label
    .align 7   /* 128 字节对齐 */
    b \label
.endm

.global vector_table

vector_table:
    /* Group 0: Current EL with SP_EL0 (不使用, 直接 panic) */
    ventry  el1_sp0_stub         /* +0x000: Sync */
    ventry  el1_sp0_stub         /* +0x080: IRQ */
    ventry  el1_sp0_stub         /* +0x100: FIQ */
    ventry  el1_sp0_stub         /* +0x180: SError */

    /* Group 1: Current EL with SP_ELx (SP_EL1) - 主中断入口 */
    ventry  el1h_sync_handler    /* +0x200: Sync */
    ventry  el1h_irq_handler     /* +0x280: IRQ ← GIC 中断到达这里 */
    ventry  el1h_fiq_stub        /* +0x300: FIQ */
    ventry  el1h_error_stub      /* +0x380: SError */
    /* ... 其余 8 个入口为 stub ... */
```

**关键设计**：EL1h IRQ handler 使用 GICv2 内存映射接口：

```assembly
el1h_irq_handler:
    /* 保存所有通用寄存器 (x0-x30) */
    stp x29, x30, [sp, #-16]!
    /* ... 保存 x0-x28 ... */

    /* 读 GICC_IAR (0x0801000C) 应答中断, 获取 INTID */
    ldr x1, =0x0801000C
    ldr w0, [x1]

    /* 调用 C 分发函数 */
    bl interrupt_dispatch

    /* 写 GICC_EOIR (0x08010010) 通知中断完成 */
    ldr x1, =0x08010010
    str w0, [x1]

    /* 恢复寄存器并返回 */
    /* ... 恢复 x0-x30 ... */
    eret
```

### Step 2: GICv2 驱动 (gic.c)

**Distributor 初始化**：

```c
static void gicd_init(void)
{
    /* 1. 禁用 Distributor */
    GICD_REG(GICD_CTLR) = 0;

    /* 2. 设置 SPI 目标为 core 0 */
    for (int i = 32; i < max_irq; i += 4)
        GICD_REG(GICD_ITARGETSR(i)) = 0x01010101;

    /* 3. 设置默认优先级 0xA0 */
    for (int i = 0; i < max_irq; i += 4)
        GICD_REG(GICD_IPRIORITYR(i)) = 0xA0A0A0A0;

    /* 4. 使能 Distributor */
    GICD_REG(GICD_CTLR) = 1;
}
```

**CPU Interface 初始化**：

```c
static void gicc_init(void)
{
    GICC_REG(GICC_CTLR) = 0;     /* 禁用 */
    GICC_REG(GICC_PMR) = 0xFF;   /* 允许所有优先级 */
    GICC_REG(GICC_BPR) = 0;      /* 无抢占 */
    GICC_REG(GICC_CTLR) = 1;     /* 使能 */
}
```

### Step 3: Generic Timer 驱动 (gtimer.c)

```c
void gtimer_init(void)
{
    /* 读取计数器频率 */
    uint64_t freq = read_sysreg("cntfrq_el0");  /* 62500000 Hz */

    /* 计算每个 tick 的倒计时值 */
    timer_step = freq / GTIMER_TICK_RATE;  /* 625000000 / 100 = 625000 */

    /* 禁用定时器, 设置倒计时值 */
    write_sysreg("cntp_ctl_el0", 0);
    write_sysreg("cntp_tval_el0", timer_step);

    /* 使能 GIC 中断 */
    gic_enable_irq(IRQ_TIMER);  /* INTID 30 */
    gic_set_priority(IRQ_TIMER, 0x80);
}

void gtimer_start(void)
{
    write_sysreg("cntp_tval_el0", timer_step);
    write_sysreg("cntp_ctl_el0", 0x1);  /* bit0 = Enable */
}

/* Timer ISR - 每 100Hz 调用一次 */
void gtimer_handler(void)
{
    gtimer_ticks++;
    gtimer_reload();  /* 重设 CNTP_TVAL_EL0 */

    if (gtimer_ticks % 100 == 0)
        uart_printf("[Timer] tick %d (%d sec)\r\n", ...);
}
```

### Step 4: 中断框架 (interrupt.c)

```c
static isr_handler_t isr_table[256];

void interrupt_install(uint32_t irq, isr_handler_t handler) {
    if (irq < 256) isr_table[irq] = handler;
}

uint32_t interrupt_dispatch(uint32_t iar) {
    if (iar >= 1020) return iar;  /* Spurious */
    if (isr_table[iar]) isr_table[iar]();
    return iar;
}
```

### Step 5: 修改 startup.S

在跳转 main 之前使能 IRQ：

```assembly
    msr daifclr, #0x2   /* 清除 DAIF.I 位, 使能 IRQ */
    bl main
```

### Step 6: 修改 main.c

```c
int main(void)
{
    uart_init();
    interrupt_init();    /* 设置 VBAR_EL1 */
    gic_init();          /* 初始化 GICv2 */
    gtimer_init();       /* 初始化定时器 */
    interrupt_install(IRQ_TIMER, gtimer_handler);  /* 注册 ISR */
    asm volatile("msr daifclr, #0x2");  /* 解除 IRQ 屏蔽 */
    gtimer_start();      /* 启动定时器 */

    while (1) asm volatile("wfi");
}
```

---

## 4. 遇到的问题与解决

### 问题 1: QEMU 默认使用 GICv2，不是 GICv3

**现象**：代码中使用 GICv3 Redistributor 地址（0x080A0000）时，读取操作导致程序挂起。

**排查**：通过 `dtc` 反编译 QEMU 设备树发现：
```
compatible = "arm,cortex-a15-gic";  /* 这是 GICv2! */
reg = <0x00 0x8000000 0x00 0x10000  0x00 0x8010000 0x00 0x10000>;
```

**解决**：QEMU 4.2.0 virt 平台默认使用 GICv2。GICv2 的 CPU Interface 在 0x08010000（不是 Redistributor）。改用 GICv2 内存映射接口后一切正常。

### 问题 2: GICv3 在 Non-secure EL1 下中断不交付

**现象**：尝试强制 GICv3（`-M virt,gic-version=3`），所有寄存器配置正确（ISENABLER、PMR、IGRPEN），定时器已触发（ISTATUS=1），但 CPU 从不进入 IRQ handler。

**根本原因**：QEMU `-kernel` 启动在 Non-secure EL1。GICv3 中 PPI 默认在 Group 0（Secure），从 Non-secure 状态无法使用 Group 0 中断。而 GICR_IGROUPR0 从 NS 状态写入可能被忽略（安全访问限制）。

**解决**：使用 GICv2（QEMU 默认），其安全模型更简单，内存映射接口在 NS EL1 下工作正常。

### 问题 3: GICv3 Redistributor SGI_base 偏移

**发现**：GICv3 Redistributor 有两个 64KB 帧：
- **RD_base**（+0x00000）：CTLR, TYPER, WAKER
- **SGI_base**（+0x10000）：ISENABLER0, IPRIORITYR, IGROUPR0

初版代码将所有 GICR 寄存器偏移都从 RD_base 计算，导致 SGI/PPI 的使能/优先级写入到错误地址。修正后所有 SGI/PPI 寄存器加 0x10000 偏移。

### 问题 4: SCTLR_EL1 开启 MMU 导致崩溃（Phase 1 遗留）

**原因**：`SCTLR_EL1 = 0x30c5083f` 的 bit 0 是 MMU 使能位，在没有页表的情况下启用 MMU 导致所有内存访问异常。

**修复**：改为 `0x30C50838`（bit 0 = 0, MMU 关闭）。

### 问题 5: vsnprintf %d 输出 0 时丢失字符（Phase 1 遗留）

**原因**：数字反转循环修改了写指针 `p`，导致反转后 `p` 指向数字起始位置而非末尾，后续字符覆盖了数字。

**修复**：用 `end = p` 保存反转前的写位置，反转后恢复 `p = end`。

---

## 5. 验证与测试

### 构建命令

```bash
$ make clean && make
```

### 运行命令

```bash
$ make run
# 或手动：
$ qemu-system-aarch64 -M virt -cpu cortex-a53 -smp 1 -m 512 \
    -kernel kernel.elf -nographic -monitor none -serial stdio
```

### 运行输出

```
=== Phase 2: Exception Vectors + GICv2 + Timer ===
Core 0 at EL1
[1] Vector table installed
[2] GICv2 initialized
[Timer] freq=62500000 Hz, step=625000 ticks
[3] Timer initialized
[4] Timer ISR registered
[5] IRQ unmasked, timer started
=== Initialization complete ===

[Timer] tick 100 (1 sec)
[Timer] tick 200 (2 sec)
[Timer] tick 300 (3 sec)
[Timer] tick 400 (4 sec)
[Timer] tick 500 (5 sec)
[Timer] tick 600 (6 sec)
[Timer] tick 700 (7 sec)
[Timer] tick 800 (8 sec)
[Timer] tick 900 (9 sec)
```

✅ **验证通过**：定时器每秒产生 100 次 tick 中断（100Hz），每秒打印一次计数。

---

## 6. 文件清单

| 文件 | 说明 |
|------|------|
| `bsp/src/freertos_vectors.S (Phase 2 original: vectors.S)` | 异常向量表（16 入口，2KB 对齐） |
| `bsp/src/gic.c` | GICv2 驱动（Distributor + CPU Interface） |
| `include/gic.h` | GICv2 接口定义 |
| `bsp/src/gtimer.c` | ARM Generic Timer 驱动 |
| `include/gtimer.h` | Timer 接口定义 |
| `bsp/src/app_irq.c (Phase 2 original: interrupt.c)` | ISR 注册表与中断分发 |
| `include/interrupt.h` | 中断框架接口 |
| `bsp/src/startup.S` | 修改：启用 IRQ (daifclr) |
| `bsp/src/main.c` | 修改：初始化序列 |
| `Makefile` | 修改：添加新源文件 |

---

## 7. 阶段总结

本阶段实现了完整的 ARMv8-A 中断处理链路：

1. **异常向量表** → IRQ 入口在 VBAR_EL1 + 0x280
2. **IRQ Handler** → 保存上下文 → 读 GICC_IAR → 分发 → 写 GICC_EOIR → 恢复上下文
3. **GICv2** → Distributor 路由 + CPU Interface 应答
4. **Generic Timer** → 倒计时到 0 产生中断 → ISR 重载定时器

**关键经验**：在 QEMU 4.2.0 virt 平台上，使用 GICv2（默认）比 GICv3 更可靠。GICv3 的安全模型在裸机 `-kernel` 启动模式下限制了 Non-secure EL1 对 PPI 中断组的访问。

**下一步**：Phase 3 将集成 FreeRTOS V11.x 内核，以单核模式运行多任务调度。
