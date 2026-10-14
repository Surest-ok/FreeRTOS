# CherryAHCI - Serial ATA AHCI 1.3+ Storage Subsystem

`cherry_ahci` 是为 CherryOS / FreeRTOS 设计的高效 Serial ATA AHCI 1.3+ 控制器驱动与存储子系统。它实现了 Linux 风格的 PCIe 设备驱动解耦架构，自动接管 AHCI HBA 控制器并注册规范的 CherryVFS 块设备。

---

## 架构特性

- **AHCI 1.3+ 标准协议栈**：
  - 支持 Generic Host Control (GHC) 寄存器空间与端口重映射；
  - 自动启用 AHCI 模式 (`GHC.AE = 1`) 并扫描端口位图 (`PI`)；
  - 动态检测端口连接状态 (`PxSSTS.DET == 3`) 与设备签名 (`PxSIG == 0x00000101`)。
- **端口内存重基址 (Rebase Engine)**：
  - 1KB 对齐的 Command List (32 个 Command Header)；
  - 256 字节对齐的 Received FIS 缓冲区；
  - 128 字节对齐的 Command Table，内嵌 PRDT (Physical Region Descriptor Table) 描述符。
- **ATA 协议与 DMA 命令引擎**：
  - `ATA IDENTIFY DEVICE (0xEC)`：解析设备型号、序列号、固件版本与 48 位 LBA 容量；
  - `ATA READ DMA EXT (0x25)`：高效扇区读取；
  - `ATA WRITE DMA EXT (0x35)`：高效扇区写入；
  - `ATA FLUSH CACHE EXT (0xEA)`：数据同步回写。
- **CherryVFS 块设备桥接**：
  - 注册 `/dev/sda` 与别名 `/dev/sata0`；
  - 对接 `struct vfs_bdev_ops`，支持标准块设备 open/close/read_blocks/write_blocks/sync/ioctl。
- **CherryShell 交互命令**：
  - `ahci_info`：打印 AHCI 控制器参数与 SATA 硬盘状态；
  - `ahci_read <lba> [count]`：读取并十六进制打印 SATA 扇区数据；
  - `ahci_write <lba> <string>`：写入字符串数据至指定 LBA。
- **自动化验收测试**：
  - 8 项全覆盖自动化验证，输出标准小写 `[ahci-verify]` 标记。
