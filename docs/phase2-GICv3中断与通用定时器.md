# Phase 2 补充: 从 GICv2 切换到 GICv3

## 背景

Phase 2 最初使用 GICv2（QEMU 4.2.0 virt 默认），因为 GICv3 在初步测试中中断无法到达 CPU。经过深入调试，发现 GICv3 本身工作正常，问题出在我们的代码中。

## QEMU GICv3 配置

QEMU virt 平台支持多种 GIC 版本：

```bash
# 默认 GICv2
-machine virt

# 指定 GICv3
-machine virt,gic-version=3

# GICv3 + 无安全状态（单 Non-secure 域）
-machine virt,gic-version=3,secure=off

# GICv3 + EL2 启动（需要额外 IRQ 路由配置）
-machine virt,gic-version=3,virtualization=on
```

使用 `-machine virt,gic-version=3` 时，QEMU 启动时进入 EL1，GICv3 地址为：
- GICD (Distributor): `0x08000000`
- GICR (Redistributor): `0x080A0000`（每核 0x20000 字节，含 RD_base + SGI_base 两个 64KB 帧）

## GICv3 架构

GICv3 有三个主要组件：

```
CPU Core ←→ CPU Interface (ICC_* 系统寄存器)
                ↕
        Redistributor (GICR MMIO)  ← 管理本核 SGI/PPI
                ↕
        Distributor (GICD MMIO)    ← 管理全局 SPI 路由
```

### 关键区别：GICv2 vs GICv3

| 特性 | GICv2 | GICv3 |
|------|-------|-------|
| CPU Interface | 内存映射 (GICC) | 系统寄存器 (ICC_*_EL1) |
| SGI/PPI 管理 | Distributor | Redistributor (每核独立) |
| 最多中断数 | 理论 1024 | 理论 2^20 |
| 亲和性路由 | ITARGETSR (8位/IRQ) | IROUTER (64位/IRQ, affinity) |
| Acknowledge | 读 GICC_IAR | 读 ICC_IAR1_EL1 |
| EOI | 写 GICC_EOIR | 写 ICC_EOIR1_EL1 |

## 调试过程与发现的两个 Bug

### Bug 1: 中断分组不匹配

**现象**: 定时器中断在 GIC Redistributor 中处于 pending 状态 (`ISPENDR0 = 0x40000000`，bit30=1)，但 CPU Interface 返回 spurious (`ICC_IAR1_EL1 = 1023`)。

**根因**: 在之前的 EL2 FIQ 路由实验中，代码将 INTID 26 和 30 从 Group 1 NS 清除到 Group 0：

```c
// gicr_init() 中的旧代码（错误）
GICR_REG(core_id, GICR_IGROUPR0) = 0xFFFFFFFF;
GICR_REG(core_id, GICR_IGROUPR0) &= ~((1 << 26) | (1 << 30)); // ← Bug!
```

但 CPU Interface 只启用了 Group 1 NS (`ICC_IGRPEN1_EL1 = 0x1`)。中断在 Group 0，但 Group 0 没有被启用，所以 CPU Interface 看不到它。

通过诊断寄存器证实：
```
IGROUPR0   = BBFFFFFF   ← bit30=0 (Group 0!)
ISENABLER0 = 40000000   ← bit30=1 (已启用)
ISPENDR0   = 40000000   ← bit30=1 (pending)
ICC_IAR1_EL1 = 1023     ← spurious! Group 1 没有待处理中断
```

**修复**: 移除 Group 0 清除，让 INTID 30 保持在 Group 1 NS：
```c
GICR_REG(core_id, GICR_IGROUPR0) = 0xFFFFFFFF;  // 全部 Group 1 NS
```

### Bug 2: EOI 写入被破坏

**现象**: 修复 Bug 1 后，中断被确认（`ISACTIVER0 = 0x40000000`）但没有后续 tick。

**根因**: 在 vectors.S 的 IRQ handler 中，`x0` 保存了 IAR 值，但 `bl interrupt_dispatch` C 调用破坏了 `x0`：

```asm
// 旧代码（错误）
mrs x0, S3_0_C12_C12_0    /* x0 = ICC_IAR1_EL1 = INTID 30 */
bl interrupt_dispatch       /* C 函数破坏 x0! */
msr S3_0_C12_C12_1, x0     /* x0 已是垃圾值 → EOI 写入错误 */
```

EOI 需要写入与 IAR 相同的 INTID，否则中断永远不会被 deactivate。中断停留在 active 状态，不再产生新的中断。

**修复**: 使用 callee-saved 寄存器 `x19` 保存 IAR 值：
```asm
mrs x0, S3_0_C12_C12_0    /* x0 = ICC_IAR1_EL1 */
mov x19, x0                /* 保存到 callee-saved 寄存器 */
bl interrupt_dispatch       /* C 函数保持 x19 不变 */
mov x0, x19                /* 恢复 IAR 值 */
msr S3_0_C12_C12_1, x0     /* 正确的 EOI */
```

## GICv3 初始化流程

### 1. Distributor 初始化 (gicd_init)

```c
// 禁用 Distributor
GICD_REG(GICD_CTLR) = 0;

// 所有 SPI 设为 Group 1 NS
for (i = 32; i < max_irq; i += 32)
    GICD_REG(GICD_IGROUPR(i)) = 0xFFFFFFFF;

// 设置默认优先级
for (i = 0; i < max_irq; i += 4)
    GICD_REG(GICD_IPRIORITYR(i)) = 0xA0A0A0A0;

// 路由所有 SPI 到 core 0
for (i = 32; i < max_irq; i++)
    GICD_REG64(GICD_IROUTER(i)) = 0;

// 启用: ARE_NS + EnableGrp1NS + EnableGrp1
GICD_REG(GICD_CTLR) = (1<<4) | (1<<1) | (1<<0);
```

### 2. Redistributor 初始化 (gicr_init)

```c
// 唤醒 Redistributor
GICR_REG(core_id, GICR_WAKER) &= ~(1<<1);
// 等待 ChildrenAsleep 清零

// 禁用并清除所有 SGI/PPI
GICR_REG(core_id, GICR_ICENABLER0) = 0xFFFFFFFF;
GICR_REG(core_id, GICR_ICPENDR0) = 0xFFFFFFFF;

// 所有 SGI/PPI 设为 Group 1 NS
GICR_REG(core_id, GICR_IGROUPR0) = 0xFFFFFFFF;

// 设置优先级
for (i = 0; i < 32; i++)
    GICR_PRIO(core_id, i) = 0xA0;
```

**重要**: Redistributor 有两个 64KB 帧：
- **RD_base** (offset 0x00000): CTLR, TYPER, WAKER
- **SGI_base** (offset 0x10000): ISENABLER0, IGROUPR0, IPRIORITYR 等

SGI/PPI 的使能、分组、优先级寄存器都在 SGI_base 帧，不是 RD_base 帧！

### 3. CPU Interface 初始化 (gic_cpuif_init)

```c
// 启用系统寄存器访问
ICC_SRE_EL1 |= 0x7;  // SRE + DFB + DIB

// 优先级掩码：允许所有
ICC_PMR_EL1 = 0xFF;

// 二进制点
ICC_BPR1_EL1 = 0;

// 启用 Group 1 NS 中断
ICC_IGRPEN1_EL1 |= 0x1;
```

## 验证结果

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

✅ GICv3 + EL1 Physical Timer，100Hz tick，每秒打印一次。

## 经验总结

1. **GICv3 分组路由至关重要**: Group 0 → FIQ, Group 1 NS → IRQ。在 Non-secure EL1，必须将中断放入 Group 1 NS 并启用 ICC_IGRPEN1_EL1
2. **EOI 必须使用正确的 INTID**: C 调用会破坏 x0，必须在 bl 指令前保存 IAR 值
3. **Redistributor SGI_base 偏移 +0x10000**: SGI/PPI 寄存器在第二个 64KB 帧中
4. **诊断寄存器是关键**: ISPENDR0 和 ISACTIVER0 能直接看到 GIC 状态，结合 ICC_IAR_EL1 能快速定位问题
5. **QEMU 本身没问题**: Linux 在相同 QEMU 上运行 GICv3 完全正常，问题一定在裸机代码中
