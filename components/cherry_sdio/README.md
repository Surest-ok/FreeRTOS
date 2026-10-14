# CherrySDIO - 现代化 SD / SDIO / MMC / eMMC 驱动子系统与 CherryVFS 块设备桥接

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Storage: SD/SDHC/eMMC](https://img.shields.io/badge/Storage-SD%20%2F%20SDHC%20%2F%20eMMC-green.svg)]()
[![Controller: SDHCI](https://img.shields.io/badge/Controller-Standard%20SDHCI%20v2%2B-blue.svg)]()
[![VFS: Block Device](https://img.shields.io/badge/VFS-/dev/mmcblk0%20Bridge-orange.svg)]()

`CherrySDIO` 是面向嵌入式与实时系统的高性能、全模块化**通用 SD / SDIO / MMC / eMMC 存储协议栈与主机控制器子系统**。它内建标准 SDHCI (Secure Digital Host Controller Interface) 控制器驱动、PCIe 扩展总线自适配驱动、严格遵循 SD Physical Layer Specification 2.0+ 与 eMMC 规范的状态机引擎，并无缝桥接到 `CherryVFS` 虚拟文件系统提供标准 POSIX 块设备 `/dev/mmcblk0`。

---

## 🚀 核心架构与特性

### 1. 标准 SDHCI 主机控制器驱动
- **通用寄存器抽象**：完全符合 SDHCI 规范（偏移 `0x00` ~ `0x58`），驱动 Software Reset、Clock Control、Power Control、Present State、Interrupt Status 等核心硬件寄存器；
- **自适应分频时钟控制**：基于控制器 Capabilities 寄存器获取基频（如 52MHz），动态支持 400kHz 卡识别时钟与 25MHz 高速数据传输时钟切换；
- **同步与中断并发兼容模型**：支持纯轮询 PIO、硬件中断 ISR 及多核并发访问，杜绝中断信号与轮询等待间的状态擦除竞态；
- **高性能 FIFO 数据传输**：实现 512 字节标准扇区的单块 (`CMD17`/`CMD24`) 与多块流式读写。

### 2. SD / eMMC 协议卡识别状态机
- **Idle 态初始化**：发送 `CMD0` (GO_IDLE_STATE) 复位卡；
- **接口条件与版本探测**：发送 `CMD8` (SEND_IF_COND, 参数 `0x1AA`)，精准区分 SD 1.x、SD 2.0+ 高容量卡以及 MMC/eMMC 设备；
- **OCR 电压协商**：
  - SD 卡：循环发送 `CMD55` (APP_CMD) + `ACMD41` (SD_SEND_OP_COND, 支持 HCS 高容量标示)；
  - eMMC 设备：循环发送 `CMD1` (SEND_OP_COND, `0x40FF8000`)；
- **CID / CSD 信息解析**：
  - `CMD2` (ALL_SEND_CID)：提取 128 位厂商标识、产品名称（Product Name）与芯片序列号；
  - `CMD3` (SET_RELATIVE_ADDR)：分配/获取 16 位相对卡地址 (RCA)；
  - `CMD9` (SEND_CSD)：精确解析 CSD 2.0 / 1.0，计算物理扇区总数与容量；
- **传输态激活**：发送 `CMD7` (SELECT_CARD) 与 `CMD16` (SET_BLOCKLEN = 512)，完成高速传输配置。

### 3. PCIe SDHCI 驱动绑定 (`sdhci-pci`)
- 自动挂载至 PCIe 驱动框架：匹配 Vendor/Device `0x1B36:0x0007` (Red Hat PCIe SDHCI) 及 Class `0x0805`；
- 动态获取 BAR0 物理地址并映射 MMIO；
- 优先申请 ARM GICv3 ITS MSI 向量，支持无缝降级至 PCI INTx (SPI 37)。

### 4. CherryVFS POSIX 块设备桥接 (`/dev/mmcblk0`)
- 自动在 CherryVFS 中注册标准块设备节点：
  - 主设备节点：`/dev/mmcblk0`
  - 兼容别名：`/dev/sd0`
- 实现标准 `struct vfs_bdev_ops`：
  - `read_blocks(bdev, buf, lba, num_blocks)`
  - `write_blocks(bdev, buf, lba, num_blocks)`
- 用户应用及文件系统（如 FAT32/LittleFS/ext2）可直接使用标准 POSIX API（`open`, `read`, `write`, `lseek`, `close`）进行扇区级访问。

### 5. CherryShell 交互式调试命令
- `sd_info`：打印当前插入卡类型（SDSC/SDHC/eMMC）、RCA、序列号、产品名、容量与扇区数；
- `sd_read <lba> [count]`：读取指定扇区并以 16 进制 Hex Dump 格式呈现；
- `sd_write <lba> <string>`：向指定扇区写入文本数据，用于持久化测试。

---

## 📂 目录结构

```text
components/cherry_sdio/
├── SConscript               # 静态库独立编译规则
├── README.md                # 本技术规范文档
├── include/
│   ├── cherry_sdio.h        # 核心公共 API 与数据结构
│   ├── sdhci.h              # SDHCI 标准寄存器与控制器句柄定义
│   └── sd_protocol.h        # SD/MMC 命令宏、OCR、CID/CSD 结构定义
├── src/
│   ├── cherry_sdio.c        # 子系统整体初始化入口
│   ├── sdhci.c              # SDHCI 主机控制器核心驱动（时钟、命令、FIFO）
│   ├── sd_card.c            # SD/eMMC 卡识别与初始化状态机
│   ├── sdhci_pci.c          # PCIe SDHCI 驱动适配与中断注册
│   ├── sd_block.c           # CherryVFS 块设备桥接 (/dev/mmcblk0)
│   └── sd_cmd.c             # CherryShell 诊断命令 (sd_info, sd_read, sd_write)
└── tests/
    ├── sdio_test.h          # 自动化测试接口声明
    └── sdio_test.c          # 全功能验收测试用例与探针
```

---

## 🛠️ API 规范与使用示例

### 1. 块设备 POSIX 读写示例
```c
#include "vfs_posix.h"

void demo_sd_rw(void)
{
    int fd = open("/dev/mmcblk0", O_RDWR, 0666);
    if (fd < 0) {
        return;
    }

    uint8_t write_sector[512] = "Hello CherrySDIO!";
    lseek(fd, 100 * 512, SEEK_SET);  // 定位到第 100 扇区
    write(fd, write_sector, 512);

    uint8_t read_sector[512] = {0};
    lseek(fd, 100 * 512, SEEK_SET);
    read(fd, read_sector, 512);

    close(fd);
}
```

### 2. 扇区级底层访问 API
```c
#include "cherry_sdio.h"

struct sdhci_host *host = sdio_get_primary_host();
if (host) {
    uint8_t buf[512];
    sdio_read_block(host, 0, buf);
    sdio_write_block(host, 0, buf);
}
```

---

## 🧪 自动化测试与验收标准

执行单元与系统验收测试：
```bash
python3 -m SCons test
```
系统启动时将打印自动化验证探针：
```text
[    0.165234] [6:cpu0:sched-mon:60111] cherry_sdio: detected sd specification v2.0+ card
[    0.165607] [6:cpu0:sched-mon:60111] cherry_sdio: ocr=0x80ffff00 high_capacity=0
[    0.165811] [6:cpu0:sched-mon:60111] cherry_sdio: cid product='' serial=0x51454d55
[    0.165951] [6:cpu0:sched-mon:60111] cherry_sdio: assigned rca=0xbcb6
[    0.166097] [6:cpu0:sched-mon:60111] cherry_sdio: csd parsed: capacity=64 mb, sectors=131072, blksize=512
[    0.166261] [6:cpu0:sched-mon:60111] cherry_sdio: card fully configured and in transfer state
[    0.166604] [6:cpu0:sched-mon:60111] cherry_sdio: registered block device /dev/mmcblk0 (131072 sectors, 512b/sector)
[    0.171520] [6:cpu0:sched-mon:60111] cherry_sdio: [sdio-verify] sdhci_init=pass card_detect=pass ocr=pass cid=pass csd=pass block_read=pass block_write=pass vfs_bdev=pass all=pass
```
各项指标说明：
- `sdhci_init=pass`：SDHCI 控制器成功复位、时钟建立并使能；
- `card_detect=pass`：通过 Present State 确认介质卡插入；
- `ocr=pass`：`CMD8` + `CMD55`/`ACMD41` 或 `CMD1` 协商就绪；
- `cid=pass`：`CMD2` 成功读取并解析卡识别寄存器；
- `csd=pass`：`CMD9` 成功读取并计算出有效扇区容量；
- `block_read=pass`：扇区读取与之前写入的数据校验 100% 匹配；
- `block_write=pass`：扇区写入指令正常应答；
- `vfs_bdev=pass`：通过 `/dev/mmcblk0` POSIX VFS 接口读写校验一致；
- `all=pass`：子系统全套链路验证全通过。
