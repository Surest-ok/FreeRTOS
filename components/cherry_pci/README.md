# CherryPCI - Linux 风格现代化 PCIe 子系统与 ARM GICv3 ITS MSI 支持

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![PCIe: ECAM](https://img.shields.io/badge/PCIe-ECAM%20Host%20Bridge-green.svg)]()
[![Interrupt: GICv3 ITS](https://img.shields.io/badge/Interrupt-ARM%20GICv3%20ITS%20MSI-blue.svg)]()
[![Driver Model: Linux Style](https://img.shields.io/badge/Driver-Linux%20Style%20pci__driver-orange.svg)]()

`CherryPCI` 是为嵌入式与实时多核系统（FreeRTOS SMP、RT-Thread 等）量身打造的**现代化、模块化、标准 Linux 接口风格**的 PCI Express (PCIe) 核心子系统。它实现了完整的 PCIe ECAM 根复合体扫描、BAR 资源动态分配、ARM GICv3 ITS (Interrupt Translation Service) 硬件消息中断 (MSI/MSI-X) 向量管理，并提供与标准 Linux 内核高度一致的驱动注册与绑定模型。

---

## 🚀 核心架构与特性

### 1. PCIe ECAM 主桥驱动与总线遍历
- **ECAM (Enhanced Configuration Access Mechanism)**：支持标准 MMIO 方式访问 PCIe 扩展配置空间（4KB/设备）；
- **自适应基地址探测**：自动识别 QEMU virt AArch64 的高位 ECAM 空间（`0x4010000000`, 256MB）与低位空间（`0x3F000000`, 16MB）；
- **层级总线遍历**：支持枚举 Bus 0..255、Device 0..31、Function 0..7（MFD 多功能设备自动探测）；
- **动态 32-bit MMIO 分配器**：自动探测设备 BAR 大小与类型（I/O 或 Memory 32/64-bit），按自然对齐分配独立 MMIO 物理窗口（`0x10000000` ~ `0x3EFFFFFF`）。

### 2. ARM GICv3 ITS (Interrupt Translation Service) MSI/MSI-X 引擎
- **纯硬件 ITS 控制器驱动**：驱动 ARM GICv3 核心 ITS 硬件模块（基地址 `0x08080000`）；
- **命令队列环形缓冲区 (Command Queue Ring Buffer)**：实现 64KB 对齐的命令队列，支持原子提交与 SYNC 屏障同步；
- **核心 ITS 命令生成**：
  - `GITS_CMD_MAPD`：映射 PCIe 设备 BDF (Requester ID) 到 ITS 设备表；
  - `GITS_CMD_MAPC`：映射中断目标集合到指定目标 CPU Redistributor；
  - `GITS_CMD_MAPTI`：将 PCIe 设备的 Event ID 转换为特定的物理 LPI (Locality-specific Peripheral Interrupt)；
  - `GITS_CMD_INVALL`：刷新 Redistributor 的 LPI 缓存；
- **LPI 内存表管理**：动态配置 LPI 全局属性表（Priority 0xA0）与每核 LPI 挂起表（Pending Table）；
- **优雅回退机制**：当设备或平台不支持 MSI 时，自动透明回退至传统 PCI INTx 共享引脚中断（SPI 35..38）。

### 3. Linux 风格驱动模型 (`struct pci_driver`)
提供标准 Linux 驱动注册结构：
```c
static const struct pci_device_id sdhci_pci_tbl[] = {
    { PCI_VENDOR_ID_REDHAT, PCI_DEVICE_ID_REDHAT_SDHCI, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0 },
    { PCI_ANY_ID, PCI_ANY_ID, PCI_ANY_ID, PCI_ANY_ID, (PCI_CLASS_SYSTEM_SDHCI << 8), 0xFFFF00, 0 },
    { 0, 0, 0, 0, 0, 0, 0 }
};

static struct pci_driver sdhci_pci_driver = {
    .name = "sdhci-pci",
    .id_table = sdhci_pci_tbl,
    .probe = sdhci_pci_probe,
    .remove = sdhci_pci_remove,
};

int sdhci_pci_register(void)
{
    return pci_register_driver(&sdhci_pci_driver);
}
```

### 4. CherryShell 交互式诊断命令
内建命令行诊断工具：
- `pci_ls`：列出当前总线上所有已枚举的 PCI 设备、Vendor/Device ID、类别、BAR 分配基地址、中断模式（MSI/ITS 或 INTx）；
- `its_info`：查看当前 ARM GICv3 ITS 控制器状态、CBASER 指针、已分配 LPI 向量统计。

---

## 📂 目录结构

```text
components/cherry_pci/
├── SConscript               # 静态库独立编译规则
├── README.md                # 本技术规范文档
├── include/
│   ├── cherry_pci.h         # 核心公共 API 与数据结构
│   ├── cherry_its.h         # ARM GICv3 ITS 寄存器、命令与 LPI 管理接口
│   ├── pci_regs.h           # PCIe 配置空间标准寄存器与能力偏移
│   └── pci_ids.h            # 常用 PCIe Vendor 与 Device ID 宏
├── src/
│   ├── cherry_pci.c         # PCIe 子系统初始化统一入口
│   ├── pci_ecam.c           # ECAM 根复合体读写与基地址自适应探测
│   ├── pci_bus.c            # PCIe 总线扫描、BAR 探测与资源分配
│   ├── pci_driver.c         # Linux 风格驱动注册与自动 Probe 框架
│   ├── pci_its.c            # ARM GICv3 ITS 驱动（命令队列与 LPI 配置）
│   ├── pci_msi.c            # MSI/MSI-X 向量绑定与 INTx 回退
│   └── pci_cmd.c            # CherryShell 调试诊断命令 (pci_ls, its_info)
└── tests/
    ├── pci_test.h           # 自动化测试接口声明
    └── pci_test.c           # 全功能验收测试用例与探针
```

---

## 🛠️ API 规范与调用流程

### 1. 子系统初始化
```c
#include "cherry_pci.h"

int cherry_pci_init(void);
```
该函数依次完成：
1. 探测并初始化 GICv3 ITS 控制器与 LPI 内存表；
2. 探测 PCIe ECAM 根复合体地址；
3. 执行整总线设备扫描并配置 BAR 物理内存映射；
4. 注册 CherryShell 调试命令。

### 2. 设备操作与使能
```c
/* 激活设备配置空间中的 I/O 和 Memory 响应 */
int pci_enable_device(struct pci_dev *dev);

/* 开启 PCI 总线主控 (Bus Mastering) */
void pci_set_master(struct pci_dev *dev);

/* 获取已映射的 BAR MMIO 虚拟/物理基地址 */
void *pci_iomap(struct pci_dev *dev, int bar, unsigned long maxlen);

/* 申请 MSI 中断向量（优先使用 GICv3 ITS，无 MSI 时透明降级为 INTx） */
int pci_enable_msi(struct pci_dev *dev, isr_fn_t handler, void *arg);
```

---

## 🧪 自动化测试与验收标准

执行单元与系统验收测试：
```bash
python3 -m SCons test
```
系统启动时将打印自动化验证探针：
```text
[    0.162929] [6:cpu0:sched-mon:60111] cherry_pci: [pci-verify] ecam=pass bus_scan=pass dev_found=pass bar_alloc=pass its_msi=pass all=pass
```
各项指标说明：
- `ecam=pass`：ECAM 根复合体能够正确读取 Root Complex Vendor/Device ID；
- `bus_scan=pass`：总线扫描算法完成全总线遍历且未溢出设备表；
- `dev_found=pass`：正确发现至少一个 PCIe 端点设备；
- `bar_alloc=pass`：所有有效的 Memory BAR 均已分配不冲突的 MMIO 地址；
- `its_msi=pass`：GICv3 ITS 控制器就绪，LPI/INTx 分发通路健全；
- `all=pass`：子系统整体状态全绿。
