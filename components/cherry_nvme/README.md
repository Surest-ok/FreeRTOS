# CherryNVMe - NVMe 1.4 Storage Subsystem

`cherry_nvme` 是为 CherryOS / FreeRTOS 深度定制的高性能 NVMe 1.4+ 固态存储子系统。它基于 Linux 风格的 PCIe 驱动模型，与 `cherry_pci` 和 `cherry_vfs` 解耦集成，提供规范的块设备抽象与极低的 I/O 延迟。

---

## 架构特性

- **NVMe 1.4+ 标准兼容**：
  - 规范解析 Controller Capabilities (`CAP`)、Version (`VS`)、Configuration (`CC`) 与 Status (`CSTS`)；
  - 动态计算 Doorbell Stride (`dstrd`) 并映射 SQ/CQ 门铃寄存器；
  - 管理 Admin Queue (SQ/CQ) 与独立的 I/O Queue (SQ/CQ)。
- **硬件与总线解耦**：
  - 通过 `struct pci_driver` 与 `cherry_pci` 绑定，基于 Class Code `0x010802` 自动发现并绑定 PCIe NVMe 控制器；
  - 自动映射 64 位 Non-prefetchable BAR0 MMIO 空间。
- **高性能 I/O 引擎**：
  - Admin 命令集：`Identify Controller`、`Create I/O CQ`、`Create I/O SQ`、`Identify Namespace`；
  - NVM I/O 命令集：`Read`、`Write`、`Flush`；
  - 环形队列配合 Phase Tag 完成位轮询检测，结合内存屏障 (`dmb sy`) 确保无竞争。
- **CherryVFS 块设备桥接**：
  - 注册 `/dev/nvme0n1` 与别名 `/dev/nvme0`；
  - 对接 `struct vfs_bdev_ops`，支持单块与多块扇区读写与同步操作。
- **CherryShell 交互命令**：
  - `nvme_info`：打印控制器型号、序列号、固件版本、命名空间信息；
  - `nvme_read <lba> [count]`：读取并十六进制打印 NVMe 扇区数据；
  - `nvme_write <lba> <string>`：写入字符串数据至指定 LBA。
- **自动化验收测试**：
  - 内置 8 项全覆盖自动化验证，输出标准小写 `[nvme-verify]` 标记。
