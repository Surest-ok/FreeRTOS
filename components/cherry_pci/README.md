# CherryPCI - Linux 风格现代化 PCIe 子系统与 ARM GICv3 ITS MSI 支持

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![PCIe: ECAM](https://img.shields.io/badge/PCIe-ECAM%20Host%20Bridge-green.svg)]()
[![Interrupt: GICv3 ITS](https://img.shields.io/badge/Interrupt-ARM%20GICv3%20ITS%20MSI-blue.svg)]()
[![Driver Model: Linux Style](https://img.shields.io/badge/Driver-Linux%20Style%20pci__driver-orange.svg)]()

`CherryPCI` 是为嵌入式与实时多核系统（FreeRTOS SMP、RT-Thread 等）量身打造的**现代化、模块化、标准 Linux 接口风格、强解耦**的 PCI Express (PCIe) 核心子系统。它实现了完整的 PCIe ECAM 根复合体扫描、BAR 资源分区动态分配（32-bit MMIO、64-bit MMIO、Prefetchable 空间与 I/O 空间）、能力链表遍历（标准 Capability 与 PCIe Extended Capability）、ARM GICv3 ITS (Interrupt Translation Service) 硬件消息中断 (MSI/MSI-X) 与 INTx 向量分配，并提供与标准 Linux 内核高度一致的 `pci_driver` 注册、设备匹配与绑定初始化模型。

---

## 🚀 核心架构与功能特性

### 1. PCIe ECAM 主桥驱动与设备枚举
- **ECAM (Enhanced Configuration Access Mechanism)**：支持标准 MMIO 方式访问 PCIe 扩展配置空间（4KB/设备）；
- **自适应基地址探测**：自动识别 QEMU virt AArch64 的高位 ECAM 空间（`0x4010000000`, 256MB）与低位空间（`0x3F000000`, 16MB）；
- **层级总线遍历**：支持枚举 Bus 0..255、Device 0..31、Function 0..7（MFD 多功能设备自动探测）；
- **设备抽象结构 (`struct pci_dev`)**：记录 BDF、Vendor/Device ID、Class Code、Revision、Subsystem IDs，并维护 BAR 资源、Capabilities 偏移与驱动绑定上下文。

### 2. BAR 资源动态分区分配（32 / 64 / Prefetchable 空间）
- **自动 Sizing 探测**：写入 `0xFFFFFFFF` 探测 BAR 需求大小，自动判别 I/O、32 位内存、64 位内存及可预取（Prefetchable / "perf"）属性。
- **跨寄存器 64 位 BAR 完整支持**：识别 `PCI_BASE_ADDRESS_MEM_TYPE_64`，自动探测并占用连续两个 32 位配置寄存器，计算 64 位掩码并分配 64 位物理基地址写入低/高 32 位寄存器。
- **独立划分的地址空间资源池**：
  - **32 位非预取 MMIO 池**（`PCIE_MMIO32_NONPREF_BASE`: `0x10000000`, 256MB）
  - **32 位可预取 MMIO 池**（`PCIE_MMIO32_PREF_BASE`: `0x20000000`, 256MB）
  - **64 位非预取 MMIO 池**（`PCIE_MMIO64_NONPREF_BASE`: `0x8000000000`, 2GB）
  - **64 位可预取 MMIO 池**（`PCIE_MMIO64_PREF_BASE`: `0x8080000000`, 2GB）
  - **传统 I/O 端口池**（`PCIE_IO_BASE`: `0x3EFF0000`, 64KB）
- **自然对齐与越界保护**：严格按 BAR 请求尺寸自然对齐（最小 4KB 对齐），超出物理窗口安全拦截。

### 3. Capability 能力链表遍历与访问（cap 访问）
- **标准 PCI Capability 链表遍历 (`0x34` .. `0xFF`)**：
  - `pci_find_capability(dev, cap_id)`：通过 `PCI_STATUS_CAP_LIST` 与 `PCI_CAPABILITY_LIST` 指针遍历标准能力链表，自动跳过保留项并具备防环形死循环保护。
  - 自动识别并缓存常用能力偏移：`pm_cap`（电源管理 0x01）、`msi_cap`（MSI 0x05）、`msix_cap`（MSI-X 0x11）、`pcie_cap`（PCI Express 0x10）。
- **PCIe 扩展能力遍历 (`0x100` .. `0xFFF`)**：
  - `pci_find_ext_capability(dev, ext_cap_id)`：在 4KB 扩展配置空间中遍历 PCIe Extended Capabilities 链表。
  - 自动缓存 `aer_cap`（高级错误报告 AER 0x0001）、虚拟通道（VC）、SR-IOV 等扩展能力。
- **配置空间能力读写助手**：
  - `pci_read_cap_dword` / `pci_write_cap_dword`：按能力 ID 偏置直接读写能力寄存器。

### 4. Linux 风格驱动模型 (`struct pci_driver`) 与设备初始化
- **驱动注册与解耦**：
  - `pci_register_driver(driver)`：注册驱动并自动扫描总线上未绑定的设备，通过 `id_table` 执行设备匹配；
  - `pci_match_device(driver, dev)`：支持按 Vendor ID、Device ID、Subsystem IDs 及 Class Code / Mask 规则匹配；
  - 绑定成功时记录 `dev->driver = driver`，调用 `driver->probe(dev, matched_id)`，支持 `pci_set_drvdata` 与 `pci_get_drvdata`；
  - `pci_unregister_driver(driver)`：触发所有已绑定设备的 `driver->remove(dev)` 并解绑。
- **设备控制与初始化**：
  - `pci_enable_device(dev)`：配置 Command 寄存器，激活设备 Memory 与 I/O 响应（`PCI_COMMAND_IO | PCI_COMMAND_MEMORY`）；
  - `pci_set_master(dev)`：开启 PCI 总线主控（`PCI_COMMAND_MASTER`）；
  - `pci_iomap(dev, bar, maxlen)`：获取 BAR 映射物理基地址。

### 5. 中断分配管理（GICv3 ITS MSI / MSI-X 与 INTx）
- **ARM GICv3 ITS 消息中断引擎**：驱动 ITS 硬件控制器（基地址 `0x08080000`），支持 `MAPD`、`MAPC`、`MAPTI` 命令生成，动态将设备 MSI 映射到 LPI 物理中断向量；
- **标准中断分配接口**：
  - `pci_alloc_irq_vectors(dev, min, max, flags)`：按偏好类型（`PCI_IRQ_MSI`, `PCI_IRQ_LEGACY`）分配中断向量；
  - `pci_enable_msi(dev, handler, arg)`：自动配置设备 MSI 寄存器（写入 GITS_TRANSLATER 与 Event ID），失败时透明降级为 INTx；
  - `pci_assign_irq(dev)`：按设备 Pin 与插槽计算 QEMU virt 平台 SPI 中断线（SPI 35..38），回写 `PCI_INTERRUPT_LINE`。

### 6. 模块严格解耦设计
- `cherry_pci` 仅依赖 `bsp/include/platform.h`（硬件窗口）与 `cherry_kal`，对外暴露通用 PCI 抽象接口；
- 绝不硬编码任何具体业务驱动（如 SDIO、网卡驱动或文件系统），业务驱动以独立的 `struct pci_driver` 插件形式注册，保持纯粹的树状单向依赖。

---

## 📂 目录结构

```text
components/cherry_pci/
├── SConscript               # 静态库独立编译规则
├── README.md                # 本技术规范文档
├── include/
│   ├── cherry_pci.h         # 核心公共 API、Linux 兼容数据结构与内联助手
│   ├── cherry_its.h         # ARM GICv3 ITS 寄存器、命令与 LPI 管理接口
│   ├── pci_regs.h           # PCIe 配置空间标准寄存器、能力宏与 IORESOURCE 标志
│   └── pci_ids.h            # 常用 PCIe Vendor 与 Device ID 宏
├── src/
│   ├── cherry_pci.c         # PCIe 子系统初始化统一入口
│   ├── pci_ecam.c           # ECAM 根复合体读写与基地址自适应探测
│   ├── pci_bus.c            # PCIe 总线扫描与设备发现
│   ├── pci_res.c            # 32/64位与Prefetchable BAR探测、资源分配与设备使能
│   ├── pci_cap.c            # 标准能力 (0x34) 与 PCIe 扩展能力 (0x100) 链表遍历
│   ├── pci_driver.c         # Linux 风格驱动注册、ID匹配、Probe绑定与注销
│   ├── pci_its.c            # ARM GICv3 ITS 驱动（命令队列与 LPI 配置）
│   ├── pci_msi.c            # MSI/MSI-X 向量绑定、INTx 分配与中断向量管理
│   └── pci_cmd.c            # CherryShell 调试诊断命令 (pci_ls, pci_res, pci_cap, its_info)
└── tests/
    ├── pci_test.h           # 自动化测试接口声明
    └── pci_test.c           # 全功能验收测试用例与探针 (8 大维度全面覆盖)
```

---

## 🛠️ C API 参考

### 1. 驱动注册示例
```c
#include "cherry_pci.h"

static int my_pci_probe(struct pci_dev *dev, const struct pci_device_id *id)
{
    /* 使能设备 MMIO 响应与总线主控 */
    pci_enable_device(dev);
    pci_set_master(dev);

    /* 获取 BAR0 物理地址与长度 */
    uint64_t bar0_base = pci_resource_start(dev, 0);
    uint64_t bar0_len  = pci_resource_len(dev, 0);

    /* 检查能力 */
    int msi_cap = pci_find_capability(dev, PCI_CAP_ID_MSI);

    /* 申请 MSI 中断 (自动回退 INTx) */
    pci_enable_msi(dev, my_interrupt_handler, dev);

    pci_set_drvdata(dev, my_priv_data);
    return 0;
}

static void my_pci_remove(struct pci_dev *dev)
{
    pci_disable_device(dev);
}

static const struct pci_device_id my_pci_tbl[] = {
    { 0x1B36, 0x0007, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0 }
};

static struct pci_driver my_pci_driver = {
    .name = "my-pci-driver",
    .id_table = my_pci_tbl,
    .probe = my_pci_probe,
    .remove = my_pci_remove,
};

/* 注册驱动 */
pci_register_driver(&my_pci_driver);
```

### 2. 诊断命令
在串口终端中支持以下命令：
- `pci_ls`：查看总线枚举设备列表、Vendor:Device ID、绑定驱动与 IRQ 模式；
- `pci_res`：查看各个设备分配的 BAR 空间（32/64/pref/io 类型、基地址与容量）；
- `pci_cap`：查看设备所有 Capability 偏移量（PM, MSI, MSI-X, PCIe, AER）；
- `its_info`：查看 ARM GICv3 ITS 硬件状态与分配的 LPI 向量。

---

## 🧪 自动化测试与验收标准

执行系统自动化验收测试：
```bash
python3 -m SCons test
```
启动时自动化验证探针输出：
```text
[    0.194776] cherry_pci: [pci-verify] ecam=pass bus_scan=pass dev_found=pass cap_access=pass bar_alloc=pass driver_bind=pass irq_alloc=pass its_msi=pass all=pass
```
各项验证指标涵盖：
- `ecam=pass`：ECAM 根复合体正确访问 4KB 扩展配置空间；
- `bus_scan=pass`：总线扫描完成全总线遍历且未溢出设备表；
- `dev_found=pass`：正确发现并枚举所有总线设备；
- `cap_access=pass`：标准 Capability 链表与扩展能力遍历准确，正确读写能力寄存器；
- `bar_alloc=pass`：32-bit MMIO、32-bit 可预取、64-bit MMIO、64-bit 可预取（"perf"）以及 I/O 窗口独立分区分配且自然对齐；
- `driver_bind=pass`：驱动注册、`id_table` 自动匹配、`probe` 绑定与 `remove` 注销全生命周期正确；
- `irq_alloc=pass`：INTx SPI 线路计算与分配生效；
- `its_msi=pass`：GICv3 ITS 控制器硬件就绪，LPI 消息中断分发通路健全；
- `all=pass`：子系统整体状态 100% 通过。
