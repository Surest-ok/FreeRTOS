# CherryEXT4 - Linux-Compatible Ext4 Filesystem & JBD2 Journaling Subsystem

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](../../LICENSE)
[![Standards: Linux Ext4](https://img.shields.io/badge/Standard-Linux%20Ext4%20%2F%20JBD2-blue.svg)]()
[![Platform: FreeRTOS SMP / AArch64](https://img.shields.io/badge/Platform-FreeRTOS%20SMP%20%2F%20AArch64-green.svg)]()

`CherryEXT4` 是专为嵌入式与实时多核操作系统（RTOS）设计的纯 C 实现、Linux 兼容的 **Ext4（Fourth Extended Filesystem）文件系统与 JBD2（Journaling Block Device 2）事务日志引擎**。

组件严格遵循 Linux 风格内核命名与规范，底层仅依赖 `CherryKAL` OSAL 抽象层与 `CherryVFS` 块设备与虚拟文件系统驱动框架，实现跨平台、高可靠的原子文件系统事务、数据读写、格式化与一致性恢复。

---

## 1. 核心架构与功能特性

### 1.1 标准 Ext4 磁盘格式实现
- **超级块（Superblock）**：标准 Ext4 Magic `0xEF53`，动态版本（`EXT4_DYNAMIC_REV`），支持块大小 1024 / 2048 / 4096 字节自适应，记录全局空闲块数、空闲 Inode 数与卷标（Volume Name）。
- **块组描述符（Group Descriptors）**：管理 Block Bitmap、Inode Bitmap、Inode Table 起始块与空闲计数。
- **区段树（Extents Tree）**：采用标准 Ext4 Extent 格式（Header Magic `0xF30A`），突破传统 12 级直接寻址限制，支持单 extent 连续块映射（最高 32768 块连续跨度），大幅提升大文件连续 I/O 吞吐。
- **目录项（Directory Entries）**：支持标准 Ext4 Dir Entry 2 结构（含 `file_type`、动态 `rec_len`、4 字节对齐）。自动维护 `.` 与 `..` 自引用项，支持条目拆分与删除合并（Coalescing）。

### 1.2 JBD2 事务日志引擎（Atomic Transactions & Recovery）
- **日志超级块**：JBD2 Magic `0xC03B3998`，支持独立 Journal Inode（Inode 8）环形日志区。
- **原子事务模型（ACID）**：
  - `jbd2_trans_start()`：开启原子事务上下文，分配事务句柄。
  - `jbd2_trans_get_write_access()`：登记事务修改块并写入事务缓冲区。
  - `jbd2_trans_commit()`：原子提交事务，依次写入 Descriptor 块（带 Magic 校验与块标签）、脏数据块、Commit 块。
  - **检查点与恢复（Checkpoint & Replay）**：支持开机或挂载前扫描 JBD2 环形日志区，针对未完成 Checkpoint 的已提交事务执行原子重放（`jbd2_replay`），确保掉电零元数据损坏。

### 1.3 核心文件与目录操作
- **文件操作**：`create`, `open`, `read`, `write`（事务保障）, `seek`, `truncate`, `close`, `stat`。
- **目录操作**：`lookup`, `mkdir`, `rmdir`（递归空校验）, `unlink`。
- **空间分配**：动态块位图扫描与快速第一适配（First-Fit）分配器、Inode 位图分配器。

### 1.4 工具链集成
- **`ext4_mkfs` (`mkfs.ext4`)**：快速全盘/分区格式化，自动生成超级块、GDT、位图、根目录（Inode 2）与干净的 JBD2 Journal 超级块（Inode 8）。
- **`ext4_fsck` (`fsck.ext4`)**：深度一致性检查器，验证 Superblock Magic、位图计数、根目录可达性与 JBD2 日志回放。

### 1.5 CherryVFS 驱动与 CherryShell 导出
- 注册为标准 `"ext4"` 文件系统类型驱动（`vfs_register_filesystem`），挂载后直接通过标准 POSIX API（`open`, `write`, `read`, `mkdir`）透明访问。
- 导出 `mkfs_ext4`, `fsck_ext4` C 符号以及 `mkfs`, `fsck` 命令行工具到 CherryShell。

---

## 2. 磁盘布局（Ext4 On-Disk Layout）

```text
+----------------+----------------+----------------+----------------+----------------+----------------+
| Boot Record    | Superblock     | Group Descs    | Block Bitmap   | Inode Bitmap   | Inode Table    |
| (1024 bytes)   | (Offset 1024)  | (GDT, 64B ea)  | (1 Block)      | (1 Block)      | (128B Inodes)  |
+----------------+----------------+----------------+----------------+----------------+----------------+
                 | Magic: 0xEF53  | Blk/Ino BM ptr | 0=Free, 1=Used | 0=Free, 1=Used | Inode 2: /     |
                 | Blk Size: 1024 | Inode Tab ptr  |                |                | Inode 8: JBD2  |
+----------------+----------------+----------------+----------------+----------------+----------------+
| Data Blocks / Extent Allocation Area             | JBD2 Journal Ring Buffer (Inode 8 Area)          |
| (Directory blocks, File extents, User data)      | Header, Descriptor, Data Logs, Commit blocks    |
+--------------------------------------------------+--------------------------------------------------+
```

---

## 3. C API 参考

### 3.1 格式化与修复
```c
#include "cherry_ext4.h"

/* 格式化块设备为 Ext4 文件系统 */
int ext4_mkfs(const char *dev_path, const char *volume_name);

/* 运行一致性检查与修复 */
int ext4_fsck(const char *dev_path, bool repair);
```

### 3.2 挂载与 POSIX 操作
```c
#include "cherry_vfs.h"
#include "vfs_posix.h"

/* 挂载 ext4 分区 */
mkdir("/mnt/ext4", 0755);
mount("/dev/mmcblk0p1", "/mnt/ext4", "ext4", 0, NULL);

/* 读写文件 (自动由 JBD2 事务引擎保护) */
int fd = open("/mnt/ext4/demo.txt", O_CREAT | O_RDWR, 0666);
write(fd, "hello ext4", 10);
close(fd);

/* 卸载 */
umount("/mnt/ext4");
```

---

## 4. 自动化验收测试

在 QEMU virt AArch64 环境下启动自检，验收探针 `[ext4-verify]` 验证全流程：

```text
[    0.210557] cherry_ext4: initializing ext4 filesystem & jbd2 journaling engine...
[    0.212208] cherry_ext4: subsystem initialized successfully
[    0.213178] cherry_ext4: ==> starting cherryext4 acceptance test suite...
[    0.215122] cherry_ext4: formatting '/dev/mmcblk0' with ext4 (size=64 mb, blocks=65536, blksize=1024)...
[    0.292241] cherry_ext4: mkfs.ext4 completed successfully on '/dev/mmcblk0' (vol='sd_ext4')
[    0.292540] cherry_ext4: starting fsck on '/dev/mmcblk0' (repair=1)...
[    0.300416] cherry_ext4: jbd2 journal active (start_block=134, maxlen=1024, seq=1, clean=yes)
[    0.303187] cherry_ext4: fsck pass - volume 'sd_ext4' is clean and consistent
[    0.308203] cherry_ext4: mounted '/dev/mmcblk0' on '/mnt/ext4' (vol='sd_ext4', blksize=1024)
[    0.371872] cherry_ext4: unmounted '/mnt/ext4'
[    0.372066] cherry_ext4: starting fsck on '/dev/mmcblk0' (repair=0)...
[    0.374577] cherry_ext4: jbd2 journal active (start_block=134, maxlen=1024, seq=1, clean=yes)
[    0.376460] cherry_ext4: fsck pass - volume 'sd_ext4' is clean and consistent
[    0.376756] cherry_ext4: [ext4-verify] mkfs=pass fsck=pass mount=pass file_io=pass dir_ops=pass journal=pass umount=pass all=pass
```
