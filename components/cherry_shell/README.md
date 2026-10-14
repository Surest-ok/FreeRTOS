# CherryShell - VxWorks 风格动态 C 表达式命令行与符号引擎

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![VxWorks: C Function Shell](https://img.shields.io/badge/Shell-VxWorks%20Style%20C%20Call-green.svg)]()
[![Symbol: addr2sym](https://img.shields.io/badge/Symbol-addr2sym%20%2B%20Offset-blue.svg)]()
[![OSAL: CherryKAL](https://img.shields.io/badge/OSAL-CherryKAL%20Decoupled-orange.svg)]()

`CherryShell` 是专为嵌入式与实时系统设计的**轻量级、交互式、全动态**的操作系统命令行外壳与符号解析引擎。它深度致敬经典工业级实时操作系统 **VxWorks WIND Shell (C Interpreter)** 的核心特性：
1. **C 函数即命令（Functions as Commands）**：可直接在终端中以标准 C 语法调用任意内核/驱动函数并传递参数，实时打印函数返回值；
2. **符号与地址双向映射（addr2sym / sym2addr）**：支持运行时根据函数指针、异常 PC 指针反查最近符号名称及偏移（如 `vfs_open + 0x18`）；
3. **极简编译期导出与动态注册**：支持通过 GCC 段属性宏（`.cherry_shell_sym`）零侵入导出，亦支持运行时动态符号注册；
4. **完全基于 CherryKAL 抽象**：与底层具体 RTOS（FreeRTOS 等）零耦合，仅依赖 `cherry_kal.h` 与 `cherry_vfs.h`。

---

## 🚀 核心特性

### 1. VxWorks 风格 C 函数直接调用
无论函数是否显式注册为传统 Shell 命令，只要其地址在符号表中，用户均可直接调用：
```bash
cherry> test_add(15, 27)
value = 42 = 0x2a

cherry> test_sum8(1, 2, 3, 4, 5, 6, 7, 8)
value = 36 = 0x24

cherry> test_str_len("Hello CherryShell")
value = 17 = 0x11
```
亦支持简洁命令行语法（无括号风格）：
```bash
cherry> test_add 100 200
value = 300 = 0x12c
```

#### AAPCS64 动态调用原理
在 ARMv8-A (AArch64) 架构下，函数前 8 个标量/指针参数严格通过 `x0` ~ `x7` 寄存器传递，标量返回值通过 `x0` 返回。`CherryShell` 利用此 ABI 特性，支持解析：
- 十进制正负整数（如 `42`, `-10`）
- 十六进制地址与数值（如 `0x40001000`）
- 双引号/单引号字符串字面量（如 `"hello world"`）
- 嵌套符号参数：若参数为已知符号（如 `s_buffer`），自动解析为其内存地址传入！

### 2. 地址到符号名称解析（`addr2sym`）
当内核发生死锁、中断故障、Data Abort 异常或需要调试堆栈回溯（Call Stack）时，`CherryShell` 提供即时符号反查能力：
```bash
cherry> addr2sym 0x0000000040003058
0x0000000040003058: test_add + 0x18

cherry> addr2sym 0x0000000040000000
0x0000000040000000: _start
```
API 接口供内核异常处理机制与日志模块直接调用：
```c
int cherry_shell_addr_to_sym(uintptr_t addr, char *name_buf, size_t buf_len, uintptr_t *out_offset);
const struct cherry_shell_symbol *cherry_shell_find_symbol_by_addr(uintptr_t addr, uintptr_t *out_offset);
```

### 3. 编译期静态导出（Zero Code-Change Export）
在任意 C 模块或驱动中，通过宏即可将命令、函数或全局变量导出到 `.cherry_shell_sym` 段：
```c
/* 导出标准 C 函数（自动支持参数解析与返回值打印） */
CHERRY_SHELL_EXPORT_SYM(gpio_set, bsp_gpio_set, "Set GPIO level: gpio_set(pin, val)");

/* 导出标准 Shell 命令（接收 int argc, char **argv） */
CHERRY_SHELL_EXPORT_CMD(ls, cmd_ls, "List files in directory");

/* 导出全局变量（在终端中输入变量名直接输出其地址与当前值） */
CHERRY_SHELL_EXPORT_VAR(g_system_ticks, g_ticks, "Global system tick counter");
```

### 4. 运行期动态符号注册（Runtime Dynamic Registration）
```c
cherry_shell_register_symbol("kmalloc", (uintptr_t)kmalloc, CHERRY_SHELL_SYM_FUNC, "Allocate memory from heap: kmalloc(size, flags)");
cherry_shell_register_symbol("kfree", (uintptr_t)kfree, CHERRY_SHELL_SYM_FUNC, "Free heap memory: kfree(ptr)");
cherry_shell_register_symbol("ktime_get_ms", (uintptr_t)ktime_get_ms, CHERRY_SHELL_SYM_FUNC, "Get uptime in ms: ktime_get_ms()");
```

### 5. 标准 VFS 字符设备终端重定向（VFS `/dev/console` Terminal I/O）
`CherryShell` 交互式终端与底层的物理串口驱动完全解耦，通过标准 `CherryVFS` 字符设备接口运行：
- Shell 启动时，自动通过 POSIX `open("/dev/console", O_RDWR)`（或 `/dev/ttyS0`）打开串口字符设备；
- 所有交互式控制台输出统一通过 `write(tty_fd, ...)` 写入字符设备；
- 所有按键输入、行编辑与换行捕获均通过 `read(tty_fd, ...)` 读取；
- Shell 线程生命周期由 Linux 风格 `kthread_run()` 与 `kthread_bind()` 管理。

---

## 📁 内置指令集

| 指令 | 语法 | 功能描述 |
|---|---|---|
| `help` | `help` | 显示所有已导出的命令、C 函数及其说明 |
| `syms` | `syms [filter]` | 列出当前系统符号表（地址、类型、名称），支持过滤 |
| `addr2sym`| `addr2sym <hex_addr>` | 查询地址对应的最近符号及偏移量 |
| `ls` | `ls [path]` | 遍历并显示 CherryVFS 目录结构（DIR, FILE, CHR, BLK, SOCK） |
| `cat` | `cat <path>` | 通过 CherryVFS 读取并显示文件内容 |
| `mkdir` | `mkdir <path>` | 创建目录 |
| `touch` | `touch <path>` | 创建空文件 |
| `ps` | `ps` | 显示当前 CPU 核心、系统运行时间与时钟频率 |
| `echo` | `echo <text>` | 回显控制台文本 |

---

## 🧪 自动化测试套件与验证

`CherryShell` 附带严格的自测试套件（`tests/shell_test.c`），在系统启动时自动运行并通过单元测试：
1. **符号表注册与查找验证**：验证静态宏导出与动态注册符号的地址匹配；
2. **`addr2sym` 精准度验证**：验证基地址精确命中（offset=0）与函数体内部偏移命中（offset=0x18）；
3. **VxWorks C 函数调用验证**：验证 2 参数、8 参数（AAPCS64 寄存器传参）、字符串参数的求值与返回值；
4. **VFS 联动指令验证**：通过 Shell 执行 `mkdir`, `touch`, `ls`，全面验证组件联动。

在 QEMU 模拟器上的启动验证输出：
```
<6>cherry_vfs: [vfs-verify] pdev=PASS cdev=PASS bdev=PASS ramfs=PASS sock=PASS all=PASS
<6>cherry_shell: initialized with VxWorks C-function call & addr2sym engine
<6>cherry_shell: starting verification test suite...
<6>cherry_shell: [shell-verify] syms=PASS addr2sym=PASS c_func=PASS sum8=PASS str=PASS cmds=PASS all=PASS
<6>[    0.000000] freertos: [sched-verify] active=PASS idle=PASS affinity=PASS coverage=PASS progress=PASS prio=PASS rr=PASS sem=PASS queue=PASS mutex=PASS spin=PASS cores=2 seen=0x3 missing=0x0 hb0=1 hb1=28
[SCHED-TEST] SUMMARY result=PASS mode=full cores=2 markers=1
```
