# FreeRTOS SMP for AArch64

[![License: MIT](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![Kernel](https://img.shields.io/badge/kernel-FreeRTOS%20V11%20SMP-informational)](kernel)
[![Arch](https://img.shields.io/badge/arch-AArch64-informational)](arch)
[![Platform](https://img.shields.io/badge/platform-QEMU%20virt-orange)](#快速开始)
[![Build](https://img.shields.io/badge/build-SCons-blue)](SConstruct)

在 ARMv8-A（Cortex-A53）和 QEMU `virt` 上运行的 FreeRTOS V11 SMP。仓库包含内核、AArch64 SMP 移植层、与内核无关的 arch 抽象层、板级支持包和 SCons 构建，并带有一套在 QEMU 上自动运行的多核调度验收。

<p align="center"><img src="docs/images/architecture.svg" alt="FreeRTOS SMP 系统架构" width="900"></p>

## 分支

| 分支 | 内容 |
| --- | --- |
| `master` | FreeRTOS SMP 内核、AArch64 移植层、arch 抽象层和 BSP，以及多核调度验收 |
| `cherry-vfs` | 加入第一批组件：`cherry_kal`、`cherry_printk`、`cherry_timer`、`cherry_vfs`、`cherry_shell` |
| `cherry-pci-sdio` | 再加入 `cherry_pci`（含 GICv3 ITS）和 `cherry_sdio` |
| `cherry-ext4-fstools` | 再加入 `cherry_ext4` 和 `cherry_fstools` |
| `vibe-coding-dev` | 组件拆成 18 个独立仓库（子模块），加入网络、NVMe、AHCI、进程与用户空间等 |
| `technical-blogs` | 从零移植的技术博客，以及 FreeRTOS、LK、ThreadX 共用 arch 层的对比实验 |

当前分支：**`master`**。

## 特性

- **FreeRTOS V11 SMP**：1–32 核，任务亲和性、优先级抢占、同优先级时间片轮转，队列、信号量、互斥锁。
- **AArch64 SMP 移植**：上下文保存与恢复、每核异常栈，跨核 Yield 走 GICv3 SGI 0。
- **arch 抽象层**：核号与 MPIDR 映射、原子操作（内联 LL/SC）、屏障、原始 / 递归自旋锁、IRQ 注册与分发、异常记录。
- **BSP**：EL2 → EL1 启动，GICv3（系统寄存器接口）、Generic Timer（1000 Hz）、PL011 串口，PSCI `CPU_ON` 拉起从核。
- **自动化验收**：一条命令在 QEMU 上检查各核在线、亲和性、优先级、轮转和同步原语，另有跨核同步压测模式。

## 快速开始

```bash
# 1. 安装依赖（Ubuntu 20.04 或更新版本，实体机、虚拟机、WSL2 都可以）
sudo apt-get update
sudo apt-get install -y gcc-aarch64-linux-gnu qemu-system-arm python3 python3-pip
pip3 install scons            # 装在 ~/.local/bin，确认它在 PATH 里

# 2. 获取代码（bsp 是子模块，必须带 --recursive）
git clone --recursive -b master https://github.com/Surest-ok/FreeRTOS.git
cd FreeRTOS

# 3. 构建，生成 build/bsp/freertos_smp.elf 和 freertos_smp.bin
scons -j$(nproc)

# 4. 在 QEMU 上运行（默认 2 核）
scons run

# 5. 运行验收
scons test
```

退出 QEMU：先按 `Ctrl+A`，松开后再按 `X`。

验收通过时的最后两行：

```text
freertos: [sched-verify] active=PASS idle=PASS affinity=PASS coverage=PASS progress=PASS prio=PASS rr=PASS sem=PASS queue=PASS mutex=PASS spin=PASS cores=2 ...
[SCHED-TEST] SUMMARY result=PASS mode=full cores=2 markers=2
```

## 启动流程

<p align="center"><img src="docs/images/boot.svg" alt="启动流程" width="900"></p>

1. `bsp/src/startup.S`：从 `0x40000000` 进入，必要时从 EL2 降到 EL1，按 `MPIDR` 为每个核设置栈，清零 BSS，安装向量表。
2. `ukernel_start`（`bsp/src/ukernel.c`）：初始化串口、GICv3 和 Generic Timer，创建任务，主核启动 FreeRTOS 调度。
3. 监控任务用 PSCI `CPU_ON` 拉起其余的核，它们进入同一个调度器。
4. `bsp/src/app.c` 创建验收探针：Core 0 的监控任务，以及固定在各核上的 worker 和同步原语探针。监控任务周期性汇总结果，打印 `[sched-verify]`。

## 构建选项

选项写在 `scons` 命令行上，例如 `scons CORES=4 run`：

| 选项 | 默认值 | 说明 |
| --- | --- | --- |
| `CORES` | `2` | 核数（1–32），同时决定 FreeRTOS 的 `configNUMBER_OF_CORES` 和 QEMU 的 `-smp` |
| `SCHED_MODE` | `full` | 验收模式：`full`，或跨核压测 `cross`、`cross-sem`、`cross-queue`、`cross-mutex`、`cross-spin` |
| `LOG_LEVEL` | `6` | 日志级别 0–7，7 打开 SMP 诊断 |
| `TICK_BROADCAST` | `1` | 时钟中断是否广播到所有核 |
| `TIMEOUT` | `30` | 验收超时，秒 |
| `CROSS_COMPILE` | `aarch64-linux-gnu-` | 交叉工具链前缀 |
| `BUILD_DIR` | `build` | 构建目录 |

`make` 是 `scons` 的薄封装：`make all`、`make run`、`make sched-full`、`make sched-cross`、`make sched-smp`、`make clean`。

## 测试

```bash
scons test                     # full：调度、亲和性、优先级、轮转与同步原语
scons SCHED_MODE=cross test    # cross：跨核信号量、队列、互斥锁、自旋锁压测
scons CORES=4 test             # 换核数
```

<p align="center"><img src="docs/images/testing.svg" alt="自动化验收" width="900"></p>

`tests/acceptance/accept_qemu_sched.py` 启动 QEMU、读取串口。对应模式的标记（`full` 为 `[sched-verify]`，`cross` 为 `[xsync-verify]`）每一项都是 pass 时输出 `[SCHED-TEST] SUMMARY result=pass`；出现 panic、abort 等异常或超时则判失败。

`[sched-verify]` 各项的含义：

| 项 | 通过条件 |
| --- | --- |
| `active` | 所有核都已经由 PSCI 拉起并完成初始化 |
| `idle` | 空闲任务的状态符合预期 |
| `affinity` | 固定核的 worker 没有跑到别的核上 |
| `coverage` | 每个核都运行过应用任务 |
| `progress` | 各核的 heartbeat 持续增长 |
| `prio` | 高优先级任务能抢占低优先级任务 |
| `rr` | 同优先级任务按时间片轮转 |
| `sem` / `queue` / `mutex` / `spin` | 同步原语工作正常，没有死锁和竞态 |

## 目录结构

```text
.
├── SConstruct            构建编排：内核、arch、BSP，QEMU 运行与验收目标
├── Makefile              scons 的快捷封装
├── kernel/               FreeRTOS V11 SMP 内核（tasks.c、queue.c 等）、MemMang/、port/（AArch64 SMP 移植）
├── arch/                 arch 抽象层：include/arch_api/ 接口、aarch64/arch.c 实现、rules.py 编译规则
├── include/              FreeRTOSConfig.h、portmacro.h 与板级公共头文件
├── bsp/                  板级支持包（子模块 qemu-virt-arm64）：startup.S、GICv3、定时器、PL011、验收探针
├── scripts/              直接启动 QEMU 的脚本
├── tests/acceptance/     验收脚本 accept_qemu_sched.py
└── docs/images/          插图
```

## 现状与限制

- 系统运行在 Non-Secure EL1，不实现 EL3 / EL2 的逻辑。
- 不启用 MMU 页表隔离：所有任务共用一个扁平的物理地址空间。
- 单一静态链接的镜像，不支持动态加载。
- 默认以 `-mgeneral-regs-only` 编译，任务切换不保存 NEON 寄存器。
- 只在 QEMU `virt` 上运行过。

## 许可证

[MIT](LICENSE)。`kernel/` 下的 FreeRTOS 内核源码同样采用 MIT 许可证，各文件保留其原始版权声明。
