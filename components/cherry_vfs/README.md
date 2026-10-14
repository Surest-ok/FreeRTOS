# CherryVFS — 小而美的虚拟文件系统与设备驱动框架

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](../../LICENSE)
[![Platform](https://img.shields.io/badge/Platform-FreeRTOS%20SMP%20%7C%20AArch64-blue.svg)]()
[![Standard](https://img.shields.io/badge/API-POSIX%20%2F%20Linux%20Compatible-green.svg)]()

`CherryVFS` 是专为嵌入式实时操作系统（FreeRTOS 等）量身打造的**现代化、模块化、小而美**的虚拟文件系统（VFS）与统一设备驱动框架（Device Driver Model）。它为极简微内核和实时系统提供了与主流 Linux 高度兼容的 Inode/Dentry 文件系统抽象、统一设备接口（字符设备、块设备、套接字网络设备）以及标准的平台总线（Platform Bus）设备模型。

---

## 1. 核心架构设计

```
┌────────────────────────────────────────────────────────────────────────┐
│                   标准 POSIX / Linux C 接口层                          │
│   open / close / read / write / ioctl / lseek / stat / mkdir / opendir │
│              socket / bind / connect / send / recv                     │
└───────────────────────────────────┬────────────────────────────────────┘
                                    │
┌───────────────────────────────────▼────────────────────────────────────┐
│                    CherryVFS 核心层 (vfs_core)                         │
│   - 文件描述符表 (FD Table): 0(stdin), 1(stdout), 2(stderr), 3..63     │
│   - 路径解析引擎 (Path Lookup): 自动跨越挂载点, 支持 '.' 与 '..'       │
│   - 目录项缓存 (Dentry Cache) & Inode 索引节点生命周期引用计数管理     │
│   - 多核 SMP 并发保护: 递归互斥锁 (Recursive Mutex) 保证线程安全       │
└───────────────┬───────────────────┬───────────────────┬────────────────┘
                │                   │                   │
    ┌───────────▼─────────┐ ┌───────▼───────────┐ ┌─────▼──────────────┐
    │  RamFS 内存文件系统  │ │  DevFS 设备文件系统 │ │ 第三方 FS (扩展接口)│
    │  - 层次化目录树     │ │  挂载于 /dev       │ │  FatFS / LittleFS   │
    │  - 动态内存分配读写  │ │  动态设备 Inode 映射│ │  RomFS              │
    └─────────────────────┘ └───────┬───────────┘ └────────────────────┘
                                    │
┌───────────────────────────────────▼────────────────────────────────────┐
│                  统一设备驱动框架 (Device Driver Model)                 │
│                                                                        │
│   ┌─────────────────────┐ ┌───────────────────┐ ┌───────────────────┐  │
│   │ 字符设备 (cdev)     │ │ 块设备 (bdev)     │ │ 套接字设备 (sock) │  │
│   │  /dev/uart0, console│ │  /dev/ram0 (Ramdisk│ │  /dev/socket      │  │
│   │  /dev/null, zero    │ │  LBA读写, IOCTL   │ │  POSIX BSD Socket │  │
│   └──────────┬──────────┘ └─────────┬─────────┘ └─────────┬─────────┘  │
│              │                      │                     │            │
│   ┌──────────▼──────────────────────▼─────────────────────▼─────────┐  │
│   │           平台总线 (Platform Bus) 设备/驱动匹配子系统            │  │
│   │   - struct platform_device (MMIO 物理地址、中断号 IRQ 等资源)   │  │
│   │   - struct platform_driver (name 匹配, probe 探针, remove 卸载) │  │
│   └─────────────────────────────────────────────────────────────────┘  │
└────────────────────────────────────────────────────────────────────────┘
```

---

## 2. 核心特性矩阵

| 特性维度 | 功能说明 |
|:---|:---|
| **Linux POSIX 兼容** | 完整实现 `open`, `close`, `read`, `write`, `ioctl`, `lseek`, `stat`, `fstat`, `sync`, `truncate`, `mkdir`, `rmdir`, `unlink`, `opendir`, `readdir`, `closedir`。 |
| **标准 Inode 抽象** | 包含清晰的 `struct vfs_inode`、`struct vfs_dentry`、`struct vfs_file`、`struct vfs_superblock`，遵循主流 Unix/Linux VFS 设计范式。 |
| **字符设备 (cdev)** | 提供流式读写抽象，内置 `/dev/null`、`/dev/zero`、`/dev/uart0` 与 `/dev/console`。 |
| **块设备 (bdev)** | 提供基于 LBA 扇区的块读写与 IOCTL 几何参数查询接口，内置 `/dev/ram0` 内存 Ramdisk 驱动。 |
| **套接字设备 (sockdev)** | 将网络 Socket 端点统一为 Inode 与文件描述符，提供 `socket()`, `bind()`, `connect()`, `listen()`, `accept()`, `send()`, `recv()` 抽象。 |
| **Platform 设备模型** | 提供类似 Linux 内核的 Platform 总线，支持 `platform_device_register()`、`platform_driver_register()`、`platform_get_resource()`、`platform_get_irq()` 自动匹配与探测。 |
| **内置文件系统** | • **DevFS**：动态呈现所有已注册设备节点至 `/dev` 目录，支持 `opendir("/dev")` 动态枚举。<br>• **RamFS**：轻量级内存文件系统，挂载于根目录 `/`，支持多级目录与普通文件动态增删改查。 |
| **SMP 线程安全** | 关键路径与元数据变更受 FreeRTOS 递归互斥锁保护，完美支持 2~32 核 SMP 并发调度。 |

---

## 3. 目录与文件布局

```text
components/cherry_vfs/
├── SConscript              # SCons 模块编译配置 (输出 libcherry_vfs.a)
├── include/                # 公共 API 与结构体头文件
│   ├── cherry_vfs.h        # 统一顶层头文件
│   ├── vfs_types.h         # POSIX 数据类型、文件掩码与错误码定义
│   ├── vfs_inode.h         # Inode, Dentry, File, Superblock 结构体
│   ├── vfs_dev.h           # cdev / bdev / sockdev 统一设备抽象定义
│   ├── vfs_driver.h        # 驱动模型基类与 Bus 总线抽象
│   ├── vfs_platform.h      # Platform 平台设备与驱动框架
│   ├── vfs_core.h          # VFS 核心管理与路径解析接口
│   └── vfs_posix.h         # POSIX 标准文件与套接字接口声明
├── src/                    # 核心实现源码
│   ├── vfs_core.c          # Inode/Dentry 分配、挂载表与路径查找引擎
│   ├── vfs_posix.c         # POSIX 系统调用包装器
│   ├── vfs_dev.c           # 设备全局注册、查找与枚举
│   ├── vfs_driver.c        # 总线注册与设备驱动自动匹配绑定 (Probe)
│   ├── vfs_platform.c      # 平台总线与资源查询 (IORESOURCE_MEM/IRQ)
│   ├── fs_devfs.c          # /dev 设备文件系统实现
│   ├── fs_ramfs.c          # / 内存层级文件系统实现
│   ├── dev_null_zero.c     # /dev/null 与 /dev/zero 伪字符设备
│   ├── dev_ramdisk.c       # /dev/ram0 块设备 (32KB 内存磁盘)
│   ├── dev_socket.c        # 套接字文件操作桥接与测试驱动
│   ├── dev_pl011.c         # PL011 UART 硬件平台驱动 (输出至 /dev/console)
│   └── cherry_vfs.c        # 框架一键自举与挂载初始化
└── tests/                  # 验收验证测试套件
    ├── vfs_test.h
    └── vfs_test.c          # 覆盖 pdev, cdev, bdev, ramfs, sock 全项测试
```

---

## 4. API 使用范例

### 4.1 初始化与自举
```c
#include "cherry_vfs.h"

int main(void)
{
    /* 初始化 CherryVFS: 挂载 / (ramfs), /dev (devfs), 并注册内置设备 */
    cherry_vfs_init();
    return 0;
}
```

### 4.2 平台设备与驱动注册 (Platform Bus)
```c
static const struct platform_resource uart_res[] = {
    { .flags = IORESOURCE_MEM, .start = 0x09000000, .end = 0x09000FFF, .name = "uart_mmio" },
    { .flags = IORESOURCE_IRQ, .start = 33, .end = 33, .name = "uart_irq" },
};

static struct platform_device uart_pdev = {
    .name = "pl011_uart",
    .resources = uart_res,
    .num_resources = 2,
};

static int uart_probe(struct platform_device *pdev)
{
    const struct platform_resource *mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
    int irq = platform_get_irq(pdev, 0);
    // 初始化外设并注册 cdev...
    return 0;
}

static struct platform_driver uart_pdrv = {
    .name = "pl011_uart",
    .probe = uart_probe,
};

// 注册驱动与设备（总线会自动进行名称匹配并回调 probe）
platform_driver_register(&uart_pdrv);
platform_device_register(&uart_pdev);
```

### 4.3 字符设备读写 (`/dev/console` 与 `/dev/null`)
```c
int fd = open("/dev/console", O_WRONLY);
write(fd, "Hello via CherryVFS cdev!\n", 26);
close(fd);
```

### 4.4 块设备读写与 IOCTL (`/dev/ram0`)
```c
int fd = open("/dev/ram0", O_RDWR);
uint32_t blk_size = 0;
ioctl(fd, BLOCK_IOCTL_GET_SECTOR_SIZE, (unsigned long)&blk_size); // 512

uint8_t buffer[512] = { 0x55 };
write(fd, buffer, 512);     // 写入第 0 扇区
lseek(fd, 0, SEEK_SET);     // 寻道复位
read(fd, buffer, 512);      // 回读校验
close(fd);
```

### 4.5 常规目录与文件操作 (RamFS)
```c
mkdir("/data", 0755);

int fd = open("/data/record.log", O_CREAT | O_RDWR, 0644);
write(fd, "system started\n", 15);
lseek(fd, 0, SEEK_SET);

char buf[32];
read(fd, buf, sizeof(buf));
close(fd);

// 目录遍历
DIR *dirp = opendir("/data");
struct dirent *entry;
while ((entry = readdir(dirp)) != NULL) {
    uart_printf("Found: %s (type=%d)\n", entry->d_name, entry->d_type);
}
closedir(dirp);
```

---

## 5. 自动化验收断言
 
测试套件内置于 `components/cherry_vfs/tests/vfs_test.c`，在 QEMU 启动阶段自动运行并打印标准化断言探针：

```text
<6>cherry_vfs: ==> Starting CherryVFS Acceptance Test Suite...
<6>cherry_vfs: [/dev/console] POSIX write through cdev successful
<6>cherry_vfs: [vfs-verify] pdev=PASS cdev=PASS bdev=PASS ramfs=PASS sock=PASS all=PASS
```

---

## 6. 大规模文件操作自动化压力测试（VFS Stress Test Suite）

针对嵌入式文件系统易发的内存泄漏、深层递归栈溢出、Dentry 悬空与多核并发竞争，`CherryVFS` 提供了专属的大规模压力测试套件（`components/cherry_vfs/tests/vfs_stress_test.c`）：

```c
int vfs_run_stress_tests(int file_count);
```

### 压力测试阶段覆盖：
1. **深层目录树遍历（Deep Hierarchy）**：
   递归创建 `/stress/lvl1/lvl2/lvl3/lvl4` 极深目录并在叶子节点创建校验文件，验证路径解析深度与 `stat`、`opendir`、`readdir`。
2. **海量批量文件操作（Massive Batch Files）**：
   单批次连续创建 50~100 个文件，写入独立 Magic 校验数据，完整读取并进行字节级一致性校验，校验 `stat` 尺寸后依次 `unlink`，最后 `rmdir` 检验目录完全清理。
3. **大文件跨块 Seek 寻道与截断（Large Seek & Ftruncate）**：
   连续写入 8KB 以上跨块文件数据，`lseek(SEEK_SET)` 随机跳转至中间块写入标记并回读验证，`lseek(SEEK_END)` 追加尾部数据，并使用 `ftruncate()` 将大文件尺寸下调收缩至 4096 字节，通过 `fstat()` 验证元数据准确性。
4. **文件移动与原子重命名（Atomic Rename）**：
   调用 `rename(src, dst)` 将源文件重命名至目标路径，验证源文件路径已被完全注销与 Dentry 回收（`open(src)` 返回 `-ENOENT`），同时验证目标路径数据完整性。
5. **多核 SMP 高并发读写竞争（Concurrent Multi-Core File Stress）**：
   使用 Linux 风格 `kthread_run()` 与 `kthread_bind()` 分别在 Core 0 与 Core 1 派生并发 Worker 线程，两核并发执行 50 次文件创建、写入、读取、比对与注销循环，通过标准 `struct semaphore` 进行两核任务汇合同步。

### 自动化测试探针输出：
```text
<6>[vfs-stress] starting large-scale file operation stress tests (batch_files=50)...
<6>[vfs-stress] hierarchy=PASS batch_files=PASS large_seek_trunc=PASS rename=PASS concurrent_smp=PASS all=PASS
```
