# FreeRTOS SMP for ARMv8-A (AArch64) on QEMU virt

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![Build System](https://img.shields.io/badge/Build-SCons-blue.svg)](https://scons.org/)
[![Architecture](https://img.shields.io/badge/Arch-AArch64%20%2F%20Cortex--A53-orange.svg)]()
[![Platform](https://img.shields.io/badge/Platform-QEMU%20virt-green.svg)]()
[![RTOS](https://img.shields.io/badge/RTOS-FreeRTOS%20V11%20SMP-red.svg)](https://freertos.org/)

本项目是一个基于 **FreeRTOS SMP (Symmetric Multiprocessing)** 的标准、高可靠、完全可复现的 64 位对称多核实时操作系统（RTOS）基准环境。项目以 **ARMv8-A (AArch64, Cortex-A53)** 架构与 **QEMU virt (GICv3)** 为目标硬件平台，提供工业级的体系结构抽象层（`arch`）、标准化的 FreeRTOS 多核调度扩展（`kernel`）、完整的板级支持包（`bsp`）以及现代化的 **SCons** 构建体系。

项目经过深度清理与架构重构，可以作为一套纯净、自包含、生产可用的 FreeRTOS SMP 操作系统基准。

---

## 目录

- [1. 能力边界（Capabilities & Boundaries）](#1-能力边界capabilities--boundaries)
- [2. 源码许可与作者声明审计](#2-源码许可与作者声明审计)
- [3. 核心架构设计](#3-核心架构设计)
- [4. 目录结构](#4-目录结构)
- [5. 环境准备](#5-环境准备)
- [6. SCons 构建系统指南](#6-scons-构建系统指南)
- [7. QEMU 多核运行与验收复现](#7-qemu-多核运行与验收复现)
- [8. 技术演进与连载博客文档](#8-技术演进与连载博客文档)
- [9. 许可证](#9-许可证)

---

## 1. 能力边界（Capabilities & Boundaries）

为了在工程实践中清晰定位系统适用场景，本项目对功能特性与能力边界进行了明确界定：

### 1.1 核心支持能力（Supported Capabilities）

- **SMP 对称多核调度与伸缩**：
  - 支持 **2 ~ 32 个 CPU 核心** 的动态参数化伸缩（通过 `CORES=N` 构建参数设定）。
  - 支持基于任务亲和性（Core Affinity）的绑定调度（`vTaskCoreAffinitySet`）。
  - 支持多优先级抢占调度与同优先级时间片轮转（Round-Robin）。
- **核间通信与协调机制（IPI）**：
  - 深度集成 ARM GICv3 SGI（Software Generated Interrupt），使用 SGI 0 触发快速跨核 Yield，实现毫秒级跨核任务协同与即时唤醒。
- **跨核线程安全与同步原语**：
  - 双层自旋锁模型：内核调度锁（`xTaskSpinlock`）与中断服务锁（`xISRSpinlock`）。
  - 原生支持跨核互斥量（Mutex）、计数信号量（Semaphore）、消息队列（Queue）以及原子操作（Atomic LL/SC）。
- **裸机底层引导与规范对接**：
  - 支持标准 ARM PSCI（Power State Coordination Interface）协议：主核通过 `CPU_ON`（`0xC4000003`）冷启动辅助核。
  - 支持安全状态感知与自动降级：兼容 QEMU virt 的 EL2/EL1 启动环境，自适应配置 `ICC_SRE_EL2` 并安全落入 EL1。
  - GICv3 系统寄存器驱动（ICC 接口）与 ARM Generic Timer（物理计时器，1000Hz SysTick）。
- **自动化工程与验收体系**：
  - 基于 SCons 的跨模块构建系统，提供构建矩阵参数化配置。
  - 内置无头 QEMU 自动化验收测试套件（支持 `active`, `affinity`, `prio`, `rr`, `sem`, `queue`, `mutex`, `spin` 全量断言自检）。

### 1.2 明确的能力边界与非设计目标（Boundaries & Limitations）

- **特权级边界（EL Exception Level）**：
  - 本操作系统运行于 **Non-Secure EL1**（操作系统级）。系统假定底层固件（如 TF-A / UEFI 或 QEMU PSCI）已完成 EL3/EL2 基础安全和虚拟化配置，本系统不承担 Secure Monitor（EL3）或 Hypervisor（EL2）自身逻辑。
- **内存管理边界（Flat Physical Memory）**：
  - 本项目定位为轻量级实时嵌入式系统 / Unikernel 原型，运行于扁平单段物理内存空间（Flat Physical Memory），**未启用 MMU 页表与用户态进程虚拟地址隔离**。所有任务运行在同一内核地址空间中，追求最低的上下文切换延迟与资源开销。
- **动态加载边界（Static Linking）**：
  - 系统采用完全静态编译链接（单一 ELF / Raw Binary 固件镜像），不支持运行时动态链接库（`.so`）或 ELF 重定位动态加载。
- **外设与驱动边界**：
  - 当前板级支持包专注于 QEMU virt 基础计算平台（PL011 UART、GICv3、ARM Generic Timer、PSCI）。暂未包含网络栈（VirtIO-Net / LwIP）、块设备存储栈（VirtIO-Block / FATFS）或 PCIe 总线枚举驱动。
- **浮点与 SIMD 上下文边界**：
  - 默认工具链强制开启 `-mgeneral-regs-only`（仅通用寄存器模式），以确保极致的异常响应时间与上下文切换吞吐。任务默认不自动备份 32 个 128 位 NEON 寄存器（可通过配置开启）。

---

## 2. 源码许可与作者声明审计

本项目恪守开源软件知识产权规范，所有集成源码与衍生代码均保证清晰、合法、合规：

| 模块 / 源码路径 | 原始作者 / 权利人 | 许可证 | 声明与保留状态 |
|:---|:---|:---|:---|
| **FreeRTOS 内核** (`kernel/`)<br>`tasks.c`, `queue.c`, `list.c`, `timers.c`, `event_groups.c`, `stream_buffer.c`, `include/*`, `MemMang/*` | **Amazon.com, Inc. / Arm Limited** | **MIT License** (SPDX: MIT) | **100% 完整保留**。<br>每个内核文件头部完整保留了原始 Copyright 声明与 MIT 许可授权正文，未做任何删减。 |
| **AArch64 SMP 移植层** (`kernel/port/`)<br>`port.c`, `port_smp.c`, `portmacro.h`, `portASM.S` | **FreeRTOS upstream / 本项目作者** | **MIT License** | **明确保留衍生出处**。<br>保留原版 `FreeRTOS-Kernel/portable/GCC/ARM_AARCH64_SRE` 版权注释，并明确注明 SMP 扩展改动。 |
| **体系结构抽象层** (`arch/`)<br>`arch/include/arch_api/*`, `arch/aarch64/*`, `rules.py` | **本项目作者 (ForgeStitch)** | **MIT License** | **自主设计**。<br>提供通用原子操作、多核屏障、自旋锁与异常抽象，采用相同宽松 MIT 许可发布。 |
| **板级支持包** (`bsp/`)<br>`startup.S`, `gic.c`, `uart.c`, `ukernel.c`, `app.c`, `linker.ld` | **本项目作者 (ForgeStitch)** | **MIT License** | **自主开发与移植**。<br>针对 QEMU virt / Cortex-A53 平台的裸机引导与驱动，遵循 MIT 许可。 |

> **提示**：仓库根目录下附有正式的 [LICENSE](LICENSE) 文件，合并声明了原始上游（Amazon / Arm）与项目作者版权，商业友好，可自由闭源商用、二次修改或学术研究。

---

## 3. 核心架构设计

项目采用清晰的分层解耦与组件化架构：

```
┌────────────────────────────────────────────────────────────────────────┐
│                   CherryOS 现代化组件生态 (components/)                │
│    CherryShell (VxWorks C解释器/符号反查) · CherryVFS (Inode/Dev/POSIX) │
│    CherryTimer (Per-CPU/hrtimer)          · CherryPrintk (无锁环形/dmesg)│
│                                   │                                    │
│                  CherryKAL (Linux 标准 API 内核抽象层)                 │
└───────────────────────────────────┬────────────────────────────────────┘
                                    │
┌───────────────────────────────────▼────────────────────────────────────┐
│              BSP 应用与测试层 (bsp/src/app.c, ukernel.c)                │
│     调度器验证探针 (优先级 / 时间片 / 信号量 / 队列 / 互斥锁)          │
└───────────────────────────────────┬────────────────────────────────────┘
                                    │
┌───────────────────────────────────▼────────────────────────────────────┐
│               FreeRTOS SMP 内核与移植层 (kernel/, include/)            │
│             FreeRTOS V11.x Core + GCC/ARM_AARCH64_SRE 端口             │
└───────────────────────────────────┬────────────────────────────────────┘
                                    │
┌───────────────────────────────────▼────────────────────────────────────┐
│                  通用体系结构抽象层 ARCH (arch/include, arch/)         │
│     MPIDR亲和性 / 内存屏障 / 内联原子操作 / 嵌套自旋锁 / 通用 IRQ 分发表│
└───────────────────────────────────┬────────────────────────────────────┘
                                    │
┌───────────────────────────────────▼────────────────────────────────────┐
│              底层硬件与固件 (QEMU virt AArch64 / Cortex-A53)           │
│             PL011 UART · GICv3 (ICC_SRE) · Generic Timer · PSCI        │
└────────────────────────────────────────────────────────────────────────┘
```

1. **体系结构抽象层 (`arch/`)**：
   - 通用多核抽象：封装 `MPIDR_EL1` 簇与核心编号计算（`arch_curr_cpu_id`）。
   - 无锁原子指令：通过 `-mno-outline-atomics` 与 GCC 内建原子原语，为 Cortex-A53 生成高效、内联的 LL/SC（`ldxr`/`stxr`）无锁操作。
   - 同步原语：实现支持重入的递归自旋锁（`arch_recursive_spinlock_t`）与裸机自旋锁（`arch_raw_spinlock_t`）。
   - 中断与异常基础设施：提供与 RTOS 无关的统一中断向量注册与分发框架（`arch_irq_register` / `arch_irq_dispatch`）。

2. **FreeRTOS SMP 内核 (`kernel/`)**：
   - 官方 FreeRTOS V11 SMP 调度器基线，支持跨核负载均衡与动态核心亲和性绑定（`xTaskCreateAffinitySet`）。
   - 核间中断（SGI 0）唤醒与让出（IPI Yield Assist），实现毫秒级跨核任务抢占。
   - GICv3 系统寄存器接口（ICC_SRE_EL1）驱动，支持优先级屏蔽（PMR）临界区。

3. **板级支持包 (`bsp/`)**：
   - 裸机引导：从 `0x40000000` 物理地址冷启动，完成 EL2→EL1 降级配置、SCTLR 寄存器保护与 BSS 快速清零。
   - 标准 PSCI 多核启动：主核（Core 0）通过 ARM 标准 PSCI `CPU_ON`（`0xC4000003`）接口唤醒副核，彻底兼容 QEMU virt 虚拟化规范。
   - 硬件驱动：PL011 UART 驱动（含轻量级 `vsnprintf` 与 Linux 风格格式化日志）、ARM Generic Timer 物理定时器（1000Hz SysTick）。

4. **CherryOS 现代化组件生态 (`components/`)**：
   - [**`CherryKAL`**](components/cherry_kal/README.md) (Kernel Abstraction Layer / OSAL)：
     - 采用严格 Linux 内核接口语义（`kal_mutex_*`, `kal_sem_*`, `kal_spin_*`, `kal_thread_*`, `kal_malloc`/`free`, `kal_atomic_*`）。
     - 将上层组件与具体底层 RTOS 100% 隔离解耦，使每个 Cherry 组件均可独立剥离给任意第三方嵌入式开发者使用。
   - [**`CherryVFS`**](components/cherry_vfs/README.md) (Virtual File System & Device Driver Framework)：
     - 与主流 Linux 高度兼容的 Inode / Dentry 虚拟文件系统抽象，提供标准 POSIX 文件操作接口（`open`, `close`, `read`, `write`, `opendir`, `readdir`, `mkdir`, `mount`）。
     - 统一抽象字符设备（Char Dev，如 PL011 UART、`/dev/null`、`/dev/zero`）、块设备（Block Dev，如 `/dev/ramdisk0`）、网络套接字设备（Socket Dev，BSD Socket 映射）。
     - 提供标准的平台总线（Platform Bus）与驱动枚举绑定体系（`vfs_platform_device_register`, `vfs_platform_driver_register`, `probe`/`remove`）。
   - [**`CherryPrintk`**](components/cherry_printk/README.md) (High-Performance Multi-Core Linux-Style Logging Engine)：
     - **100% Linux 内核 Dmesg 规范**：格式对齐 `[seconds.usec] [level:cpux:task:tid] msg`，支持 `pr_info`, `pr_err`, `pr_warn`, `pr_debug` 等标准日志级别，全小写规整输出。
     - **多核并发原子行输出**：结构化环形记录缓冲区（Record Ring Buffer），避免跨核字符交错（Non-Interleaving）。
     - **极短关中断时间（< 100ns）**：入队仅占用微秒级以下自旋锁，慢速 UART 打印完全在开中断状态下异步 Drain，绝不影响硬件定时器中断。
   - [**`CherryTimer`**](components/cherry_timer/README.md) (Multi-Core High-Precision Linux Timer Subsystem)：
     - **Linux 双标准接口**：完整支持低精度定时器（`struct timer_list`，`timer_setup`, `mod_timer`, `del_timer`）与高分辨率纳秒定时器（`struct hrtimer`，`hrtimer_init`, `hrtimer_start`, `hrtimer_cancel`）。
     - **硬件级精准纳秒时基**：直接对接 BSP 的 ARM Generic Timer 物理计数器 `cntpct_el0`，单周期精度 ~16ns，零漂移。
     - **Per-CPU Timer Base**：独立每个核心的定时队列与自旋锁，实现多核并发无竞争添加与分发。
   - [**`CherryShell`**](components/cherry_shell/README.md) (VxWorks-style Dynamic C Interpreter & Symbol Engine)：
     - **C 函数即命令（Functions as Commands）**：终端直接以 C 语法调用任意函数并传递参数，实时打印返回值（`value = <dec> = 0x<hex>`）。基于 AAPCS64 实现最高 8 参数（`x0`~`x7`）无缝寄存器调用。
     - **符号与地址双向解析（`addr2sym`）**：运行时支持从内存或异常 PC 指针反查最近符号名称及偏移（如 `vfs_open + 0x18`）。
     - **编译期段导出与动态注册**：支持 `CHERRY_SHELL_EXPORT_SYM` / `CMD` / `VAR` 零侵入宏导出，亦支持运行时动态注册。
     - 内置丰富的系统级指令：`help`, `syms`, `addr2sym`, `ls`, `cat`, `mkdir`, `touch`, `ps`, `echo`。

---

## 4. 目录结构

```text
freertos-workspace/
├── LICENSE                     # MIT 开源许可证（包含 Amazon/Arm 上游与作者版权声明）
├── SConstruct                  # SCons 顶层构建脚本（选项配置、构建流编排、QEMU与测试目标）
├── Makefile                    # 命令行友好型 Makefile 前端包装器
├── .gitignore                  # Git 忽略规则（已自动排除 docx、中间生成物与调试文件）
├── components/                 # CherryOS 现代化操作系统组件库 (全解耦设计)
│   ├── cherry_kal/             # 内核抽象层 (Linux 接口标准 OSAL: mutex, sem, spinlock, thread, malloc)
│   │   ├── include/cherry_kal.h
│   │   ├── src/kal_freertos.c  # 基于 FreeRTOS V11 SMP + AArch64 的适配层
│   │   └── SConscript          # libcherry_kal.a 构建规则
│   ├── cherry_printk/          # 高性能多核并发 Linux 日志引擎 (短关中断、环形队列、dmesg 格式)
│   │   ├── include/printk.h    # 标准 printk 与 pr_* 宏
│   │   ├── src/                # 环形缓冲区、栈局部格式化、整行输出
│   │   └── SConscript          # libcherry_printk.a 构建规则
│   ├── cherry_timer/           # 多核并发高精度 Linux 风格定时器 (timer_list + hrtimer, BSP 硬件驱动)
│   │   ├── include/            # timer.h, hrtimer.h, cherry_timer.h
│   │   ├── src/                # Per-CPU Timer Base, 纳秒 hrtimer, 队列分发器
│   │   └── SConscript          # libcherry_timer.a 构建规则
│   ├── cherry_vfs/             # 虚拟文件系统与统一设备驱动框架
│   │   ├── include/            # VFS、Inode、POSIX、Device、Driver、Platform 头文件
│   │   ├── src/                # Inode 管理、DevFS、RamFS、PL011/Null/Zero/Ramdisk/Socket 驱动
│   │   ├── tests/              # VFS 自动化验收测试套件 (vfs_test.c)
│   │   └── SConscript          # libcherry_vfs.a 构建规则
│   ├── cherry_shell/           # VxWorks 风格动态 C 表达式命令行与符号引擎
│   │   ├── include/            # CherryShell、符号表、解析器 API
│   │   ├── src/                # 符号解析 (addr2sym), AAPCS64 动态调用, 内置指令集, 交互式终端
│   │   ├── tests/              # 符号、C 函数调用与指令集测试套件 (shell_test.c)
│   │   └── SConscript          # libcherry_shell.a 构建规则
│   ├── cherry_pci/             # Linux 风格 PCIe 子系统与 ARM GICv3 ITS MSI 支持
│   │   ├── include/            # PCIe ECAM、GICv3 ITS (cherry_its.h)、标准寄存器 (pci_regs.h)
│   │   ├── src/                # 总线扫描、BAR 分配器、ITS 命令队列驱动、MSI 绑定、pci_ls 指令
│   │   ├── tests/              # PCIe 自动化验收测试套件 (pci_test.c)
│   │   └── SConscript          # libcherry_pci.a 构建规则
│   └── cherry_sdio/            # SD / SDIO / MMC / eMMC 协议栈与 CherryVFS 块设备
│       ├── include/            # SDHCI 寄存器、SD 协议规范头文件
│       ├── src/                # SDHCI 主机驱动、卡识别状态机、PCI 驱动适配、/dev/mmcblk0 桥接
│       ├── tests/              # SD/eMMC 读写自动化验收测试套件 (sdio_test.c)
│       └── SConscript          # libcherry_sdio.a 构建规则
├── arch/                       # 体系结构抽象层
│   ├── rules.py                # 通用 AArch64 编译规则与工具链初始化
│   ├── SConscript              # arch 静态对象构建脚本
│   ├── aarch64/arch.c          # AArch64 中断表、系统调用与异常记录实现
│   └── include/arch_api/       # 通用架构头文件 (atomic, spinlock, barrier, ipi, irq, thread...)
├── kernel/                     # FreeRTOS SMP 内核与端口
│   ├── SConscript              # libfreertos.a 构建脚本
│   ├── tasks.c, queue.c...     # FreeRTOS 核心源文件 (保留上游作者与 MIT 声明)
│   ├── MemMang/heap_4.c        # 动态内存管理
│   └── port/                   # AArch64 SMP 汇编与 C 端口实现 (port.c, port_smp.c, portASM.S)
├── bsp/                        # 板级支持包与目标可执行镜像
│   ├── SConscript              # BSP 对象编译与目标链接脚本 (生成 freertos_smp.elf / .bin)
│   ├── linker.ld               # AArch64 链接脚本 (.cherry_shell_sym 符号段, RAM 基址 0x40000000)
│   ├── include/                # 板级驱动头文件
│   └── src/                    # 启动代码 (startup.S), 中断向量 (freertos_vectors.S), 
│                               #   GICv3 (gic.c), Timer (gtimer.c), UART (uart.c), 测试探针 (app.c)
├── include/                    # 全局与 FreeRTOS 配置头文件 (FreeRTOSConfig.h, portmacro.h...)
├── tests/                      # 自动化测试套件
│   └── acceptance/             # QEMU 调度器与组件验收测试脚本 (accept_qemu_sched.py)
├── docs/                       # 完整技术博客与架构设计连载文档（Markdown 格式，排除 docx）
│   ├── phase1-AArch64裸机引导与UART输出.md
│   ├── phase2-异常向量表与GICv2中断.md
│   ├── phase2-GICv3中断与通用定时器.md
│   ├── phase3-FreeRTOS集成.md
│   ├── phase4-FreeRTOS-SMP.md
│   ├── unikernel-architecture.md
│   └── diagrams/               # 架构与调度时序图表
└── status.md                   # SMP 内核与 ARCH 抽象层演进状态报告
```

---

## 5. 环境准备

本项目推荐在 Linux 或 WSL（Ubuntu 20.04 / 22.04 LTS）环境下开发与运行。

### 5.1 安装交叉工具链与仿真器

```bash
# 安装 AArch64 交叉编译器与 QEMU
sudo apt-get update
sudo apt-get install -y gcc-aarch64-linux-gnu qemu-system-arm python3 python3-pip make
```

### 5.2 安装 SCons

```bash
# 通过 pip 安装最新版 SCons
pip3 install --user scons

# 确保 ~/.local/bin 在环境变量 PATH 中
echo 'export PATH="$HOME/.local/bin:$PATH"' >> ~/.bashrc
source ~/.bashrc
```

验证安装：
```bash
scons --version
# SCons: v4.x 或更高
aarch64-linux-gnu-gcc --version
qemu-system-aarch64 --version
```

---

## 6. SCons 构建系统指南

项目采用现代化的 SCons 构建系统，彻底实现编译选项参数化、依赖精确追踪与跨模块静态库构建。

### 6.1 可配置参数列表

| 参数名 | 默认值 | 可选值 | 说明 |
|--------|--------|--------|------|
| `CORES` | `2` | `1`, `2`, `4`, `8`, `16`, `32` | SMP CPU 核心数 |
| `SCHED_MODE` | `full` | `full`, `cross`, `cross-sem`, `cross-queue`, `cross-mutex`, `cross-spin` | 调度器验证模式 |
| `LOG_LEVEL` | `6` | `0` ~ `7` (7 开启诊断) | 内核 UART 日志输出级别 |
| `TICK_BROADCAST` | `1` | `0`, `1` | 是否向所有核心广播 SysTick |
| `CROSS_COMPILE` | `aarch64-linux-gnu-` | 自定义前缀 | 交叉编译器前缀 |
| `BUILD_DIR` | `build` | 任意路径 | 构建产物输出目录 |
| `TIMEOUT` | `30` | 整数（秒） | 自动化验收测试超时时间 |

### 6.2 常用构建命令

```bash
# 1. 默认构建 (2核 SMP, full 模式)
scons

# 2. 指定核心数编译 (如 4 核或 32 核)
scons CORES=4
scons CORES=32

# 3. 指定核间同步压测模式 (cross 模式)
scons CORES=4 SCHED_MODE=cross

# 4. 多线程加速编译
scons -j$(nproc)

# 5. 清理所有编译产物
scons -c
```

构建完成后，目标文件位于 `build/bsp/`：
- `build/bsp/freertos_smp.elf`：可执行 ELF 镜像（带符号表调试信息）
- `build/bsp/freertos_smp.bin`：扁平化裸机固件二进制

---

## 7. QEMU 多核运行与验收复现

项目内置了 QEMU 自动化运行与调度器验收目标，一键即可复现完整的 SMP 运行状态。

### 7.1 交互式运行 QEMU 模拟仿真

直接启动 QEMU 并在终端查看系统日志（按 `Ctrl+A`, `X` 退出 QEMU）：

```bash
# 使用 scons 目标运行
scons run

# 或者指定 4 核心运行
scons CORES=4 run
```

亦可直接使用底层的 `qemu-system-aarch64` 命令行：

```bash
qemu-system-aarch64 \
    -machine virt,gic-version=3 \
    -cpu cortex-a53 \
    -smp 2 \
    -m 512 \
    -kernel build/bsp/freertos_smp.elf \
    -nographic \
    -monitor none \
    -serial stdio
```

### 7.2 运行自动化调度器验收测试 (pass 验证)

验收测试套件会抓取 QEMU 控制台输出，解析所有的 `[sched-verify]` 状态指标：

```bash
# 运行 2 核完整调度验收测试 (full 模式)
scons test
# 或使用 Makefile
make sched-test
```

**测试输出示例**：
```text
==> Running Scheduler Acceptance Test (cores=2, mode=full)...
[    0.027849] [6:cpu0:sched-mon:60111] [printk-verify] basic=pass multicore=pass irq_safe=pass all=pass
[    0.061194] [6:cpu0:sched-mon:60111] [timer-verify] standard=pass hrtimer=pass multi_core=pass restart=pass cancel=pass all=pass
[    0.065649] [6:cpu0:sched-mon:60111] cherry_vfs: ==> starting cherryvfs acceptance test suite...
cherry_vfs: [/dev/console] posix write through cdev successful
[    0.082868] [6:cpu0:sched-mon:60111] cherry_vfs: [vfs-verify] pdev=pass cdev=pass bdev=pass ramfs=pass sock=pass all=pass
[    0.083756] [6:cpu0:sched-mon:60111] [vfs-stress] starting large-scale file operation stress tests (batch_files=50)...
[    0.178185] [6:cpu0:sched-mon:60111] [vfs-stress] hierarchy=pass batch_files=pass large_seek_trunc=pass rename=pass concurrent_smp=pass all=pass
[    0.180757] [6:cpu0:sched-mon:60111] cherry_pci: gicv3 its engine initialized (cbaser=0xa80000004004040f, translater=0x8090040, pta=0)
[    0.181324] [6:cpu0:sched-mon:60111] cherry_pci: pcie ecam host bridge detected at 0x4010000000
[    0.184568] [6:cpu0:sched-mon:60111] cherry_pci: pcie bus scan completed, 3 devices enumerated
[    0.185747] [6:cpu0:sched-mon:60111] cherry_pci: [pci-verify] ecam=pass bus_scan=pass dev_found=pass bar_alloc=pass its_msi=pass all=pass
[    0.188675] [6:cpu0:sched-mon:60111] cherry_sdio: detected sd specification v2.0+ card
[    0.189699] [6:cpu0:sched-mon:60111] cherry_sdio: csd parsed: capacity=64 mb, sectors=131072, blksize=512
[    0.190117] [6:cpu0:sched-mon:60111] cherry_sdio: registered block device /dev/mmcblk0 (131072 sectors, 512b/sector)
[    0.194767] [6:cpu0:sched-mon:60111] cherry_sdio: [sdio-verify] sdhci_init=pass card_detect=pass ocr=pass cid=pass csd=pass block_read=pass block_write=pass vfs_bdev=pass all=pass
[    0.195332] [6:cpu0:sched-mon:60111] cherry_shell: initialized with vxworks c-function call & addr2sym engine
[    0.215410] [6:cpu0:sched-mon:60111] cherry_shell: [shell-verify] syms=pass addr2sym=pass c_func=pass sum8=pass str=pass cmds=pass all=pass
[    0.216483] [6:cpu0:sched-mon:60111] freertos: [sched-verify] active=pass idle=pass affinity=pass coverage=pass progress=pass prio=pass rr=pass sem=pass queue=pass mutex=pass spin=pass cores=2 seen=0x3 missing=0x0 hb0=1 hb1=173
[SCHED-TEST] SUMMARY result=pass mode=full cores=2 markers=1
scons: done building targets.
```

### 7.3 跨核同步压力测试 (Cross-core Stress)

```bash
# 运行跨核信号量、消息队列、互斥锁与自旋锁同步测试
scons SCHED_MODE=cross test
# 或使用 Makefile
make sched-cross
```

**测试输出示例**：
```text
==> Running Scheduler Acceptance Test (cores=2, mode=cross)...
[    0.030843] [6:cpu0:sched-mon:58869] [printk-verify] basic=pass multicore=pass irq_safe=pass all=pass
[    0.063757] [6:cpu0:sched-mon:58869] [timer-verify] standard=pass hrtimer=pass multi_core=pass restart=pass cancel=pass all=pass
[    0.074100] [6:cpu0:sched-mon:58869] cherry_vfs: ==> starting cherryvfs acceptance test suite...
cherry_vfs: [/dev/console] posix write through cdev successful
[    0.097956] [6:cpu0:sched-mon:58869] cherry_vfs: [vfs-verify] pdev=pass cdev=pass bdev=pass ramfs=pass sock=pass all=pass
[    0.099242] [6:cpu0:sched-mon:58869] [vfs-stress] starting large-scale file operation stress tests (batch_files=50)...
[    0.188765] [6:cpu0:sched-mon:58869] [vfs-stress] hierarchy=pass batch_files=pass large_seek_trunc=pass rename=pass concurrent_smp=pass all=pass
[    0.191431] [6:cpu0:sched-mon:58869] cherry_pci: gicv3 its engine initialized (cbaser=0xa80000004004040f, translater=0x8090040, pta=0)
[    0.192344] [6:cpu0:sched-mon:58869] cherry_pci: pcie ecam host bridge detected at 0x4010000000
[    0.196787] [6:cpu0:sched-mon:58869] cherry_pci: [pci-verify] ecam=pass bus_scan=pass dev_found=pass bar_alloc=pass its_msi=pass all=pass
[    0.199429] [6:cpu0:sched-mon:58869] cherry_sdio: detected sd specification v2.0+ card
[    0.200213] [6:cpu0:sched-mon:58869] cherry_sdio: csd parsed: capacity=64 mb, sectors=131072, blksize=512
[    0.200897] [6:cpu0:sched-mon:58869] cherry_sdio: registered block device /dev/mmcblk0 (131072 sectors, 512b/sector)
[    0.205436] [6:cpu0:sched-mon:58869] cherry_sdio: [sdio-verify] sdhci_init=pass card_detect=pass ocr=pass cid=pass csd=pass block_read=pass block_write=pass vfs_bdev=pass all=pass
[    0.206180] [6:cpu0:sched-mon:58869] cherry_shell: initialized with vxworks c-function call & addr2sym engine
[    0.221261] [6:cpu0:sched-mon:58869] cherry_shell: [shell-verify] syms=pass addr2sym=pass c_func=pass sum8=pass str=pass cmds=pass all=pass
[    0.222377] [6:cpu0:sched-mon:58869] freertos: [xsync-verify] sem=pass queue=pass mutex=pass spin=pass sg=16 st=16 qs=16 qr=16 qe=0 me=32 mc=32 merr=0 se=32 sc=32 serr=0
[SCHED-TEST] SUMMARY result=pass mode=cross cores=2 markers=1
scons: done building targets.
```

### 7.4 验证指标说明

在 `[sched-verify]` 标志中，各项测试代表的含义如下：
- `active=pass`：所有配置的 CPU 核心均已通过 PSCI 唤醒并在线注册。
- `idle=pass`：核心空闲任务状态与预期一致。
- `affinity=pass`：工作任务的 CPU 核心亲和性约束未发生越界违规。
- `coverage=pass`：所有核心均已执行过应用级任务。
- `progress=pass`：心跳计数（Heartbeat）平稳持续增长。
- `prio=pass`：高优先级任务抢占低优先级任务时序严格正确。
- `rr=pass`：同优先级任务在时间片轮转（Round-Robin）调度下公平分发。
- `sem / queue / mutex / spin=pass`：跨核同步与 IPC 原语安全可靠，无死锁、无竞态。

---

## 8. 技术演进与连载博客文档

本项目保留了从零编写并移植 FreeRTOS SMP 全过程的深度技术文档（Markdown 格式，撰写过程中的 `.docx` 原稿已在 `.gitignore` 中排除）：

| 阶段文档 | 主题与重点内容 |
|:---|:---|
| [Phase 1 技术博客](docs/phase1-AArch64裸机引导与UART输出.md) | **ARMv8-A 裸机引导与 UART 输出**：`startup.S`、内存布局 `linker.ld`、SCTLR 寄存器避坑、PL011 驱动与轻量级 `vsnprintf` |
| [Phase 2 (GICv2) 技术博客](docs/phase2-异常向量表与GICv2中断.md) | **异常向量表与 GICv2**：ARM 16 入口异常模型（VBAR_EL1）、中断上下文压栈与 ARM 通用定时器（SysTick）驱动 |
| [Phase 2 (GICv3) 技术博客](docs/phase2-GICv3中断与通用定时器.md) | **GICv3 系统寄存器升级**：ICC_SRE 接口、Group 1 非安全中断处理、Redistributor 配置与 EL2 降级 |
| [Phase 3 技术博客](docs/phase3-FreeRTOS集成.md) | **FreeRTOS 内核集成**：任务控制块（TCB）、单核上下文保存与恢复、SVC 系统调用与抢占式调度 |
| [Phase 4 技术博客](docs/phase4-FreeRTOS-SMP.md) | **FreeRTOS SMP 多核架构**：核间中断（SGI 0）、原子锁与自旋锁、PSCI 多核启动、32 核调度压测 |
| [Unikernel 架构说明](docs/unikernel-architecture.md) | **Unikernel 架构规范**：极简内核引导模型与轻量级应用融合设计 |
| [演进状态报告](status.md) | **SMP 内核调度器与 ARCH 抽象层设计报告**：各阶段调试实录与架构变更履历 |

---

## 9. 许可证

本项目所含的 FreeRTOS 内核代码遵循 FreeRTOS 原版 [MIT 许可证](https://github.com/FreeRTOS/FreeRTOS-Kernel/blob/main/LICENSE.md)。  
体系结构抽象层（`arch`）、板级支持包（`bsp`）、SCons 构建脚本及技术博客文档同样基于 MIT 许可证开源。详细授权条款参见 [LICENSE](LICENSE)。
