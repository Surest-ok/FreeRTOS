# FreeRTOS SMP for AArch64

[![License: MIT](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![Kernel](https://img.shields.io/badge/kernel-FreeRTOS%20V11%20SMP-informational)](kernel)
[![Arch](https://img.shields.io/badge/arch-AArch64-informational)](arch)
[![Platform](https://img.shields.io/badge/platform-QEMU%20virt-orange)](#快速开始)
[![Build](https://img.shields.io/badge/build-SCons-blue)](SConstruct)

在 ARMv8-A（Cortex-A53）和 QEMU `virt` 上运行的 FreeRTOS V11 SMP，并在内核之上加入第一批组件：内核抽象层、多核日志、定时器、虚拟文件系统与设备模型、串口 Shell。

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

当前分支：**`cherry-vfs`**。

## 特性

- **FreeRTOS V11 SMP**：1–32 核，任务亲和性、优先级抢占、时间片轮转，跨核 Yield 走 GICv3 SGI 0。
- **组件化**：组件放在 `components/` 下，各自编译成静态库，只依赖 `cherry_kal` 提供的内核原语，不直接调用 FreeRTOS。
- **日志、定时器与文件系统**：多核不交错的 `printk`，`timer_list` / `hrtimer`，带 devfs、ramfs 和设备模型的 VFS。
- **串口 Shell**：直接按名字调用 C 函数，地址与符号互查。
- **自检与验收**：每个组件初始化后运行自己的自检；一条命令在 QEMU 上完成多核调度验收。

## 快速开始

```bash
# 1. 安装依赖（Ubuntu 20.04 或更新版本，实体机、虚拟机、WSL2 都可以）
sudo apt-get update
sudo apt-get install -y gcc-aarch64-linux-gnu qemu-system-arm python3 python3-pip
pip3 install scons            # 装在 ~/.local/bin，确认它在 PATH 里

# 2. 获取代码
git clone -b cherry-vfs https://github.com/Surest-ok/FreeRTOS.git
cd FreeRTOS

# 3. 构建，生成 build/bsp/freertos_smp.elf 和 freertos_smp.bin
scons -j$(nproc)

# 4. 在 QEMU 上运行（默认 2 核）
scons run

# 5. 运行验收
scons test
```

退出 QEMU：先按 `Ctrl+A`，松开后再按 `X`。

QEMU 只接 PL011 串口，不需要额外的磁盘镜像。

启动后每个组件打印一行自检结果，例如：

```text
[    0.027849] [6:cpu0:sched-mon:60111] [printk-verify] basic=pass multicore=pass irq_safe=pass all=pass
[    0.061194] [6:cpu0:sched-mon:60111] [timer-verify] standard=pass hrtimer=pass multi_core=pass restart=pass cancel=pass all=pass
```

## 组件

| 组件 | 作用 |
| --- | --- |
| [`cherry_kal`](components/cherry_kal/README.md) | 内核抽象层：`kal_mutex_*`、`kal_sem_*`、`kal_spin_*`、`kal_thread_*`、`kal_malloc` 等；其余组件只依赖它 |
| [`cherry_printk`](components/cherry_printk/README.md) | 多核日志：`printk` / `pr_*`，格式 `[秒.微秒] [级别:核:任务:任务号] 内容`，每行整体输出不交错 |
| [`cherry_timer`](components/cherry_timer/README.md) | 定时器：`timer_list` 与 `hrtimer`，每核一个定时器队列，时基为 Generic Timer |
| [`cherry_vfs`](components/cherry_vfs/README.md) | 虚拟文件系统与设备模型：inode / dentry、devfs、ramfs，字符 / 块 / 套接字设备，平台总线与驱动绑定 |
| [`cherry_shell`](components/cherry_shell/README.md) | 串口 Shell：按名字直接调用 C 函数（最多 8 个参数），地址与符号互查（`addr2sym`），内置 `ls`、`cat`、`ps` 等命令 |

组件之间的依赖：`cherry_kal` 在最底层，`cherry_printk`、`cherry_timer` 在它之上，`cherry_vfs` 提供设备模型和文件接口，其余组件把自己的设备或文件系统注册到 `cherry_vfs`。

## 启动流程

<p align="center"><img src="docs/images/boot.svg" alt="启动流程" width="900"></p>

1. `bsp/src/startup.S`：必要时从 EL2 降到 EL1，按 `MPIDR` 设置每核栈，清零 BSS，安装向量表。
2. `ukernel_start`（`bsp/src/ukernel.c`）：初始化串口、GICv3 和 Generic Timer，创建任务，主核启动 FreeRTOS 调度。
3. 监控任务用 PSCI `CPU_ON` 拉起其余的核。
4. `bsp/src/app.c` 按上图顺序初始化各组件并运行自检，然后创建调度验收探针，最后启动串口 Shell。

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
├── SConstruct            构建编排：内核、arch、组件、BSP，QEMU 运行与验收目标
├── Makefile              scons 的快捷封装
├── components/           组件，每个含 include/、src/、tests/、SConscript、README.md
│   ├── cherry_kal/       内核抽象层
│   ├── cherry_printk/    多核日志
│   ├── cherry_timer/     定时器
│   ├── cherry_vfs/       虚拟文件系统与设备模型
│   └── cherry_shell/     串口 Shell
├── kernel/               FreeRTOS V11 SMP 内核、MemMang/、port/（AArch64 SMP 移植）
├── arch/                 arch 抽象层：include/arch_api/ 接口、aarch64/arch.c 实现、rules.py 编译规则
├── include/              FreeRTOSConfig.h、portmacro.h 与公共头文件
├── bsp/                  启动、GICv3、定时器、PL011、组件初始化与验收探针（app.c）
├── tests/acceptance/     验收脚本 accept_qemu_sched.py
├── docs/                 技术文档与插图（images/）
└── status.md             演进记录
```

## 文档

| 文档 | 内容 |
| --- | --- |
| [Phase 1](docs/phase1-AArch64裸机引导与UART输出.md) | 裸机引导、链接脚本、SCTLR、PL011 驱动与 `vsnprintf` |
| [Phase 2（GICv2）](docs/phase2-异常向量表与GICv2中断.md) | 异常向量表、中断上下文、通用定时器 |
| [Phase 2（GICv3）](docs/phase2-GICv3中断与通用定时器.md) | ICC 系统寄存器、Group 1 非安全中断、Redistributor、EL2 降级 |
| [Phase 3](docs/phase3-FreeRTOS集成.md) | FreeRTOS 集成：TCB、上下文保存与恢复、SVC 与抢占调度 |
| [Phase 4](docs/phase4-FreeRTOS-SMP.md) | FreeRTOS SMP：SGI 0 跨核 Yield、自旋锁、PSCI 多核启动、32 核压测 |
| [单镜像架构](docs/unikernel-architecture.md) | 启动、BSP、内核、运行时与应用在一个 ELF 里的组织方式 |
| [演进记录](status.md) | SMP 调度器与 arch 抽象层的修改和调试记录 |

## 现状与限制

- **本分支的验收当前不通过**：在 2 核上，Core 1 的 worker 始终没有运行（`[sched-verify] ... coverage=fail ... seen=0x1 missing=0x2`），之后的分支已修复。组件自检和 `[xsync-verify]` 正常。
- 系统运行在 Non-Secure EL1，不实现 EL3 / EL2 的逻辑。
- 不启用 MMU 页表隔离：所有任务共用一个扁平的物理地址空间。
- 单一静态链接的镜像，不支持动态加载。
- 默认以 `-mgeneral-regs-only` 编译，任务切换不保存 NEON 寄存器。
- 只在 QEMU `virt` 上运行过。

## 许可证

[MIT](LICENSE)。`kernel/` 下的 FreeRTOS 内核源码同样采用 MIT 许可证，各文件保留其原始版权声明。
