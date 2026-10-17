# FreeRTOS SMP for ARMv8-A (AArch64) on QEMU virt

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![Build System](https://img.shields.io/badge/Build-SCons-blue.svg)](https://scons.org/)
[![Architecture](https://img.shields.io/badge/Arch-AArch64%20%2F%20Cortex--A53-orange.svg)]()
[![Platform](https://img.shields.io/badge/Platform-QEMU%20virt-green.svg)]()
[![RTOS](https://img.shields.io/badge/RTOS-FreeRTOS%20V11%20SMP-red.svg)](https://freertos.org/)

本项目是一个基于 **FreeRTOS SMP (Symmetric Multiprocessing)** 的模块化、高可靠、完全可复现的 64 位对称多核实时操作系统基准环境。项目以 **ARMv8-A (AArch64, Cortex-A53)** 架构与 **QEMU virt (GICv3)** 为目标硬件平台，提供工业级的模块化体系结构抽象层（`arch`）、标准化的 FreeRTOS 多核调度扩展（`kernel`）、完整的板级支持包（`bsp`）以及现代化的 **SCons** 构建体系。

项目采用清晰分层的模块化架构，作为一套纯净、自包含、生产可用的 FreeRTOS SMP 模块化操作系统基准。

---

## 1. 核心模块化特性

- **模块化分层设计**：严格解耦体系结构层（`arch`）、板级支持包（`bsp`）与实时内核（`kernel`），模块间接口正交、职责明确。
- **标准 FreeRTOS SMP 内核**：采用官方 FreeRTOS V11.x SMP 内核，原生支持任务亲和性绑定（Affinity）、时间片轮转、优先级抢占及全套多核同步原语（队列、信号量、互斥锁）。
- **AArch64 纯净架构支持**：适配 64 位 ARMv8-A 体系结构，针对 Cortex-A53 提供优化的上下文保存/恢复汇编指令与内存屏障控制。
- **GICv3 中断控制器**：全面支持 ARM GICv3 系统寄存器接口（`ICC_SRE`），实现 Group 1 非安全中断路由、纳秒级 ARM 通用定时器（Generic Timer）时钟驱动以及 SGI 核间中断（IPI）调度分发。
- **PSCI 多核引导**：遵循 ARM PSCI 工业标准规范，通过安全监视调用（SMC/HVC `CPU_ON`）实现多从核动态唤醒与冷启动初始化。
- **自包含与零外部依赖**：代码仓库完全独立，无需拉取外部第三方子模块，开箱即用。
- **自动化测试编排**：内置完整的调度器多核验证套件，支持基础探针测试（Full）与高强度跨核同步压测（Cross-core Stress）。

---

## 2. 模块化系统架构

```
┌────────────────────────────────────────────────────────────────────────┐
│                        用户多任务应用程序 (app.c)                       │
└───────────────────────────────────┬────────────────────────────────────┘
                                    │ FreeRTOS API
┌───────────────────────────────────▼────────────────────────────────────┐
│                    FreeRTOS V11 SMP 多核调度内核 (kernel/)              │
│  - tasks.c / queue.c / list.c / timers.c / event_groups.c              │
│  - 多核任务调度、亲和性分配、优先级抢占、自旋锁与核间 Yield 路由          │
└───────────────────┬────────────────────────────────┬───────────────────┘
                    │                                │
┌───────────────────▼─────────────┐    ┌─────────────▼───────────────────┐
│     模块化体系结构层 (arch/)    │    │      模块化板级支持包 (bsp/)    │
│  - MPIDR 核心编号映射            │    │  - startup.S (冷启动/异常向量表) │
│  - 原生原子指令与自旋锁          │    │  - gic.c (GICv3 系统寄存器)      │
│  - 内存屏障与关中断原子上下文    │    │  - gtimer.c (通用物理定时器)     │
│  - 共享线程状态句柄抽象          │    │  - uart.c (PL011 串口驱动)       │
└─────────────────────────────────┘    └─────────────────────────────────┘
                    │                                │
                    └───────────────┬────────────────┘
                                    │
┌───────────────────────────────────▼────────────────────────────────────┐
│              目标硬件 / 虚拟机平台 (QEMU virt, Cortex-A53 SMP)          │
└────────────────────────────────────────────────────────────────────────┘
```

---

## 3. 目录结构

```text
freertos-workspace/
├── SConstruct                  # SCons 顶层构建编排脚本（多核配置与自动化测试目标）
├── Makefile                    # 快捷构建命令行封装
├── arch/                       # 体系结构抽象层（原子操作、自旋锁、中断与屏障）
│   ├── rules.py                # 体系结构编译工具链与通用参数规则
│   ├── aarch64/                # AArch64 具体硬件原语实现 (arch.c)
│   └── include/                # 体系结构通用 API 头文件
├── bsp/                        # 板级支持包（QEMU virt 裸机启动与外设驱动）
│   ├── SConscript              # BSP 组件构建规则
│   ├── linker.ld               # 统一内存布局与段映射链接脚本
│   ├── include/                # 板级外设驱动头文件 (gic.h, gtimer.h, uart.h 等)
│   └── src/                    # 启动引导 startup.S、向量表与外设驱动源码
├── kernel/                     # FreeRTOS V11 SMP 官方内核源码及 AArch64 移植层
│   ├── SConscript              # 内核库静态编译脚本
│   ├── tasks.c / queue.c ...   # FreeRTOS 核心组件
│   └── port/                   # GCC/ARM_AARCH64_SRE 架构移植层 (port.c, portASM.S)
└── tests/                      # 自动化测试编排套件
    └── acceptance/             # 调度器多核并发验收脚本 (accept_qemu_sched.py)
```

---

## 4. 快速上手

### 4.1 环境准备

确保主机环境（Ubuntu 20.04 / 22.04 或 WSL2）已安装 AArch64 交叉编译器、QEMU 仿真器与 SCons：

```bash
# 安装基础编译与仿真依赖
sudo apt-get update
sudo apt-get install -y gcc-aarch64-linux-gnu g++-aarch64-linux-gnu qemu-system-arm python3 python3-pip
pip3 install scons
```

### 4.2 编译与构建

通过 SCons 执行编译，构建产物将输出至 `build/` 目录：

```bash
# 1. 默认构建 (双核 SMP, 生成 build/bsp/freertos_smp.elf 与 freertos_smp.bin)
scons

# 2. 指定核心数构建 (例如 4 核心)
scons CORES=4

# 3. 指定日志级别构建 (0-7, 7 启用深度调度追踪)
scons LOG_LEVEL=7
```

### 4.3 模拟仿真运行

```bash
# 启动 QEMU 进行交互式运行
scons run
# (使用 Ctrl+A, X 退出 QEMU 仿真器)
```

### 4.4 自动化验收测试

项目集成了端到端的多核调度自动化验收套件，通过 QEMU 自动捕获串口标志并校验任务行为：

```bash
# 1. 执行全功能多核调度验证 (Full Mode)
scons test

# 2. 执行跨核同步高压测试 (Cross-core Stress Mode: 验证跨核队列、信号量、互斥锁与自旋锁)
scons SCHED_MODE=cross test
```

**验收通过标志输出**：
```text
freertos: [sched-verify] active=PASS idle=PASS affinity=PASS coverage=PASS progress=PASS prio=PASS rr=PASS sem=PASS queue=PASS mutex=PASS spin=PASS cores=2 seen=0x00000003 missing=0x00000000 hb0=2 hb1=103
[SCHED-TEST] SUMMARY result=PASS mode=full cores=2 markers=2
scons: done building targets.
```

---

## 5. 许可证

本项目所含的 FreeRTOS 内核代码遵循 FreeRTOS 原版 [MIT 许可证](https://github.com/FreeRTOS/FreeRTOS-Kernel/blob/main/LICENSE.md)。体系结构适配层与板级支持包代码同样采用 [MIT 许可证](LICENSE)。
