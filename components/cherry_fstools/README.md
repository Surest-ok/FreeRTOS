# CherryFSTools - Disk Partitioning, Mount Management & Core File Utilities

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](../../LICENSE)
[![Standards: Linux MBR / POSIX](https://img.shields.io/badge/Standard-Linux%20MBR%20%2F%20POSIX-blue.svg)]()
[![Platform: FreeRTOS SMP / AArch64](https://img.shields.io/badge/Platform-FreeRTOS%20SMP%20%2F%20AArch64-green.svg)]()

`CherryFSTools` 是专为嵌入式与实时多核操作系统（RTOS）设计的系统级磁盘与文件操作工具集，提供标准 **MBR 磁盘分区管理（fdisk）**、**子块设备动态虚拟化（`/dev/mmcblk0p1`）**、**文件系统挂载与卸载（mount / umount）** 以及核心 **Linux 风格文件实用工具集（ls, cat, cp, mv, rm, mkdir, echo, mkfs, fsck）**。

---

## 1. 核心功能与架构

### 1.1 MBR 磁盘分区引擎（fdisk）
- **标准 MBR 扇区解析**：解析 LBA 0 处的 512 字节主引导记录，校验 `0xAA55` 启动特征字。
- **4 主分区管理**：支持查看、新建主分区，标准分区类型码（`0x83` Linux Native, `0x0B`/`0x0C` FAT32, `0x07` NTFS/exFAT）。
- **分区对齐**：支持标准 LBA 扇区对齐（如 LBA 2048 起始对齐）。

### 1.2 分区子块设备虚拟化（Partition Sub-Block Device）
- **动态设备注册**：扫描 MBR 并在 CherryVFS 块设备层自动注册命名为 `/dev/<parent>p<part_no>` 的虚拟块设备（如 `/dev/mmcblk0p1`）。
- **LBA 地址重定向与边界防护**：透明将分区块操作映射为物理设备上的偏置操作：
  $$\text{LBA}_{\text{parent}} = \text{LBA}_{\text{start}} + \text{LBA}_{\text{part}}$$
  严格校验分区边界，超出总扇区数即时拦截，保障相邻分区数据安全。

### 1.3 挂载与卸载引擎（mount / umount）
- 支持挂载物理块设备或分区设备到指定目录：
  `mount /dev/mmcblk0p1 /mnt/part1 ext4`
- 卸载已挂载的文件系统并同步落盘：
  `umount /mnt/part1`

### 1.4 Linux 风格核心文件实用工具集
- **`ls [path]`**：列出目录内容，输出类型（`[DIR]` / `[FILE]`）、Inode 编号与文件名。
- **`cat <file>`**：读取并显示文件内容。
- **`cp <src> <dst>`**：跨路径、跨文件系统安全复制文件。
- **`mv <src> <dst>`**：安全移动或重命名文件。
- **`rm <path>`**：删除指定文件（自动调用 `unlink`）或空目录（自动调用 `rmdir`）。
- **`mkdir <path>`** 与 **`mkdior <path>`**：创建目录（内置 `mkdior` 别名兼容）。
- **`echo <text> [> >> file]`**：终端文本回显，原生支持 `>` 覆盖重定向与 `>>` 追加重定向。
- **`mkfs <dev> [vol_name]`**：格式化块设备为 Ext4 文件系统。
- **`fsck <dev> [-y]`**：执行文件系统一致性检查与修复。

---

## 2. 命令行与 CherryShell 指令集

在串口终端中，`CherryFSTools` 提供直观便捷的操作指令：

```bash
# 1. 查看磁盘与分区情况
$ fdisk /dev/mmcblk0

# 2. 挂载 ext4 分区到 /mnt/part1
$ mount /dev/mmcblk0p1 /mnt/part1 ext4

# 3. 写入文件与追加重定向
$ echo "Hello CherryOS" > /mnt/part1/test.txt
$ echo "Second line appended" >> /mnt/part1/test.txt

# 4. 查看文件内容
$ cat /mnt/part1/test.txt

# 5. 文件复制与重命名
$ cp /mnt/part1/test.txt /mnt/part1/copy.txt
$ mv /mnt/part1/copy.txt /mnt/part1/renamed.txt

# 6. 查看目录结构
$ ls /mnt/part1

# 7. 创建与删除目录
$ mkdir /mnt/part1/sub_dir
$ rm /mnt/part1/sub_dir
$ rm /mnt/part1/renamed.txt

# 8. 卸载与一致性检测
$ umount /mnt/part1
$ fsck /dev/mmcblk0p1
```

---

## 3. C API 参考

所有工具功能均提供标准 C 语言可重入 API：

```c
#include "cherry_fstools.h"

/* 分区操作 */
int fstools_fdisk(const char *dev_path);
int fstools_part_create(const char *dev_path, int part_no, uint32_t start_lba, uint32_t sectors, uint8_t type);

/* 挂载 / 卸载 */
int fstools_mount(const char *src, const char *target, const char *fstype);
int fstools_umount(const char *target);

/* 文件操作 */
int fstools_ls(const char *path);
int fstools_cat(const char *path);
int fstools_cp(const char *src, const char *dst);
int fstools_mv(const char *src, const char *dst);
int fstools_rm(const char *path);
int fstools_mkdir(const char *path);
int fstools_echo(const char *str, const char *redir_file, bool append);
```

---

## 4. 自动化验收测试

在 QEMU virt 启动时，`cherry_fstools_run_tests()` 执行端到端分区、注册、格式化、挂载、文件操作、卸载与校验自检：

```text
[    0.378482] cherry_fstools: ==> starting cherryfstools acceptance test suite...
[    0.382067] cherry_fstools: partition 1 created on 'mmcblk0' (start=2048, sectors=65536, type=0x83)
[    0.383279] cherry_fstools: registered partition device /dev/mmcblk0p1 (start=2048, count=65536, type=0x83)
[    0.385166] cherry_ext4: formatting '/dev/mmcblk0p1' with ext4 (size=32 mb, blocks=32768, blksize=1024)...
[    0.464982] cherry_ext4: mkfs.ext4 completed successfully on '/dev/mmcblk0p1' (vol='part1_fs')
[    0.469211] cherry_ext4: mounted '/dev/mmcblk0p1' on '/mnt/part1' (vol='part1_fs', blksize=1024)
[    0.564520] cherry_ext4: unmounted '/mnt/part1'
[    0.570077] cherry_ext4: fsck pass - volume 'part1_fs' is clean and consistent
[    0.570997] cherry_fstools: [fstools-verify] fdisk=pass part_bdev=pass mkfs=pass mount=pass echo_cat=pass cp_mv=pass ls_rm=pass umount=pass all=pass
```
