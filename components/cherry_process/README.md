# CherryProcess - 轻量化进程模型与 POSIX 兼容 Syscall 子系统

`cherry_process` 是专为 CherryOS（基于 FreeRTOS SMP AArch64 平台）设计的轻量化进程抽象模型与 POSIX 系统调用层，同时提供完整的 **BusyBox 多呼叫二进制实用工具集** 与 **ash/sh 兼容命令行 Shell**。

---

## 核心特性

1. **轻量化进程控制块 (PCB)**：
   - 独立的进程标识符 (`pid`、`ppid`) 与生命周期状态机 (`READY`、`RUNNING`、`SLEEPING`、`ZOMBIE`、`DEAD`)。
   - 每个进程私有的文件描述符表 (`fd_table[32]`)，映射全局 CherryVFS 句柄，支持标准的 `stdin` (0)、`stdout` (1)、`stderr` (2) 继承与重定向。
   - 进程级工作目录 (`cwd`) 与命令行参数管理 (`argc`、`argv`、`envp`)。
   - `waitpid()` 进程回收与同步原语，支持 `WNOHANG` 非阻塞轮询。

2. **POSIX 兼容 System Call 体系**：
   - 遵循 Linux ARM64 体系结构标准系统调用编号 (`__NR_*`) 与 `cherry_syscall()` 分发器。
   - 支持核心系统调用：`sys_open`, `sys_openat`, `sys_close`, `sys_read`, `sys_write`, `sys_lseek`, `sys_stat`, `sys_fstat`, `sys_ioctl`, `sys_pipe2`, `sys_dup`, `sys_dup2`, `sys_mkdir`, `sys_unlink`, `sys_getcwd`, `sys_chdir`, `sys_getpid`, `sys_getppid`, `sys_exit`, `sys_wait4`, `sys_kill`, `sys_uname`, `sys_nanosleep`, `sys_clock_gettime`, `sys_brk`。

3. **内核级管道 IPC (`cherry_pipe`)**：
   - 4KB 环形缓冲区单向数据流通道，无缝接入 CherryVFS `vfs_file_operations`。
   - 支持多读者/多写者计数、空管道自动阻塞唤醒、写者全关返回 EOF、读者全关报错 EPIPE 等标准语义。

4. **BusyBox 进程包装与 Applet 引擎**：
   - 完整的多呼叫二进制架构，每个 Applet 均可在独立的进程上下文中并发执行。
   - 内置 Applet 矩阵：
     - **Shell & 进程管理**：`sh` (ash), `ps`, `kill`, `sleep`, `true`, `false`。
     - **文件与目录操作**：`ls`, `cat`, `echo`, `cp`, `mv`, `rm`, `mkdir`, `rmdir`, `pwd`, `touch`, `hexdump`。
     - **文本分析工具**：`head`, `tail`, `wc`, `grep`。
     - **系统诊断工具**：`uname`, `df`, `free`, `dmesg`, `date`。
   - **管道与重定向 Shell (`sh`)**：
     - 支持管道串联执行（如 `echo hello | cat | wc -c`）。
     - 支持标准输出/输入重定向 (`> file`, `>> file`, `< file`)。
     - 支持内置命令 `cd`, `pwd`, `exit` 以及后台执行。

---

## 8 点自动化验收测试套件

| 验证项 | 验证内容 | 成功标志 |
| :--- | :--- | :--- |
| **`spawn`** | 进程创建、PID 分配、FreeRTOS Task 关联 | `spawn=pass` |
| **`wait`** | 进程退出码传递、父进程 waitpid 阻塞回收与僵尸状态清理 | `wait=pass` |
| **`fd`** | 进程私有 FD 表隔离、标准输入输出继承与重定向 | `fd=pass` |
| **`pipe`** | 管道 `sys_pipe2` 创建、跨读写端环形缓冲流式传输 | `pipe=pass` |
| **`syscall`** | POSIX 系统调用分发器 (`getpid`, `uname`, `getcwd`) 正确性 | `syscall=pass` |
| **`applets`** | BusyBox 基础工具 (`echo`, `cat`, `ls`, `mkdir`) 进程化运行 | `applets=pass` |
| **`sh`** | BusyBox Shell 管道与多进程串联 (`cmd1 \| cmd2`) 验证 | `sh=pass` |
| **`smp_ps`** | SMP 多核并发进程调度与 `ps` 进程快照输出 | `smp_ps=pass` |

测试输出格式：
```text
cherry_process: [process-verify] spawn=pass wait=pass fd=pass pipe=pass syscall=pass applets=pass sh=pass smp_ps=pass all=pass
```
