# FreeRTOS SMP for ARMv8-A (AArch64) - 模块化 CherryOS 集成平台

本项目为基于 **ARMv8-A (AArch64, Cortex-A53)** 架构与 **QEMU virt (GICv3)** 虚拟化平台的现代化 **FreeRTOS SMP 模块化操作系统集成运行环境**。

> [!NOTE]
> **模块化架构定位**：本系统遵循 **“模块化解耦与可组合性”** 设计原则，是一套完全模块化的现代操作系统集成基线。架构遵循 **“FreeRTOS 专注多核底座与任务调度”** 原则，负责 CPU 裸机引导、GICv3 中断路由、ARM Generic Timer、PSCI 多核启动及任务调度；上层所有核心操作系统能力（文件系统、网络栈、存储驱动、进程模型、动态链接器、图形与安全机制）均作为独立的 Git 子模块模块化拆分、独立维护，并通过通用内核抽象层（`cherry_kal`）进行高度解耦与按需集成。

---

## 1. 模块化生态与独立组件仓库矩阵

所有 Cherry 操作系统核心子系统与板级支持包均已彻底解耦并发布至独立的 Git 模块化仓库中：

| 模块名称 | 功能定位与核心特性 | 独立仓库地址 | 本地路径 |
| :--- | :--- | :--- | :--- |
| **`bsp`** | QEMU virt ARM64 模块化板级支持包（startup.S、GICv3、PL011 UART、设备注册） | [Surest-ok/qemu-virt-arm64](https://github.com/Surest-ok/qemu-virt-arm64) | [`bsp`](bsp) |
| **`cherry_vfs`** | Linux 兼容 Inode/Dentry 虚拟文件系统、平台总线与设备驱动框架 (`cdev`/`bdev`/`sockdev`) | [Surest-ok/cherry_vfs](https://github.com/Surest-ok/cherry_vfs) | [`components/cherry_vfs`](components/cherry_vfs) |
| **`cherry_process`** | 轻量级进程模型 (PCB)、POSIX Syscall 层、完全解耦 ELF64 动态链接器、Ubuntu GNU Bash 执行与 BusyBox 应用集 | [Surest-ok/cherry_process](https://github.com/Surest-ok/cherry_process) | [`components/cherry_process`](components/cherry_process) |
| **`cherry_ahci`** | Serial ATA AHCI 1.3+ 存储控制器，支持多端口 SATA/AHCI 设备枚举与 `/dev/sda`、`/dev/sdb` 块设备对接 | [Surest-ok/cherry_ahci](https://github.com/Surest-ok/cherry_ahci) | [`components/cherry_ahci`](components/cherry_ahci) |
| **`cherry_ext4`** | Ext4 日志文件系统原生驱动、JBD2 事务管理、`ext4_mkfs` 与 `ext4_fsck` 修复工具 | [Surest-ok/cherry_ext4](https://github.com/Surest-ok/cherry_ext4) | [`components/cherry_ext4`](components/cherry_ext4) |
| **`cherry_fstools`** | MBR 分区解析、分区子块设备映射、`mount`/`umount` 与 `fs` 核心存储实用工具 | [Surest-ok/cherry_fs](https://github.com/Surest-ok/cherry_fs) | [`components/cherry_fstools`](components/cherry_fstools) |
| **`cherry_net`** | 多核多队列 (MQ-RSS) TCP/IP 协议栈、VirtIO-Net PCI 硬件加速驱动与 BSD 套接字 | [Surest-ok/cherry_net](https://github.com/Surest-ok/cherry_net) | [`components/cherry_net`](components/cherry_net) |
| **`cherry_pci`** | PCIe ECAM 主机桥枚举、BAR 资源分配、MSI/MSI-X 与 GICv3 ITS 中断分发驱动 | [Surest-ok/cherry_pci](https://github.com/Surest-ok/cherry_pci) | [`components/cherry_pci`](components/cherry_pci) |
| **`cherry_nvme`** | PCIe NVMe 1.4+ 固态存储子系统、PRP 分页内存映射与 `/dev/nvme0n1` 块设备对接 | [Surest-ok/cherry_nvme](https://github.com/Surest-ok/cherry_nvme) | [`components/cherry_nvme`](components/cherry_nvme) |
| **`cherry_sdio`** | SD / SDIO / eMMC 协议栈、SDHCI PCI 控制器驱动与 `/dev/mmcblk0` 块设备驱动 | [Surest-ok/cherry_sdio](https://github.com/Surest-ok/cherry_sdio) | [`components/cherry_sdio`](components/cherry_sdio) |
| **`cherry_printk`** | 多核并发无交错 Linux 规范日志引擎，纳秒级关中断入队与异步开中断输出 | [Surest-ok/cherry_printk](https://github.com/Surest-ok/cherry_printk) | [`components/cherry_printk`](components/cherry_printk) |
| **`cherry_timer`** | 硬件纳秒时钟基准、Per-CPU 独立队列、Linux 标准 `timer_list` 与 `hrtimer` 定时器 | [Surest-ok/cherry_timer](https://github.com/Surest-ok/cherry_timer) | [`components/cherry_timer`](components/cherry_timer) |
| **`cherry_shell`** | VxWorks 风格终端 C 函数直接调用、多参数无缝传递与 `addr2sym` 双向符号反查引擎 | [Surest-ok/cherry_shell](https://github.com/Surest-ok/cherry_shell) | [`components/cherry_shell`](components/cherry_shell) |
| **`cherry_kal`** | 通用内核抽象层 (OSAL)，将具体底层 RTOS 接口标准化为 Linux 语义 | [Surest-ok/cherry_kal](https://github.com/Surest-ok/cherry_kal) | [`components/cherry_kal`](components/cherry_kal) |
| **`cherry_usbd`** | USB 2.0/3.0 设备协议栈、Virtual UDC 环回驱动、CDC-ACM、MSC BOT 与 HID 引擎 | [Surest-ok/cherry_usbd](https://github.com/Surest-ok/cherry_usbd) | [`components/cherry_usbd`](components/cherry_usbd) |
| **`cherry_kmod`** | Linux 规范内核模块动态装载与符号重定位引擎 | [Surest-ok/cherry_kmod](https://github.com/Surest-ok/cherry_kmod) | [`components/cherry_kmod`](components/cherry_kmod) |
| **`cherry_ebpf`** | eBPF 虚拟机、内核探针 (kprobe/tracepoint) 与执行跟踪引擎 | [Surest-ok/cherry_ebpf](https://github.com/Surest-ok/cherry_ebpf) | [`components/cherry_ebpf`](components/cherry_ebpf) |
| **`cherry_drm`** | Direct Rendering Manager 显示与帧缓冲驱动框架 | [Surest-ok/cherry_drm](https://github.com/Surest-ok/cherry_drm) | [`components/cherry_drm`](components/cherry_drm) |
| **`cherry_mesa`** | 3D 渲染与图形管线用户态支撑库 | [Surest-ok/cherry_mesa](https://github.com/Surest-ok/cherry_mesa) | [`components/cherry_mesa`](components/cherry_mesa) |

---

## 2. 模块化集成架构

> [!TIP]
> 架构图由 [Archify](https://github.com/tt-a1i/archify) 生成。点击可查看 [交互式 HTML 架构图](docs/diagrams/cherry-modular-os-architecture.html) 进行全景探索、高亮路径追踪与节点检查。

<p align="center">
  <img src="docs/diagrams/cherry-modular-os-architecture.svg" alt="Cherry Modular OS 现代化模块化架构" width="100%" />
</p>

---

## 3. 核心功能特性

### 3.1 纯净 EL1 内核运行模式
移除冗余 EL2 Hypervisor 实现，内核运行在标准的 Exception Level 1 (EL1)。启动引导流程精简可靠，具备更清晰的异常向量表与内存映射。

### 3.2 多端口 SATA/AHCI 与根文件系统挂载
- **SATA Port 0 (`/dev/sda`)**：挂载标准 `rootfs.ext4`，承载系统基础命令、驱动模块与 BusyBox 应用。
- **SATA Port 1 (`/dev/sdb`)**：挂载 64MB `ubuntu_rootfs.ext4` 到根目录 `/`，提供原生 Ubuntu AArch64 用户空间文件体系。

### 3.3 原生 GNU Bash 与动态链接应用加载
- **ELF64 动态链接器支持**：内核原生解析 GNU `PT_INTERP`，自动加载 `/lib/ld-linux-aarch64.so.1` 动态链接器，映射 `libc.so.6` 与 `libtinfo.so.6`。
- **System V ABI 与 POSIX 系统调用**：完整支持用户态 `argc`/`argv`/`envp`、`brk`/`mmap`/`munmap`、`ioctl`（TTY 终端控制）、`rt_sigaction`/`rt_sigprocmask`、`getpid`/`getppid`、`fork`/`execve`/`wait4`、`pipe` 等核心调用。
- **原生 Bash 执行**：支持 `bash -c "echo ..."` 单行命令执行与 `/bin/test_bash.sh` 复杂脚本运行（循环变量展开、`$((12+30))` 算术求值、退出码精确捕获）。

### 3.4 工业级 BusyBox 多调用工具集
内置 29 种 BusyBox 工具（`ls`, `cat`, `echo`, `sh`, `ps`, `pwd`, `mkdir`, `rm`, `kill`, `head`, `tail`, `wc`, `grep`, `sleep` 等），支持管道通信、重定向与多核任务调度观察。

---

## 4. 目录结构

```text
freertos-workspace/
├── SConstruct                  # SCons 顶层构建编排脚本（模块联动编译与 QEMU/测试目标）
├── Makefile                    # 快捷构建命令行封装
├── kernel/                     # FreeRTOS V11 SMP 官方内核源码及 AArch64 端口实现
│   ├── FreeRTOS-Kernel/        # 核心任务调度器 (tasks.c, queue.c, list.c 等)
│   └── port/                   # GCC/ARM_AARCH64_SRE 移植层 (port.c, portASM.S)
├── arch/                       # 体系结构抽象层 (原子操作、自旋锁、中断与屏障)
├── bsp/                        # 板级支持包 (Git 子模块: Surest-ok/qemu-virt-arm64)
├── components/                 # CherryOS 现代化操作系统模块库 (18 个独立子模块仓库)
├── scripts/                    # 构建辅助脚本 (build_ubuntu_rootfs.py, run-qemu.sh 等)
└── tests/                      # 自动化测试编排脚本 (accept_qemu_sched.py)
```

---

## 5. 快速上手与功能复现

### 5.1 环境准备

确保主机（Ubuntu 20.04 / 22.04 或 WSL2）已安装交叉编译器、QEMU 与 SCons：

```bash
sudo apt-get update
sudo apt-get install -y gcc-aarch64-linux-gnu g++-aarch64-linux-gnu qemu-system-arm python3 python3-pip e2fsprogs
pip3 install scons
```

### 5.2 完整代码拉取（含全部 19 个模块化子模块）

克隆主仓库并递归拉取所有依赖子模块：

```bash
git clone --recursive -b vibe-coding-dev https://github.com/Surest-ok/FreeRTOS.git
cd FreeRTOS
```

### 5.3 编译与构建

```bash
# 1. 编译全系统内核镜像 (生成 build/bsp/freertos_smp.elf)
scons

# 2. 构建 Ubuntu 根文件系统镜像（用于原生 GNU Bash 动态执行验证）
python3 scripts/build_ubuntu_rootfs.py
```

### 5.4 自动化验收测试

运行包含 FreeRTOS 多核调度、全组件联动与 GNU Bash / BusyBox 执行的端到端自动化验收：

```bash
# 1. 运行全功能自动化验收（默认双核 SMP，包含多磁盘挂载与原生应用执行）
scons test

# 2. 运行高压力跨核同步压测 (Cross-core Stress)
scons SCHED_MODE=cross test

# 3. 指定核心数编译并交互式运行 QEMU (例如 4 核心)
scons CORES=4 run
# (使用 Ctrl+A, X 退出 QEMU 模拟器)
```

**验收通过标志输出**：
```text
cherry_process: [process-verify] spawn=pass wait=pass fd=pass pipe=pass syscall=pass applets=pass sh=pass smp_ps=pass elf_glibc=pass nopie=pass dyn=pass all=pass
[SCHED-TEST] SUMMARY result=pass mode=full cores=2 markers=1
```

---

## 6. 许可证

本项目遵循 [MIT 许可证](LICENSE)。FreeRTOS 原版内核代码遵循其官方 [MIT 许可证](https://github.com/FreeRTOS/FreeRTOS-Kernel/blob/main/LICENSE.md)。
