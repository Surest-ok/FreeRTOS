# FreeRTOS SMP for ARMv8-A (AArch64) - Unikernel 集成平台

本项目为基于 **ARMv8-A (AArch64) 架构** 与 **QEMU virt 虚拟化平台** 的现代化 **FreeRTOS SMP Unikernel 集成与运行环境**。

> [!NOTE]
> **集成平台定位**：本仓库遵循 **“FreeRTOS 只负责多核底座与集成”** 的轻量化设计原则，承担 CPU 裸机引导、GICv3 中断控制器、ARM Generic Timer、PSCI 多核冷启动、FreeRTOS V11 SMP 任务调度及多核压测验收。所有操作系统上层能力均全面组件化，拆分至独立的组件仓库开发维护，并通过通用内核抽象层（`cherry_kal`）与 FreeRTOS 解耦集成。

---

## 1. 组件生态与独立仓库矩阵

所有 Cherry 操作系统核心子系统均已解耦并发布至独立的组件仓库中，本仓库作为集成运行时统筹编译与多核联动：

| 组件名称 | 功能定位与说明 | 独立仓库地址 | 本地路径 |
| :--- | :--- | :--- | :--- |
| **`cherry_vfs`** | Linux 兼容 Inode/Dentry 虚拟文件系统、平台总线与设备驱动框架 (`cdev`/`bdev`/`sockdev`) | [Surest-ok/cherry_vfs](https://github.com/Surest-ok/cherry_vfs) | [`components/cherry_vfs`](components/cherry_vfs) |
| **`cherry_printk`** | 多核并发无交错 Linux 规范日志引擎，纳秒级关中断入队与异步开中断输出 | [Surest-ok/cherry_printk](https://github.com/Surest-ok/cherry_printk) | [`components/cherry_printk`](components/cherry_printk) |
| **`cherry_timer`** | 硬件纳秒时钟基准、Per-CPU 独立队列、Linux 标准 `timer_list` 与 `hrtimer` 定时器 | [Surest-ok/cherry_timer](https://github.com/Surest-ok/cherry_timer) | [`components/cherry_timer`](components/cherry_timer) |
| **`cherry_shell`** | VxWorks 风格终端 C 函数直接调用、多参数无缝传递与 `addr2sym` 双向符号反查引擎 | [Surest-ok/cherry_shell](https://github.com/Surest-ok/cherry_shell) | [`components/cherry_shell`](components/cherry_shell) |
| **`cherry_pci`** | PCIe ECAM 主机桥枚举、BAR 资源分配、MSI/MSI-X 与 GICv3 ITS 中断分发驱动 | [Surest-ok/cherry_pci](https://github.com/Surest-ok/cherry_pci) | [`components/cherry_pci`](components/cherry_pci) |
| **`cherry_sdio`** | SD / SDIO / eMMC 协议栈、SDHCI PCI 控制器驱动与 `/dev/mmcblk0` 块设备驱动 | [Surest-ok/cherry_sdio](https://github.com/Surest-ok/cherry_sdio) | [`components/cherry_sdio`](components/cherry_sdio) |
| **`cherry_ext4`** | Ext4 日志文件系统原生驱动、JBD2 事务管理、`ext4_mkfs` 与 `ext4_fsck` 修复工具 | [Surest-ok/cherry_ext4](https://github.com/Surest-ok/cherry_ext4) | [`components/cherry_ext4`](components/cherry_ext4) |
| **`cherry_fs`** | MBR 分区解析、分区子块设备映射、`mount`/`umount` 与 `fs` 引导核心存储实用工具 | [Surest-ok/cherry_fs](https://github.com/Surest-ok/cherry_fs) | [`components/cherry_fstools`](components/cherry_fstools) |
| **`cherry_net`** | 多核多队列 (MQ-RSS) TCP/IP 协议栈、VirtIO-Net PCI 硬件加速驱动与 BSD 套接字 | [Surest-ok/cherry_net](https://github.com/Surest-ok/cherry_net) | [`components/cherry_net`](components/cherry_net) |
| **`cherry_nvme`** | PCIe NVMe 1.4+ 固态存储子系统、PRP 分页内存映射与 `/dev/nvme0n1` 块设备对接 | [Surest-ok/cherry_nvme](https://github.com/Surest-ok/cherry_nvme) | [`components/cherry_nvme`](components/cherry_nvme) |
| **`cherry_ahci`** | Serial ATA AHCI 1.3+ 存储控制器、FIS/PRDT 重基址引擎与 `/dev/sda` 块设备对接 | [Surest-ok/cherry_ahci](https://github.com/Surest-ok/cherry_ahci) | [`components/cherry_ahci`](components/cherry_ahci) |
| **`cherry_process`** | 轻量级进程模型 (PCB)、POSIX Syscall 层、BusyBox 25+ 应用集与完全解耦 ELF64 加载器 | [Surest-ok/cherry_process](https://github.com/Surest-ok/cherry_process) | [`components/cherry_process`](components/cherry_process) |
| **`cherry_usbd`** | USB 2.0/3.0 设备协议栈、Virtual UDC 环回驱动、CDC-ACM、MSC BOT 与 HID 引擎 | 独立组件开发中 | [`components/cherry_usbd`](components/cherry_usbd) |
| **`cherry_kal`** | 通用内核抽象层 (OSAL)，将具体底层 RTOS 接口标准化为 Linux 语义 | 内核适配层（集成于本仓库） | [`components/cherry_kal`](components/cherry_kal) |

---

## 2. 整体集成架构

```
┌────────────────────────────────────────────────────────────────────────┐
│               用户空间应用程序 / ELF64 二进制 / BusyBox 工具集         │
└───────────────────────────────────┬────────────────────────────────────┘
                                    │ Linux System V ABI / cherry_syscall
┌───────────────────────────────────▼────────────────────────────────────┐
│                    CherryOS 现代化组件层 (独立组件仓库)                 │
│  cherry_process · cherry_vfs  · cherry_net   · cherry_nvme · cherry_fs│
│  cherry_ext4    · cherry_ahci · cherry_sdio  · cherry_pci  · ...       │
└───────────────────────────────────┬────────────────────────────────────┘
                                    │ cherry_kal (Linux 标准 OSAL 抽象接口)
┌───────────────────────────────────▼────────────────────────────────────┐
│               FreeRTOS SMP 内核与集成层 (本仓库核心职责)               │
│  - kal_freertos.c : 线程生命周期、互斥锁、信号量、自旋锁映射           │
│  - FreeRTOS V11.x : SMP 多核调度、时间片轮转、核心亲和性绑定           │
│  - portASM.S      : GUEST EL1 上下文保存恢复与 SGI 0 核间中断 Yield    │
└───────────────────────────────────┬────────────────────────────────────┘
                                    │
┌───────────────────────────────────▼────────────────────────────────────┐
│                  通用体系结构与硬件抽象层 (arch/ & bsp/)               │
│  - arch/ : MPIDR 核心编号、AArch64 屏障与内联原子指令 (LL/SC)          │
│  - bsp/  : PSCI CPU_ON 多核启动、GICv3 (ICC_SRE) 中断、PL011 串口驱动  │
└────────────────────────────────────────────────────────────────────────┘
```

---

## 3. 目录结构

```text
freertos-workspace/
├── SConstruct                  # SCons 顶层构建编排脚本（组件联动编译与 QEMU/测试目标）
├── Makefile                    # 快捷构建命令行封装
├── kernel/                     # FreeRTOS V11 SMP 官方内核源码及 AArch64 端口实现
│   ├── FreeRTOS-Kernel/        # 核心任务调度器 (tasks.c, queue.c, list.c 等)
│   └── port/                   # GCC/ARM_AARCH64_SRE 移植层 (port.c, portASM.S)
├── arch/                       # 体系结构抽象层 (原子操作、自旋锁、中断与屏障)
├── bsp/                        # 板级支持包 (QEMU virt 裸机启动 startup.S, GICv3, PL011)
├── components/                 # CherryOS 现代化操作系统组件库 (由独立组件仓库集成)
│   ├── cherry_kal/             # 内核抽象层 (FreeRTOS OSAL 适配)
│   ├── cherry_printk/          # 多核并发日志引擎
│   ├── cherry_timer/           # 高精度多核定时器
│   ├── cherry_vfs/             # 虚拟文件系统与设备框架
│   ├── cherry_shell/           # 动态 C 解释器 Shell
│   ├── cherry_pci/             # PCIe ECAM 与 GICv3 ITS
│   ├── cherry_sdio/            # SD/SDIO/eMMC 协议栈
│   ├── cherry_ext4/            # Ext4 文件系统与 JBD2
│   ├── cherry_fstools/         # 分区挂载管理与核心存储工具
│   ├── cherry_net/             # 多核多队列 TCP/IP 协议栈
│   ├── cherry_nvme/            # NVMe 固态存储驱动
│   ├── cherry_ahci/            # SATA AHCI 存储驱动
│   ├── cherry_usbd/            # USB 设备协议栈
│   └── cherry_process/         # 轻量级进程模型、Syscall 与 ELF 加载器
├── tests/                      # 自动化测试编排脚本 (accept_qemu_sched.py)
└── docs/                       # 阶段技术博客与演进架构设计文档
```

---

## 4. 快速上手

### 4.1 环境准备

确保系统安装了 AArch64 交叉编译器、QEMU 仿真器与 SCons：

```bash
# Ubuntu / Debian
sudo apt-get update
sudo apt-get install -y gcc-aarch64-linux-gnu g++-aarch64-linux-gnu qemu-system-arm python3 python3-pip
pip3 install scons
```

### 4.2 构建与运行

```bash
# 1. 默认编译 (生成 2 核 SMP Unikernel 镜像 build/bsp/freertos_smp.elf)
python3 -m SCons

# 2. 指定核心数编译 (例如 4 核心)
python3 -m SCons CORES=4

# 3. 交互式启动 QEMU 模拟仿真
python3 -m SCons run
# (使用 Ctrl+A, X 退出 QEMU)
```

### 4.3 自动化多核验收验证

项目包含一键式多核自动化验收套件，自动拉起 QEMU 并执行严格的组件与调度探针校验：

```bash
# 1. 运行标准双核调度与全组件验收测试
python3 -m SCons test

# 2. 运行高压力跨核同步压测 (Cross-core Stress)
python3 -m SCons SCHED_MODE=cross test
```

**验收通过标志输出**：
```text
cherry_process: [process-verify] spawn=pass wait=pass fd=pass pipe=pass syscall=pass applets=pass sh=pass smp_ps=pass elf_glibc=pass all=pass
freertos: [sched-verify] active=pass idle=pass affinity=pass coverage=pass progress=pass prio=pass rr=pass sem=pass queue=pass mutex=pass spin=pass cores=2 seen=0x3 missing=0x0
[SCHED-TEST] SUMMARY result=pass mode=full cores=2 markers=1
```

---

## 5. 阶段技术文档

项目保留了完整的嵌入式多核系统演进文档：

| 阶段文档 | 核心技术主题 |
| :--- | :--- |
| [Phase 1 技术博客](docs/phase1-AArch64裸机引导与UART输出.md) | **AArch64 裸机引导与 UART**：`startup.S`、链接脚本、EL2 降级、SCTLR 寄存器与 PL011 驱动 |
| [Phase 2 (GICv2) 博客](docs/phase2-异常向量表与GICv2中断.md) | **异常向量表与 GICv2**：ARM 16 入口异常模型、中断栈帧保存与通用定时器 SysTick |
| [Phase 2 (GICv3) 博客](docs/phase2-GICv3中断与通用定时器.md) | **GICv3 升级**：ICC_SRE 接口、Group 1 非安全中断与 Redistributor 配置 |
| [Phase 3 技术博客](docs/phase3-FreeRTOS集成.md) | **FreeRTOS 单核集成**：任务控制块 (TCB)、SVC 上下文切换与抢占式调度 |
| [Phase 4 技术博客](docs/phase4-FreeRTOS-SMP.md) | **FreeRTOS SMP 多核架构**：SGI 0 核间中断 Yield、PSCI 多核引导与 32 核调度压测 |
| [Unikernel 架构规范](docs/unikernel-architecture.md) | **Unikernel 规范**：极简内核引导模型与组件生态融合设计 |

---

## 6. 许可证

本项目遵循 [MIT 许可证](LICENSE)。FreeRTOS 原版内核代码遵循其官方 [MIT 许可证](https://github.com/FreeRTOS/FreeRTOS-Kernel/blob/main/LICENSE.md)。
