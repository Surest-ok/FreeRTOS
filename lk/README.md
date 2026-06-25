# LK QEMU Cortex-A53 4-Core SMP Port

## 项目概述

本项目是Little Kernel (LK)内核移植到QEMU ARMv8-A (Cortex-A53) 4核SMP平台的精简版本,专注于嵌入式系统核心功能。

### 主要特性

- ✅ **ARM64架构**: 只保留ARM64,删除其他所有架构
- ✅ **QEMU平台**: 针对QEMU virt machine优化
- ✅ **4核SMP**: 完整支持4个Cortex-A53核心的对称多处理
- ✅ **GICv3支持**: 完整的中断控制器和IPI(处理器间中断)支持
- ✅ **C++支持**: C++11编译器和运行时库
- ✅ **网络栈**: minip轻量级TCP/IP协议栈集成
- ✅ **精简驱动**: UART(PL011)、GICv3、ARM Generic Timer、PSCI
- ✅ **YAML构建**: 采用现代化的YAML构建框架(Luna BSP catboy)
- ✅ **MMU支持**: 完整的虚拟内存管理和4级页表
- ✅ **多线程演示**: 独立工作和同步协作两种SMP演示

### 最新更新 (2025-12-08)

**🎉 v1.2 - C++支持与网络栈集成!**

本次更新新增:

- ✅ **C++编译器支持**: 集成aarch64-none-elf-g++,支持C++11标准
- ✅ **minip网络栈**: 完整的TCP/IP协议栈(IPv4/ARP/ICMP/UDP/TCP/DHCP)
- ✅ **多线程同步演示**: 新增`smp_sync`命令展示semaphore/mutex/event
- ✅ **日志优化**: 移除频繁调试打印,保留关键系统日志
- ✅ **Shell交互**: Demo改为手动启动,支持`smp_demo`和`smp_sync`命令

---

## 系统架构

### 启动流程详解

#### 1. 主核 (CPU0) 启动序列

```
QEMU启动
  ↓
[EL2] arm64_reset (arch/arm64/start.S)
  ├─ 清零BSS段
  ├─ 设置初始栈指针
  └─ 调用 arm64_elX_to_el1 (EL2→EL1降级)
     ├─ 配置HCR_EL2 (启用AArch64)
     ├─ 设置CPTR_EL2 (允许FP/NEON)
     ├─ 配置计数器访问
     └─ ERET到EL1
       ↓
[EL1] _start (arch/arm64/start.S)
  ├─ 设置VBAR_EL1 (异常向量表)
  ├─ 启用NEON/FP
  ├─ 调用 arm64_mmu_init
  │   ├─ 创建4级页表 (L0/L1/L2/L3)
  │   ├─ 映射内核空间 (物理0x40000000 → 虚拟0xffff000000000000)
  │   ├─ 映射外设空间 (物理0x08000000 → 虚拟0xffff000008000000)
  │   └─ 启用MMU (SCTLR_EL1)
  └─ 跳转到虚拟地址
       ↓
lk_main (top/main.c)
  ├─ 调用早期初始化钩子
  ├─ 初始化堆 (PMM物理内存管理器)
  ├─ 初始化线程系统
  ├─ 创建bootstrap线程
  └─ 进入调度器
       ↓
bootstrap2 (top/main.c)
  ├─ platform_early_init
  │   ├─ UART初始化 (PL011)
  │   ├─ GICv3初始化 (CPU 0)
  │   │   ├─ 配置GICD (distributor)
  │   │   ├─ 唤醒GICR (redistributor)
  │   │   └─ 配置CPU interface
  │   └─ 定时器初始化
  ├─ platform_init
  │   ├─ 创建次级CPU的idle和bootstrap线程
  │   └─ 通过PSCI启动CPU 1-3
  └─ 初始化应用层
       ↓
应用运行 (app/lk_smp_demo.c)
```

#### 2. 次级核心 (CPU1-3) 启动序列

```
PSCI CPU_ON调用 (CPU0发起)
  ↓
[EL2] arm64_secondary_phys_entry (arch/arm64/start.S)
  ├─ 设置CPU ID (x0寄存器传入)
  ├─ 调用 arm64_elX_to_el1 (EL2→EL1降级)
  └─ ERET到EL1
       ↓
[EL1] arm64_secondary_entry (arch/arm64/mp.c)
  ├─ 初始化percpu结构
  ├─ 设置VBAR_EL1
  ├─ 启用MMU (页表由CPU0建立)
  ├─ arm64_early_init_percpu
  ├─ lk_secondary_cpu_entry_early
  │   └─ 设置当前线程为idle
  ├─ arm_gicv3_init_percpu ⭐ **关键!**
  │   ├─ 计算本CPU的GICR基地址
  │   ├─ 唤醒GICR
  │   ├─ 启用SGI 14-15 (IPI中断)
  │   └─ 配置CPU interface
  ├─ arm64_mp_init_percpu
  │   ├─ 注册IPI中断处理器
  │   └─ unmask IPI中断
  └─ lk_secondary_cpu_entry
      ├─ mp_set_curr_cpu_active(true)
      └─ 进入idle循环 + 调度器
```

### 内存布局

#### 物理内存 (DRAM)
```
0x40000000 ┌────────────────┐
           │  内核代码段    │
           │  (.text)       │
           ├────────────────┤
           │  只读数据      │
           │  (.rodata)     │
           ├────────────────┤
           │  数据段        │
           │  (.data)       │
           ├────────────────┤
           │  BSS段         │
           │  (.bss)        │
           ├────────────────┤
           │  堆空间        │
           │  (动态分配)    │
           ├────────────────┤
           │  栈空间        │
           │  (每个线程)    │
0x48000000 └────────────────┘
           总大小: 128MB
```

#### 虚拟内存映射

| 虚拟地址 | 物理地址 | 大小 | 用途 | 属性 |
|---------|---------|------|------|------|
| 0xffff000000000000 | 0x40000000 | 128MB | 内核代码+数据+堆 | Normal, Cacheable |
| 0xffff000008000000 | 0x08000000 | 1GB | 外设MMIO | Device-nGnRnE |

**页表结构** (4级转换):
- L0: 512GB per entry
- L1: 1GB per entry
- L2: 2MB per entry
- L3: 4KB per entry (页大小)

#### 外设地址映射

| 外设 | 物理基地址 | 虚拟基地址 | 功能 |
|-----|-----------|-----------|------|
| GICv3 Distributor | 0x08000000 | 0xffff000008000000 | 中断分发器 |
| GICv3 Redistributor | 0x080A0000 + cpu*0x20000 | 同左 | Per-CPU中断redistributor |
| PL011 UART | 0x09000000 | 0xffff000009000000 | 串口控制台 |
| ARM Generic Timer | System registers | - | 系统计时器 |

---

## SMP多核架构

### GICv3中断系统

#### 组件说明

1. **GICD (Distributor)**
   - 全局唯一,所有CPU共享
   - 管理SPI(共享外设中断,32-1019)
   - 地址: 0x08000000

2. **GICR (Redistributor)**
   - **每个CPU有独立的GICR实例!**
   - 管理PPI(私有外设中断,16-31)和SGI(软件生成中断,0-15)
   - CPU n的地址: 0x080A0000 + n * 0x20000
   - 包含两部分:
     - RD_base: Redistributor配置寄存器
     - SGI_base: RD_base + 0x10000 (SGI/PPI控制)

3. **CPU Interface**
   - 通过系统寄存器访问 (ICC_*_EL1)
   - 每个CPU独立的接口

#### IPI (处理器间中断) 机制

```
CPU 0                          CPU 1
  |                              |
  | thread_resume(worker1)       |
  |   pinned_cpu = 1             |
  |   ↓                          |
  | mp_reschedule(0x2)           |
  |   target &= active_cpus      |
  |   ↓                          |
  | arch_mp_send_ipi(0x2, 15)    |
  |   ↓                          |
  | arm_gic_sgi(15, 0x2)         |
  |   写ICC_SGI1R_EL1 ──────────→ GICv3
  |                              ↓
  |                        中断触发
  |                              ↓
  |                     platform_irq()
  |                              ↓
  |                   arm_ipi_reschedule_handler()
  |                              ↓
  |                   返回 INT_RESCHEDULE
  |                              ↓
  |                        调度器重新调度
  |                              ↓
  |                        worker1线程运行!
```

**SGI编号分配**:
- SGI 14: `MP_IPI_GENERIC` - 通用IPI
- SGI 15: `MP_IPI_RESCHEDULE` - 调度器reschedule IPI

### GICv3 Per-CPU初始化

GICv3中断控制器为每个CPU提供独立的redistributor(GICR),需要在每个CPU启动时单独初始化:

**初始化流程**:
1. 计算当前CPU的GICR基地址: `GICR_BASE + cpu_id * 0x20000`
2. 唤醒GICR redistributor
3. 启用SGI 14-15 (IPI中断)
4. 配置CPU interface寄存器

**关键函数**:
- `arm_gicv3_init()`: 主CPU初始化GICD和第一个GICR
- `arm_gicv3_init_percpu()`: 次级CPU初始化各自的GICR
- `arch_mp_send_ipi()`: 通过SGI发送处理器间中断

---

## 构建系统

### 前置条件

1. **工具链**: `aarch64-none-elf-gcc` (ARM GNU Toolchain)
2. **构建工具**: Luna BSP的catboy构建系统
3. **模拟器**: QEMU (qemu-system-aarch64)

### 构建配置文件

```
build.yaml                          # 根构建配置
  ├─ projects/lk_demo.yaml         # 项目定义
  └─ catboy/platforms/aarch64_lk.yaml  # 工具链配置

lk_qemu.ld                         # 链接脚本
```

### 构建步骤

```bash
cd D:\workspace\luna-workspace\other-os\lk-master

# 构建项目
catboy build lk_demo aarch64_lk

# 构建产物
ls build/lk_demo/aarch64_lk/lk_demo.elf
```

### 链接脚本关键配置 (lk_qemu.ld)

```ld
ENTRY(_start)
SECTIONS
{
    . = 0xffff000000000000 + 0x00100000;  /* 虚拟地址 */

    /* 代码段 */
    .text : AT(0x40000000 + 0x00100000) {
        KEEP(*(.text.boot))
        *(.text*)
    }

    /* 数据段 */
    .data : { *(.data*) }

    /* BSS段 */
    .bss : { *(.bss*) }
}
```

---

## 运行说明

### 基础运行

```bash
# 4核SMP启动 (GICv3)
qemu-system-aarch64 \
    -machine virt,gic-version=3,virtualization=on \
    -cpu cortex-a53 \
    -smp 4 \
    -m 128M \
    -nographic \
    -kernel build/lk_demo/aarch64_lk/lk_demo.elf \
    -serial mon:stdio
```

### 调试模式

```bash
# 启动GDB服务器
qemu-system-aarch64 \
    -machine virt,gic-version=3 \
    -cpu cortex-a53 \
    -smp 4 \
    -m 128M \
    -nographic \
    -kernel build/lk_demo/aarch64_lk/lk_demo.elf \
    -serial mon:stdio \
    -s -S

# 另一终端连接
aarch64-none-elf-gdb build/lk_demo/aarch64_lk/lk_demo.elf
(gdb) target remote :1234
(gdb) break lk_main
(gdb) continue
```

### Shell交互

系统启动后进入交互式Shell,可以手动运行演示:

```
LK starting...
GICv3: Initializing GICv3 for QEMU virt
...
ARM64: secondary cpu 3 started, mpidr 0x80000003
SMP: Requested 3 secondary CPUs to start
starting app shell
] ← Shell提示符
```

**可用命令**:

```bash
] help                  # 查看所有命令
] smp_demo             # 运行独立工作模式SMP演示
] smp_sync             # 运行同步协作模式SMP演示 (新增)
```

**独立工作模式** (`smp_demo`):
```
] smp_demo
========================================
LK SMP Test - Independent Workers
4 CPUs configured
========================================

Created and started 'worker0' pinned to CPU 0
Created and started 'worker1' pinned to CPU 1
Created and started 'worker2' pinned to CPU 2
Created and started 'worker3' pinned to CPU 3

[Core 0] Worker thread started on CPU 0
[Core 1] Worker thread started on CPU 1
[Core 2] Worker thread started on CPU 2
[Core 3] Worker thread started on CPU 3
[Core 0] Iteration 0 running on CPU 0
...
```

**同步协作模式** (`smp_sync`):
```
] smp_sync
========================================
LK SMP Test - Synchronized Workers
4 CPUs configured
========================================

Creating synchronized worker threads...
Creating coordinator thread...
Coordinator started - will run 5 rounds

[Coordinator] Started

=== Round 1 ===
[Core 0] Processing task on CPU 0
[Core 1] Processing task on CPU 1
[Core 2] Processing task on CPU 2
[Core 3] Processing task on CPU 3
--- Round 1 Statistics ---
Shared counter: 20000000
Core 0: 1 tasks completed
Core 1: 1 tasks completed
Core 2: 1 tasks completed
Core 3: 1 tasks completed
...
```

---

## 组件说明

### 核心库组件 (lib/)

| 组件 | 路径 | 功能 | 使用情况 |
|-----|------|------|---------|
| **console** | lib/console | 命令行接口和debug命令 | ✅ 使用中 |
| **heap** | lib/heap | 动态内存分配 (malloc/free) | ✅ 使用中 |
| **libc** | lib/libc | C标准库子集 | ✅ 使用中 |
| **cbuf** | lib/cbuf | 循环缓冲区 | ✅ UART使用 |
| **iovec** | lib/iovec | 分散/聚集I/O向量 | ✅ minip使用 |
| **pool** | lib/pool | 对象池内存管理 | ✅ minip使用 |
| **libcpp** | lib/libcpp | C++运行时支持 | ✅ minip DHCP使用 |
| **minip** | lib/minip | 轻量级TCP/IP栈 | ✅ 已集成 |
| **bio** | lib/bio | 块IO抽象层 | ⚠️ 未使用 |
| **fs** | lib/fs | 文件系统框架 | ❌ 已删除 |
| **tftp** | lib/tftp | TFTP客户端 | ❌ 未移植 |

### 应用组件 (app/)

| 应用 | 路径 | 功能 | 状态 |
|-----|------|------|------|
| **lk_smp_demo** | app/lk_smp_demo.c | SMP多核演示 | ✅ 工作正常 |
| **shell** | app/shell | 交互式命令行 | ✅ 可用 |
| **tests** | app/tests | 各种单元测试 | ✅ 部分可用 |
| **inetsrv** | app/inetsrv | 网络服务demo | ⚠️ 待集成 |
| **lkboot** | app/lkboot | 网络引导加载器 | ⚠️ 待集成 |

### 驱动框架 (dev/)

当前已实现的驱动:

```
dev/
├── interrupt/arm_gicv3/     # GICv3中断控制器 ⭐ 完整实现
│   ├── arm_gicv3.c          # 初始化、中断处理、SGI发送
│   └── include/dev/interrupt/arm_gicv3.h
├── timer/arm_generic/       # ARM Generic Timer
│   └── arm_generic_timer.c
├── uart/pl011/              # PL011 UART
│   └── pl011.c
└── power/psci/              # PSCI电源管理
    └── psci.c               # CPU启动、关机、重启
```

---

## minip网络栈

### 架构概述

minip是LK内核的轻量级TCP/IP协议栈实现,专为嵌入式系统设计。当前项目已完成minip的集成和适配。

**核心特性**:
- IPv4协议栈(不支持IPv6)
- ARP地址解析协议
- ICMP协议(支持ping)
- UDP socket API
- TCP socket API(基础实现)
- DHCP客户端自动配置
- 独立网络定时器线程

### 源码组织

```
lib/minip/
├── minip.c              # 核心协议处理: IP/ARP/ICMP
├── tcp.c                # TCP协议实现 (连接管理、流控、重传)
├── udp.c                # UDP协议实现 (无连接数据报)
├── dhcp.cpp             # DHCP客户端 (C++实现)
├── arp.c                # ARP地址解析缓存
├── chksum.c             # IP校验和计算
├── pktbuf.c             # 网络数据包缓冲区管理
├── net_timer.c          # 网络定时器线程
├── lk_console.c         # 网络调试命令
├── minip-internal.h     # 内部数据结构定义
└── include/lib/minip.h  # 对外公共API

依赖库:
├── lib/iovec/           # 分散/聚集I/O向量
├── lib/pool/            # 对象池内存管理器
├── lib/cbuf/            # 循环缓冲区 (TCP使用)
└── lib/libcpp/          # C++运行时 (DHCP使用)
```

### 工作原理

#### 数据包处理流程

```
网卡驱动接收
    ↓
minip_rx_driver_callback(pktbuf_t *p)
    ├─ 解析以太网帧头
    ├─ ARP包 → arp_input()
    │   └─ 更新ARP缓存表
    ├─ IP包 → minip_ipv4_input()
    │   ├─ ICMP → icmp_input()
    │   │   └─ echo request → 自动回复echo reply
    │   ├─ UDP → udp_input()
    │   │   └─ 查找监听socket → 调用用户回调
    │   └─ TCP → tcp_input()
    │       └─ 状态机处理 → 更新连接状态
    └─ 其他 → 丢弃
```

#### 网络定时器机制

minip使用独立线程`net_timer_thread`处理网络超时和周期性任务:

```
net_timer_thread (lib/minip/net_timer.c)
    ↓
主循环 (每10ms检查一次)
    ├─ 遍历所有注册的定时器
    ├─ 检查是否到期 (current_time() >= scheduled_time)
    ├─ 到期 → 调用回调函数
    │   ├─ TCP重传定时器
    │   ├─ TCP保活定时器
    │   ├─ ARP缓存老化
    │   └─ DHCP续约
    └─ 休眠直到下一个定时器到期
```

**关键数据结构**:
```c
typedef struct net_timer {
    struct list_node node;         // 链表节点
    lk_time_t scheduled_time;      // 计划触发时间
    lk_time_t periodic_time;       // 周期(0=单次)
    net_timer_callback_t callback; // 回调函数
    void *arg;                     // 回调参数
} net_timer_t;
```

#### TCP状态机

TCP连接使用标准的状态机实现:

```
CLOSED
  ↓ tcp_open_listen()
LISTEN ─────────────────┐
  ↓ SYN收到              │
SYN_RECEIVED            │
  ↓ ACK收到              │
ESTABLISHED ←──────────┘ tcp_connect()
  ↓                         ↓
  │                      SYN_SENT
  │                         ↓ SYN-ACK收到
  │                      ESTABLISHED
  ↓ tcp_close()             ↓ tcp_close()
FIN_WAIT_1               CLOSE_WAIT
  ↓ ACK收到                 ↓ tcp_close()
FIN_WAIT_2               LAST_ACK
  ↓ FIN收到                 ↓ ACK收到
TIME_WAIT                CLOSED
  ↓ 2MSL超时
CLOSED
```

**TCP可靠性保证**:
- 序列号/确认号机制
- 滑动窗口流量控制
- 超时重传 (RTO自适应计算)
- 乱序重组
- 校验和验证

#### 构建系统集成

**1. build.yaml配置**

```yaml
targets:
  lk_demo:
    build:
      includes:
        - lib/minip/include      # minip公共API
        - lib/iovec/include      # I/O向量
        - lib/pool/include       # 对象池
        - lib/libcpp/include     # C++支持

      defines:
        - WITH_LIB_MINIP=1       # 启用minip

      sources:
        c:
          # minip网络栈 (C语言部分)
          - lib/minip/arp.c
          - lib/minip/chksum.c
          - lib/minip/lk_console.c
          - lib/minip/minip.c
          - lib/minip/net_timer.c
          - lib/minip/pktbuf.c
          - lib/minip/tcp.c
          - lib/minip/udp.c

          # 依赖库
          - lib/iovec/iovec.c
          - lib/pool/pool.c

        cpp:
          # C++支持库
          - lib/libcpp/new.cpp
          - lib/libcpp/pure_virtual.cpp

          # minip DHCP客户端 (C++)
          - lib/minip/dhcp.cpp
```

**2. 工具链配置**

`catboy/platforms/aarch64_lk.yaml`中添加C++编译器和原子操作标志:

```yaml
aarch64_lk:
  toolchain:
    c-compiler:
      flags: >-
        -O2 -march=armv8-a -std=gnu11
        -mno-outline-atomics    # 禁用outline atomics(避免链接错误)

    cpp-compiler:              # C++编译器支持
      source_type: cpp
      path: aarch64-none-elf-g++
      flags: >-
        -O2 -march=armv8-a -std=c++11
        -fno-exceptions -fno-rtti
        -mno-outline-atomics
```

**关键编译选项说明**:
- `-mno-outline-atomics`: 禁用GCC 14的outline atomics特性,内联原子操作避免依赖`__aarch64_ldadd4_relax`等符号
- `-fno-exceptions -fno-rtti`: 禁用C++异常和RTTI,减小代码体积

### 网络驱动接口

minip与网卡驱动通过回调函数接口解耦:

```c
// 驱动 → minip (接收路径)
void minip_rx_driver_callback(pktbuf_t *p);

// minip → 驱动 (发送路径)
typedef int (*tx_func_t)(void *arg, pktbuf_t *p);

// 初始化minip,注册发送函数和MAC地址
void minip_set_eth(tx_func_t tx_handler, void *tx_arg, const uint8_t *macaddr);
```

**工作流程**:
1. 网卡驱动初始化时调用`minip_set_eth()`注册发送回调
2. 网卡收到数据包 → 分配pktbuf → 调用`minip_rx_driver_callback()`
3. minip发送数据包 → 调用注册的`tx_func_t` → 驱动发送到硬件

#### VirtIO-Net驱动集成 (待实现)

QEMU virt平台需要VirtIO-Net驱动。驱动需要实现:

**1. VirtIO设备探测和初始化**
```c
// 探测MMIO设备
// 地址范围: 0x0a000000 - 0x0a003fff (QEMU virt)
void virtio_net_init(void) {
    // 读取VirtIO设备ID (应为1 = network)
    // 协商特性 (feature negotiation)
    // 初始化virtqueue (RX queue, TX queue)
    // 注册中断处理器
}
```

**2. 数据包发送**
```c
int virtio_net_send(void *arg, pktbuf_t *p) {
    // 填充virtqueue descriptor
    // 将pktbuf数据写入DMA缓冲区
    // 通知设备 (写QUEUE_NOTIFY寄存器)
    // 等待发送完成
}
```

**3. 数据包接收**
```c
void virtio_net_rx_handler(void) {
    // 从RX virtqueue读取descriptor
    // 分配pktbuf并拷贝数据
    // 调用minip_rx_driver_callback(pkt)
    // 回收descriptor供下次接收
}
```

### API使用示例

#### UDP监听服务器

```c
#include <lib/minip.h>

// UDP数据回调函数
void udp_recv_callback(void *data, size_t len,
                       uint32_t srcaddr, uint16_t srcport, void *arg) {
    printf("UDP recv from %u.%u.%u.%u:%u, len=%zu\n",
           IPV4_SPLIT(srcaddr), srcport, len);

    // 处理接收到的数据
    // ...
}

// 监听UDP端口8000
int main(void) {
    udp_listen(8000, udp_recv_callback, NULL);
    printf("UDP server listening on port 8000\n");
    return 0;
}
```

#### UDP客户端发送

```c
#include <lib/minip.h>

int send_udp_packet(void) {
    udp_socket_t *sock;
    uint32_t dest_ip = IPV4(192, 168, 1, 100);
    uint16_t dest_port = 9000;

    // 打开UDP socket
    status_t ret = udp_open(dest_ip, 0, dest_port, &sock);
    if (ret < 0) {
        printf("Failed to open UDP socket\n");
        return -1;
    }

    // 发送数据
    char msg[] = "Hello from LK!";
    ret = udp_send(msg, sizeof(msg), sock);

    // 关闭socket
    udp_close(sock);

    return ret;
}
```

#### TCP服务器

```c
#include <lib/minip.h>
#include <kernel/thread.h>

static int tcp_server_thread(void *arg) {
    tcp_socket_t *listen_socket;

    // 监听端口80
    if (tcp_open_listen(&listen_socket, 80) < 0) {
        printf("Failed to listen on port 80\n");
        return -1;
    }

    printf("TCP server listening on port 80\n");

    for (;;) {
        tcp_socket_t *accept_socket;

        // 接受连接 (阻塞等待)
        if (tcp_accept(listen_socket, &accept_socket) < 0) {
            continue;
        }

        printf("Client connected\n");

        // 读取客户端请求
        char buf[256];
        ssize_t len = tcp_read(accept_socket, buf, sizeof(buf) - 1);
        if (len > 0) {
            buf[len] = '\0';
            printf("Received: %s\n", buf);

            // 发送HTTP响应
            const char *response =
                "HTTP/1.0 200 OK\r\n"
                "Content-Type: text/plain\r\n"
                "\r\n"
                "Hello from LK minip!\n";
            tcp_write(accept_socket, response, strlen(response));
        }

        // 关闭连接
        tcp_close(accept_socket);
    }

    return 0;
}

void start_tcp_server(void) {
    thread_t *t = thread_create("tcp_server", tcp_server_thread,
                                 NULL, DEFAULT_PRIORITY, DEFAULT_STACK_SIZE);
    thread_resume(t);
}
```

#### TCP客户端

```c
#include <lib/minip.h>

int tcp_client_example(void) {
    tcp_socket_t *sock;
    uint32_t server_ip = IPV4(192, 168, 1, 100);
    uint16_t server_port = 8080;

    // 连接到服务器
    status_t ret = tcp_connect(&sock, server_ip, server_port);
    if (ret < 0) {
        printf("Failed to connect to server\n");
        return -1;
    }

    printf("Connected to %u.%u.%u.%u:%u\n",
           IPV4_SPLIT(server_ip), server_port);

    // 发送数据
    const char *msg = "GET / HTTP/1.0\r\n\r\n";
    tcp_write(sock, msg, strlen(msg));

    // 接收响应
    char buf[512];
    ssize_t len = tcp_read(sock, buf, sizeof(buf) - 1);
    if (len > 0) {
        buf[len] = '\0';
        printf("Response:\n%s\n", buf);
    }

    // 关闭连接
    tcp_close(sock);

    return 0;
}
```

### 当前集成状态

**✅ 已完成**:
- minip源码集成到`lib/minip/`
- YAML构建系统配置
- C++编译器支持(DHCP使用)
- 网络定时器线程自动启动
- 依赖库集成(iovec, pool, libcpp)
- 编译器原子操作标志优化

**⚠️ 待完成**:
- VirtIO-Net网卡驱动实现
- 平台初始化中调用minip初始化
- DHCP/静态IP配置
- 网络应用示例(inetsrv)

**构建验证**:
```bash
$ catboy build lk_demo
Build completed successfully in 2.82s

$ ls -lh build/lk_demo/aarch64_lk/lk_demo.elf
-rw-r--r-- 1 user staff 1012K  lk_demo.elf
```

**运行测试**:
```bash
$ qemu-system-aarch64 -machine virt,gic-version=3 \
    -cpu cortex-a53 -smp 4 -m 128M -nographic \
    -kernel build/lk_demo/aarch64_lk/lk_demo.elf

LK starting...
[...]
initializing heap
calling constructors
creating bootstrap completion thread
[SCHED] get_top_thread: cpu=0 bitmap=0x10000
[SCHED]   checking thread=net timer pinned=-1    ← minip定时器线程启动
[SCHED]   -> selected thread=net timer for cpu=0
[...]
```

---

## 移植流程总结

如果要移植LK到新的ARM64平台,按以下步骤进行:

### 1. 硬件适配层

**platform/[your-board]/** 目录包含平台相关代码:
- `platform.c`: 平台初始化和配置
- `platform/[board].h`: 硬件地址定义
- `drivers/`: 平台特定驱动

**核心接口**:
- `platform_early_init()`: 早期初始化(UART、中断控制器)
- `platform_init()`: 标准初始化(启动次级CPU、外设)
- `platform_halt()`: 系统关机/重启

### 2. 内存配置

**MMU映射表** (arch/arm64/mmu.c):

定义物理到虚拟地址的映射关系:
- 内核空间: 物理DRAM映射到高地址虚拟空间
- 外设空间: MMIO设备映射,标记为Device内存
- 内存属性: Normal(可缓存) 或 Device(不可缓存)

映射表示例: DRAM物理0x40000000 → 虚拟0xffff000000000000

### 3. 中断控制器

**必须实现的接口**:
- `platform_init_interrupts()`: 初始化中断系统
- `mask_interrupt()` / `unmask_interrupt()`: 中断屏蔽控制
- `register_int_handler()`: 注册中断处理函数

**SMP支持**:
- `arch_mp_send_ipi()`: 发送处理器间中断(IPI)
- Per-CPU中断控制器初始化

**参考实现**: `dev/interrupt/arm_gicv3/`

### 4. 定时器

实现定时器驱动提供系统时钟:
- 初始化定时器并配置频率
- 注册周期性中断处理
- 提供当前时间查询接口

ARM平台通常使用ARM Generic Timer

### 5. 串口

实现基础UART接口用于控制台:
- `uart_putc()`: 输出字符
- `uart_getc()`: 接收字符(可选阻塞)

常见驱动: PL011, 16550

### 6. SMP支持

**次级CPU启动**:
- 使用PSCI接口启动其他CPU核心
- 提供次级CPU入口地址(物理地址)
- 传递CPU ID参数

**Per-CPU初始化**:
- 设置异常向量表(VBAR_EL1)
- 启用MMU(使用主CPU创建的页表)
- 初始化中断控制器Per-CPU部分
- 进入内核调度器

### 7. 链接脚本

配置内核加载地址和段布局:
- 定义虚拟地址和物理地址映射
- 指定代码段、数据段、BSS段位置
- 设置入口点符号(_start)
- 配置栈和堆空间

关键配置: 虚拟地址、物理加载地址(AT)、段对齐

---

## 驱动移植参考

### 从其他平台移植驱动的通用步骤

1. **复制驱动源码**
   ```bash
   cp -r original_platform/dev/[driver] new_platform/dev/
   ```

2. **修改地址定义**
   - 更新MMIO基地址
   - 调整中断号

3. **适配初始化顺序**
   - 在`platform_early_init()`或`platform_init()`中调用驱动初始化

4. **添加到构建系统**
   ```yaml
   components:
     - dev/[driver]
   ```

### 常见驱动类型

| 驱动类型 | 接口标准 | 示例 |
|---------|---------|------|
| UART | `dev/class/uart` | PL011, 16550 |
| 块设备 | `lib/bio` | eMMC, SD, VirtIO-blk |
| 网络 | `dev/class/netif` | VirtIO-net, SMSC91x |
| 显示 | `lib/gfx` | HDLCD, SimpleFB |
| 定时器 | Platform-specific | ARM Generic Timer, SP804 |

---

## 故障排查

### SMP相关

**次级CPU启动**:
- 检查PSCI接口返回值
- 验证entry point物理地址正确
- 确认MMU页表完整映射

**线程调度**:
- 验证IPI中断正常工作
- 检查Per-CPU初始化完成
- 确认CPU标记为active状态

**系统挂起**:
- 检查中断控制器初始化顺序
- 验证异常向量表设置
- 确认调度器正常运行

### 网络相关

**初始化问题**:
- 确认网络设备驱动加载
- 检查DMA内存分配
- 验证中断注册成功

**网络不通**:
- 检查QEMU网络配置
- 验证MAC地址设置
- 测试基础连通性(ping)

---

## 性能优化

### Cache优化
- 启用L1/L2 cache
- 正确配置内存属性(Cacheable vs Device)
- 数据结构cache line对齐

### 中断优化
- 配置中断亲和性(IRQ affinity)
- 高负载场景考虑polling模式

### 调度器优化
- 实时任务启用抢占
- 合理设置线程CPU亲和性
- 优化锁粒度减少竞争

---

## 扩展方向

### 存储支持
- ext2/FAT32文件系统
- VirtIO-blk块设备驱动
- Flash/eMMC驱动

### 网络功能
- VirtIO-Net网卡驱动
- 网络应用示例

---

## 许可证

本项目遵循LK的MIT许可证。

---

## 维护日志

| 日期 | 版本 | 修改内容 |
|-----|------|---------|
| 2025-12-05 | v1.0 | ARM64 QEMU平台初始移植完成 |
| 2025-12-08 | v1.1 | GICv3 IPI支持,SMP 4核完全工作 |
| 2025-12-08 | v1.2 | minip网络栈集成,YAML构建系统,C++编译器支持 |

---

**项目状态**: ✅ 核心功能完成 + 网络栈集成
**目标平台**: QEMU virt machine (ARMv8-A, Cortex-A53, 4 cores)
**构建框架**: YAML-based (Luna BSP catboy)
**网络支持**: ✅ minip TCP/IP栈已集成 (待VirtIO驱动)
